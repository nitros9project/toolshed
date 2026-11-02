/********************************************************************
 * libdecbdisk.c - Sectors of a Disk BASIC image
 *
 * A slot is a sector's place on a disk of 18 sectors per track: track * 18 +
 * sector - 1.  In a raw image it is also the sector's place in the file, past
 * any drive or HDB-DOS offset.  Other images hold one drive only, on side 0.
 ********************************************************************/
#include <string.h>

#include "decbpath.h"

#define DECB_SECTORS_PER_TRACK	18
#define DECB_MAX_SECTOR_SIZE	1024


/* Bad data is still the data on the disk, so it is used with a warning. */
error_code _decb_slot_read(decb_path_id path, long long slot, void *buffer)
{
	u_char sector[DECB_MAX_SECTOR_SIZE];
	u_int size = sizeof(sector);
	u_int cylinder = (u_int)(slot / DECB_SECTORS_PER_TRACK);
	u_int number = (u_int)(slot % DECB_SECTORS_PER_TRACK) + 1;
	error_code ec = _diskimage_read_sector(path->disk, cylinder, 0, number, sector, &size);

	if (ec == EOS_CRC)
	{
		fprintf(stderr, "Warning: bad CRC in track %u sector %u of %s\n", cylinder, number, path->imgfile);
		ec = 0;
	}
	if (ec == EOS_IA || (ec == 0 && size != 256))
	{
		return EOS_SECT;
	}
	if (ec == 0)
	{
		memcpy(buffer, sector, 256);
	}

	return ec;
}


error_code _decb_slot_write(decb_path_id path, long long slot, const void *buffer)
{
	error_code ec = _diskimage_write_sector(path->disk, (u_int)(slot / DECB_SECTORS_PER_TRACK), 0,
						(u_int)(slot % DECB_SECTORS_PER_TRACK) + 1, buffer, 256);

	return ec == EOS_IA ? EOS_SECT : ec;
}


/*
 * _decb_open_image()
 *
 * Open the image file of a path.  A drive number or offset only means
 * something in a raw image, which can hold many drives.
 */
error_code _decb_open_image(decb_path_id path)
{
	diskimage_geometry geometry;
	error_code ec = _diskimage_open(&path->disk, path->imgfile, FAM_READ | (path->mode & FAM_WRITE));

	if (ec != 0)
	{
		return ec == EOS_WT ? ec : EOS_BPNAM;
	}

	ec = _diskimage_query_geometry(path->disk, &geometry);
	if (ec == 0 && path->has_drive && geometry.format != DISKIMAGE_RAW)
	{
		ec = EOS_BPNAM;
	}
	if (ec != 0)
	{
		_diskimage_close(path->disk);
		path->disk = NULL;
		return ec;
	}

	path->disk_offset = 161280 * path->drive;
	path->disk_offset += path->hdbdos_offset;

	return 0;
}


/* Read the FAT.  Only what needs it fails if it is missing. */
void _decb_load_fat(decb_path_id path)
{
	path->fat_ec = _decb_gs_sector(path, 17, 2, (char *) path->FAT);
	if (path->fat_ec != 0)
	{
		memset(path->FAT, 0xFF, sizeof(path->FAT));
	}
	memcpy(path->saved_FAT, path->FAT, sizeof(path->FAT));
}


/* Write the FAT back if it changed, and close the image file. */
error_code _decb_close_image(decb_path_id path)
{
	error_code ec = 0;
	error_code close_ec;

	if ((path->mode & FAM_WRITE) && path->fat_ec == 0 && memcmp(path->FAT, path->saved_FAT, sizeof(path->FAT)) != 0)
	{
		ec = _decb_ss_sector(path, 17, 2, (char *) path->FAT);
	}

	close_ec = _diskimage_close(path->disk);

	return ec != 0 ? ec : close_ec;
}


/* Read from a raw path.  Stops before a missing sector, and only fails if
   nothing could be read. */
error_code _decb_raw_read(decb_path_id path, void *buffer, u_int *size)
{
	u_char *out = buffer;
	u_int done = 0;
	error_code ec = 0;

	while (done < *size)
	{
		u_char sector[256];
		u_int offset = path->filepos % 256;
		u_int count = 256 - offset;

		ec = _decb_slot_read(path, path->filepos / 256, sector);
		if (ec != 0)
		{
			break;
		}
		if (count > *size - done)
		{
			count = *size - done;
		}
		memcpy(out + done, sector + offset, count);
		done += count;
		path->filepos += count;
	}

	*size = done;

	return done > 0 ? 0 : ec;
}


/* Write to a raw path.  A raw image grows; the rest of a new sector is $FF. */
error_code _decb_raw_write(decb_path_id path, const void *buffer, u_int *size)
{
	const u_char *in = buffer;
	u_int done = 0;
	error_code ec = 0;

	while (done < *size)
	{
		u_char sector[256];
		u_int offset = path->filepos % 256;
		u_int count = 256 - offset;

		if (count > *size - done)
		{
			count = *size - done;
		}
		if (count < 256)
		{
			ec = _decb_slot_read(path, path->filepos / 256, sector);
			if (ec == EOS_SECT)
			{
				diskimage_geometry geometry;

				_diskimage_query_geometry(path->disk, &geometry);
				if (!geometry.extendable)
				{
					break;
				}
				memset(sector, 0xFF, sizeof(sector));
				ec = 0;
			}
			if (ec != 0)
			{
				break;
			}
		}
		memcpy(sector + offset, in + done, count);
		ec = _decb_slot_write(path, path->filepos / 256, sector);
		if (ec != 0)
		{
			break;
		}
		done += count;
		path->filepos += count;
	}

	*size = done;

	return ec;
}


/* Read a sector of drive 0.  Damaged data can still be recognized. */
static int identify_sector(diskimage_id disk, u_int sector, u_char *buffer)
{
	u_int size = 256;
	error_code ec = _diskimage_read_sector(disk, 17, 0, sector, buffer, &size);

	return (ec == 0 || ec == EOS_CRC) && size == 256;
}


/* A chain of granules must end, in a value that says how much of its last
   granule is used. */
static int chain_is_valid(const u_char *fat, u_int granules, u_int first)
{
	u_int granule = first;
	u_int count;

	for (count = 0; count <= granules; count++)
	{
		if (granule >= granules)
		{
			return 0;
		}
		if (fat[granule] >= 0xC0)
		{
			return fat[granule] <= 0xC9;
		}
		granule = fat[granule];
	}

	return 0;
}


/* A directory entry's name is printable, and its type bytes are known ones. */
static int entry_is_valid(const decb_dir_entry *entry, const u_char *fat, u_int granules)
{
	int i;

	for (i = 0; i < 8; i++)
	{
		if (entry->filename[i] < 0x20 || entry->filename[i] > 0x7E)
		{
			return 0;
		}
	}
	for (i = 0; i < 3; i++)
	{
		if (entry->file_extension[i] < 0x20 || entry->file_extension[i] > 0x7E)
		{
			return 0;
		}
	}

	return entry->file_type <= 3 && (entry->ascii_flag == 0x00 || entry->ascii_flag == 0xFF) &&
		chain_is_valid(fat, granules, entry->first_granule);
}


/*
 * _decb_identify()
 *
 * Determine if an image could be a Disk BASIC disk.  Nothing marks one, so it
 * must have a FAT and directory on track 17 with nothing wrong in them.  A
 * track of only $FF is an unused track rather than an empty directory.
 */
error_code _decb_identify(const char *pathname, int *found)
{
	diskimage_geometry geometry;
	diskimage_id disk;
	u_char fat[256];
	u_char sector[256];
	u_int granules, i, number;
	int used = 0;
	int free_only = 1;
	int skitzo = 1;
	int all_ff = 1;
	int valid = 1;
	int ended = 0;
	error_code ec;

	*found = 0;

	ec = _diskimage_open(&disk, pathname, FAM_READ);
	if (ec != 0)
	{
		return ec;
	}

	ec = _diskimage_query_geometry(disk, &geometry);
	if (ec != 0 || geometry.cylinders < 18 || !identify_sector(disk, 2, fat))
	{
		_diskimage_close(disk);
		return ec;
	}

	/* Every track but the directory's has two granules. */
	granules = (geometry.cylinders - 1) * 2;
	if (granules > 156)
	{
		granules = 156;
	}

	for (i = 0; i < sizeof(fat); i++)
	{
		all_ff = all_ff && fat[i] == 0xFF;
	}
	for (i = 0; i < granules; i++)
	{
		if (!(fat[i] < granules || (fat[i] >= 0xC0 && fat[i] <= 0xC9) || fat[i] == 0xFF))
		{
			valid = 0;
		}
		free_only = free_only && fat[i] == 0xFF;
		skitzo = skitzo && fat[i] == (i < 34 ? 0x00 : 0xFF);
	}

	/* A missing directory sector says nothing either way. */
	for (number = 3; number <= 11 && valid && !ended; number++)
	{
		if (!identify_sector(disk, number, sector))
		{
			continue;
		}
		for (i = 0; i < 256 && valid && !ended; i += sizeof(decb_dir_entry))
		{
			const decb_dir_entry *entry = (const decb_dir_entry *) &sector[i];

			if (entry->filename[0] == 0xFF)
			{
				ended = 1;
			}
			else
			{
				used = 1;
				if (entry->filename[0] != 0)
				{
					valid = entry_is_valid(entry, fat, granules);
				}
			}
		}
	}

	/* With no files, nothing can have put anything in the FAT. */
	if (!used && (all_ff || !(free_only || skitzo)))
	{
		valid = 0;
	}

	*found = valid;
	_diskimage_close(disk);

	return 0;
}

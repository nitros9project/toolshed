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

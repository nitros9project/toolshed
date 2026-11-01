/********************************************************************
 * libdiskimageopen.c - Identify, open, close and query disk images
 ********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libdiskimageint.h"


/* Open a file and read what is needed to tell its format.  A header
   shorter than DI_HEADER_PROBE is zero filled; the format checks that
   use the bytes also require a file long enough to hold them. */
static error_code open_file(const char *pathname, const char *fopen_mode, FILE **fd,
			    u_char *header, long long *file_size)
{
	struct stat st;
	size_t got;

	if (stat(pathname, &st) != 0)
	{
		return UnixToCoCoError(errno);
	}
	if (S_ISDIR(st.st_mode))
	{
		return EOS_FNA;
	}

	*fd = fopen(pathname, fopen_mode);
	if (*fd == NULL)
	{
		return UnixToCoCoError(errno);
	}

	memset(header, 0, DI_HEADER_PROBE);
	got = fread(header, 1, DI_HEADER_PROBE, *fd);
	if ((got < DI_HEADER_PROBE && ferror(*fd)) ||
	    fseeko(*fd, 0, SEEK_END) != 0 ||
	    (*file_size = (long long)ftello(*fd)) < 0)
	{
		fclose(*fd);
		*fd = NULL;
		return EOS_READ;
	}

	return 0;
}


/* SDF, DMK and VDK sizes also fit JVC and raw images, so they are tried first. */
static diskimage_format detect_format(const u_char *header, long long file_size, di_jvc_header *jvc)
{
	if (di_sdf_probe(header, file_size))
	{
		return DISKIMAGE_SDF;
	}
	if (di_dmk_probe(header, file_size))
	{
		return DISKIMAGE_DMK;
	}
	if (di_vdk_probe(header, file_size))
	{
		return DISKIMAGE_VDK;
	}
	if (file_size % 256 == 0)
	{
		return DISKIMAGE_RAW;
	}
	if (di_jvc_parse(header, file_size, jvc))
	{
		return DISKIMAGE_JVC;
	}

	return DISKIMAGE_UNKNOWN;
}


error_code _diskimage_identify(const char *pathname, diskimage_format *format)
{
	u_char header[DI_HEADER_PROBE];
	di_jvc_header jvc;
	long long file_size;
	FILE *fd;
	error_code ec;

	if (pathname == NULL || format == NULL)
	{
		return EOS_IA;
	}

	ec = open_file(pathname, "rb", &fd, header, &file_size);
	if (ec != 0)
	{
		return ec;
	}

	*format = detect_format(header, file_size, &jvc);
	fclose(fd);

	return 0;
}


/* Return the sectors on track 0 if LSN0 describes a disk with a short track
   0, or 0 if not.  Only OS-9 disks have one, and a raw image has nowhere else
   to record it.  All of this must agree, so that the first sector of any
   other disk is very unlikely to be taken for it. */
static u_int raw_short_track0(const lsn0_sect *lsn0, long long slots)
{
	int is_osk = memcmp(lsn0->dd_sync, "Cruz", 4) == 0;
	u_char *pd_sct = (u_char *)(is_osk ? lsn0->dd_opt.m68k.pd_sct : lsn0->dd_opt.m6809.pd_sct);
	u_char *pd_t0s = (u_char *)(is_osk ? lsn0->dd_opt.m68k.pd_t0s : lsn0->dd_opt.m6809.pd_t0s);
	u_int total = int3((u_char *)lsn0->dd_tot);
	u_int track0 = int2(pd_t0s);

	if (is_osk && int2((u_char *)lsn0->dd_lsnsize) > 1)
	{
		return 0;
	}
	if (int2((u_char *)lsn0->dd_spt) != DI_RAW_SECTORS_PER_TRACK || int2(pd_sct) != DI_RAW_SECTORS_PER_TRACK)
	{
		return 0;
	}
	if (track0 == 0 || track0 >= DI_RAW_SECTORS_PER_TRACK || total < track0 ||
	    (total - track0) % DI_RAW_SECTORS_PER_TRACK != 0)
	{
		return 0;
	}

	/* The file can be shorter than the disk, but no longer. */
	if (slots > (long long)total + (DI_RAW_SECTORS_PER_TRACK - track0))
	{
		return 0;
	}

	return track0;
}


static error_code load_raw(diskimage_id disk, long long file_size)
{
	long long slots = file_size / DI_RAW_SECTOR_SIZE;
	long long total = slots;

	disk->linear = 1;
	disk->sides = 1;
	disk->sector_size = DI_RAW_SECTOR_SIZE;
	disk->sectors_per_track = DI_RAW_SECTORS_PER_TRACK;
	disk->first_sector = 1;
	disk->raw_slots = slots;
	disk->raw_track0 = DI_RAW_SECTORS_PER_TRACK;

	if (slots > 0)
	{
		u_char lsn0[DI_RAW_SECTOR_SIZE];
		u_int track0;
		error_code ec = di_read_at(disk, 0, lsn0, sizeof(lsn0));

		if (ec != 0)
		{
			return ec;
		}

		track0 = raw_short_track0((const lsn0_sect *)lsn0, slots);
		if (track0 != 0)
		{
			disk->raw_track0 = track0;
			disk->raw_missing = DI_RAW_SECTORS_PER_TRACK - track0;

			/* Slots in the unused part of track 0 are not sectors. */
			if (slots >= DI_RAW_SECTORS_PER_TRACK)
			{
				total = slots - disk->raw_missing;
			}
			else if (slots > track0)
			{
				total = track0;
			}
		}
	}

	disk->total_sectors = (u_int)total;
	disk->stream_size = total * DI_RAW_SECTOR_SIZE;

	return 0;
}


error_code _diskimage_open(diskimage_id *disk, const char *pathname, int mode)
{
	u_char header[DI_HEADER_PROBE];
	di_jvc_header jvc;
	long long file_size;
	diskimage_id new_disk;
	error_code ec;

	if (disk == NULL || pathname == NULL)
	{
		return EOS_IA;
	}
	if ((mode & (FAM_READ | FAM_WRITE)) == 0)
	{
		return EOS_BMODE;
	}

	new_disk = calloc(1, sizeof(*new_disk));
	if (new_disk == NULL)
	{
		return UnixToCoCoError(ENOMEM);
	}
	new_disk->mode = mode;

	ec = open_file(pathname, (mode & FAM_WRITE) ? "r+b" : "rb", &new_disk->fd, header, &file_size);
	if (ec != 0)
	{
		free(new_disk);
		return ec;
	}

	new_disk->format = detect_format(header, file_size, &jvc);
	switch (new_disk->format)
	{
	case DISKIMAGE_SDF:
		ec = di_sdf_load(new_disk, header);
		break;
	case DISKIMAGE_DMK:
		ec = di_dmk_load(new_disk, header);
		break;
	case DISKIMAGE_JVC:
		ec = di_jvc_load(new_disk, &jvc);
		break;
	case DISKIMAGE_VDK:
		di_vdk_load(new_disk, header, file_size);
		break;
	case DISKIMAGE_RAW:
		ec = load_raw(new_disk, file_size);
		break;
	default:
		ec = EOS_WT;
		break;
	}

	if (ec != 0)
	{
		_diskimage_close(new_disk);
		return ec;
	}

	*disk = new_disk;

	return 0;
}


error_code _diskimage_close(diskimage_id disk)
{
	error_code ec = 0;

	if (disk == NULL)
	{
		return EOS_IA;
	}

	while (disk->streams != NULL)
	{
		_diskimage_stream_close(disk->streams);
	}
	if (fclose(disk->fd) != 0)
	{
		ec = EOS_WRITE;
	}
	free(disk->table);
	free(disk);

	return ec;
}


static void table_geometry(diskimage_id disk, diskimage_geometry *geometry)
{
	u_int track_count = 0;
	u_int i;

	geometry->first_sector = disk->total_sectors ? 255 : 0;
	for (i = 0; i < disk->total_sectors; i++)
	{
		const di_sector *entry = &disk->table[i];

		if (i > 0 && (entry->cylinder != entry[-1].cylinder || entry->side != entry[-1].side))
		{
			track_count = 0;
		}
		track_count++;
		if (track_count > geometry->sectors_per_track)
		{
			geometry->sectors_per_track = track_count;
		}
		if (entry->size > geometry->sector_size)
		{
			geometry->sector_size = entry->size;
		}
		if (entry->id < geometry->first_sector)
		{
			geometry->first_sector = entry->id;
		}
		if (entry->cylinder == 0 && entry->side == 0)
		{
			geometry->sectors_track0++;
		}
	}

	geometry->uniform = disk->total_sectors > 0 &&
		disk->total_sectors == disk->cylinders * disk->sides * geometry->sectors_per_track;
	for (i = 0; i < disk->total_sectors && geometry->uniform; i++)
	{
		const di_sector *entry = &disk->table[i];
		const di_sector *model = &disk->table[i % geometry->sectors_per_track];

		geometry->uniform = entry->id == model->id && entry->size == model->size;
	}
}


error_code _diskimage_query_geometry(diskimage_id disk, diskimage_geometry *geometry)
{
	if (disk == NULL || geometry == NULL)
	{
		return EOS_IA;
	}

	memset(geometry, 0, sizeof(*geometry));
	geometry->format = disk->format;
	geometry->cylinders = disk->cylinders;
	geometry->sides = disk->sides;
	geometry->total_sectors = disk->total_sectors;
	geometry->stream_size = disk->stream_size;
	geometry->write_protected = disk->write_protected;
	geometry->extendable = disk->format == DISKIMAGE_RAW;

	if (disk->format == DISKIMAGE_RAW)
	{
		long long slots = disk->total_sectors ? di_linear_slot(disk, disk->total_sectors - 1) + 1 : 0;

		geometry->cylinders = (u_int)((slots + DI_RAW_SECTORS_PER_TRACK - 1) / DI_RAW_SECTORS_PER_TRACK);
		geometry->uniform = disk->raw_missing == 0 && disk->total_sectors % DI_RAW_SECTORS_PER_TRACK == 0;
	}
	else if (disk->linear)
	{
		geometry->uniform = 1;
	}

	if (disk->linear)
	{
		geometry->sectors_per_track = disk->sectors_per_track;
		geometry->sector_size = disk->sector_size;
		geometry->first_sector = disk->first_sector;
		geometry->sectors_track0 = disk->sectors_per_track - disk->raw_missing;
		if (geometry->sectors_track0 > disk->total_sectors)
		{
			geometry->sectors_track0 = disk->total_sectors;
		}
	}
	else
	{
		table_geometry(disk, geometry);
	}

	return 0;
}


error_code _diskimage_query_sector(diskimage_id disk, u_int index, diskimage_sector_info *info)
{
	if (disk == NULL || info == NULL)
	{
		return EOS_IA;
	}
	if (index >= disk->total_sectors)
	{
		return EOS_SECT;
	}

	if (disk->linear)
	{
		u_int spt = disk->sectors_per_track;
		long long slot = di_linear_slot(disk, index);
		u_int track = (u_int)(slot / spt);

		info->cylinder = track / disk->sides;
		info->side = track % disk->sides;
		info->sector = disk->first_sector + (u_int)(slot % spt);
		info->size = disk->sector_size;
		info->flags = 0;
		info->stream_offset = (long long)index * disk->sector_size;
	}
	else
	{
		const di_sector *entry = &disk->table[index];

		info->cylinder = entry->cylinder;
		info->side = entry->side;
		info->sector = entry->id;
		info->size = entry->size;
		info->flags = entry->flags & DI_FLAG_PUBLIC;
		info->stream_offset = entry->stream_off;
	}

	return 0;
}

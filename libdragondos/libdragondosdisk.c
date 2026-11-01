/********************************************************************
 * libdragondosdisk.c - Sectors, geometry and the directory track
 ********************************************************************/
#include <string.h>

#include "libdragondosint.h"


static void lsn_to_address(dragondos_path_id path, u_int lsn, u_int *cylinder, u_int *side, u_int *sector)
{
	if (path->linear)
	{
		*cylinder = lsn / DRAGONDOS_TRACK_SECTORS;
		*side = 0;
		*sector = lsn % DRAGONDOS_TRACK_SECTORS + 1;
		return;
	}

	*cylinder = lsn / path->sectors_per_track;
	*sector = lsn % path->sectors_per_track;
	*side = *sector / DRAGONDOS_TRACK_SECTORS;
	*sector = *sector % DRAGONDOS_TRACK_SECTORS + 1;
}

/* Read one sector.  EOS_CRC still delivers the data as recorded. */
static error_code read_address(dragondos_path_id path, u_int cylinder, u_int side, u_int sector, u_char *buffer)
{
	u_int size = DRAGONDOS_SECTOR_SIZE;
	error_code ec = _diskimage_read_sector(path->disk, cylinder, side, sector, buffer, &size);

	if (ec == EOS_IA || (ec == 0 && size != DRAGONDOS_SECTOR_SIZE))
	{
		return EOS_WT;
	}
	return ec;
}

error_code _dd_read_lsn(dragondos_path_id path, u_int lsn, u_char *buffer)
{
	u_int cylinder, side, sector;

	lsn_to_address(path, lsn, &cylinder, &side, &sector);
	return read_address(path, cylinder, side, sector, buffer);
}

error_code _dd_write_lsn(dragondos_path_id path, u_int lsn, const u_char *buffer)
{
	u_int cylinder, side, sector;

	lsn_to_address(path, lsn, &cylinder, &side, &sector);
	return _diskimage_write_sector(path->disk, cylinder, side, sector, buffer, DRAGONDOS_SECTOR_SIZE);
}

error_code _dd_set_geometry(dragondos_path_id path, const u_char *bitmap_sector)
{
	u_int tracks = bitmap_sector[0xFC];
	u_int sectors_per_track = bitmap_sector[0xFD];

	if ((tracks ^ bitmap_sector[0xFE]) != 0xFF || (sectors_per_track ^ bitmap_sector[0xFF]) != 0xFF ||
	    (sectors_per_track != DRAGONDOS_TRACK_SECTORS && sectors_per_track != DRAGONDOS_TRACK_SECTORS * 2))
	{
		return EOS_WT;
	}

	path->tracks = tracks;
	path->sectors_per_track = sectors_per_track;
	path->total_sectors = tracks * sectors_per_track;
	if (path->total_sectors > DD_MAX_LSN)
	{
		path->total_sectors = DD_MAX_LSN;
	}
	path->directory_lsn = 20 * sectors_per_track;
	return 0;
}

/* Find the bitmap's first sector at a directory track or its copy. */
static error_code try_geometry(dragondos_path_id path, u_int cylinder, u_int sectors_per_track)
{
	u_int cylinders[2];
	error_code first_error = 0;
	int i;

	cylinders[0] = cylinder;
	cylinders[1] = cylinder - DD_BACKUP_TRACKS_BACK * (cylinder / 20);

	for (i = 0; i < 2; i++)
	{
		u_char sector[DRAGONDOS_SECTOR_SIZE];
		error_code ec = read_address(path, cylinders[i], 0, 1, sector);

		if (ec == 0)
		{
			ec = _dd_set_geometry(path, sector);
			if (ec == 0 && path->sectors_per_track != sectors_per_track)
			{
				ec = EOS_WT;
			}
			if (ec == 0)
			{
				return 0;
			}
		}
		if (first_error == 0)
		{
			first_error = ec;
		}
	}
	return first_error;
}

error_code _dd_load_geometry(dragondos_path_id path)
{
	diskimage_geometry geometry;
	error_code ec = _diskimage_query_geometry(path->disk, &geometry);
	error_code raw_ec;

	if (ec != 0)
	{
		return ec;
	}

	if (geometry.format != DISKIMAGE_RAW)
	{
		/* A double sided disk also has its directory on cylinder 20. */
		ec = try_geometry(path, 20, DRAGONDOS_TRACK_SECTORS);
		if (ec != 0)
		{
			ec = try_geometry(path, 20, DRAGONDOS_TRACK_SECTORS * 2);
		}
		return ec;
	}

	/* A raw image has no sides, so a double sided disk's directory track
	   is twice as far in. */
	ec = try_geometry(path, 20, DRAGONDOS_TRACK_SECTORS);
	if (ec == 0)
	{
		return 0;
	}
	raw_ec = try_geometry(path, 40, DRAGONDOS_TRACK_SECTORS * 2);
	if (raw_ec == 0)
	{
		path->linear = 1;
		return 0;
	}
	return ec == EOS_WT ? ec : raw_ec;
}

/* Get a sector of the directory track, from its copy if it must be. */
error_code _dd_track_sector(dragondos_path_id path, u_int index, u_char **sector)
{
	if (!path->track_loaded[index])
	{
		u_int backup = path->directory_lsn - DD_BACKUP_TRACKS_BACK * path->sectors_per_track;
		error_code ec = _dd_read_lsn(path, path->directory_lsn + index, path->track[index]);

		if (ec == EOS_SECT || ec == EOS_CRC)
		{
			error_code backup_ec = _dd_read_lsn(path, backup + index, path->track[index]);

			if (backup_ec == 0)
			{
				ec = 0;
			}
		}
		if (ec != 0)
		{
			return ec;
		}
		path->track_loaded[index] = 1;
	}

	*sector = path->track[index];
	return 0;
}

void _dd_track_dirty(dragondos_path_id path, u_int index)
{
	path->track_dirty[index] = 1;
}

/* Write changed sectors of the directory track, and their copies.  A copy
   the image lacks is only a problem if both are lacking. */
error_code _dd_flush(dragondos_path_id path)
{
	u_int backup = path->directory_lsn - DD_BACKUP_TRACKS_BACK * path->sectors_per_track;
	u_int i;

	for (i = 0; i < DRAGONDOS_TRACK_SECTORS; i++)
	{
		error_code ec, backup_ec;

		if (!path->track_dirty[i])
		{
			continue;
		}

		ec = _dd_write_lsn(path, path->directory_lsn + i, path->track[i]);
		if (ec != 0 && ec != EOS_SECT)
		{
			return ec;
		}
		backup_ec = _dd_write_lsn(path, backup + i, path->track[i]);
		if (backup_ec != 0 && (backup_ec != EOS_SECT || ec == EOS_SECT))
		{
			return backup_ec;
		}
		path->track_dirty[i] = 0;
	}
	return 0;
}

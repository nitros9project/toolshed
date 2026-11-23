/********************************************************************
 * libflexdskinit.c - Make a blank disk, as NEWDISK does
 *
 * Track 0 has the boot sectors, the SIR and the directory, which fills
 * the rest of the track.  Every sector of the other tracks is in the
 * free chain, in order.  Unused bytes are zero.
 ********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cococonv.h"
#include "libflexint.h"


/* A raw image of zeros, written here as libdiskimage only makes raw images
   of 18 sectors per track. */
static error_code create_raw(const char *pathname, u_int sectors)
{
	u_char zero[FLEX_SECTOR_SIZE];
	struct stat st;
	error_code ec = 0;
	FILE *fd;
	u_int i;

	if (stat(pathname, &st) == 0)
	{
		return EOS_FAE;
	}
	fd = fopen(pathname, "wb");
	if (fd == NULL)
	{
		return UnixToCoCoError(errno);
	}

	memset(zero, 0, sizeof(zero));
	for (i = 0; i < sectors && ec == 0; i++)
	{
		if (fwrite(zero, 1, sizeof(zero), fd) != sizeof(zero))
		{
			ec = EOS_WRITE;
		}
	}
	if (fclose(fd) != 0 && ec == 0)
	{
		ec = EOS_WRITE;
	}
	if (ec != 0)
	{
		remove(pathname);
	}
	return ec;
}

static error_code format_sectors(flex_path_id path, u_int track0_sectors, const char *volume_name, u_int volume_number)
{
	u_char sector[FLEX_SECTOR_SIZE];
	u_char name[11];
	u_int track, number;
	error_code ec;

	memset(name, 0, sizeof(name));
	if (volume_name != NULL && volume_name[0] != '\0')
	{
		ec = _fx_parse_name(volume_name, name);
		if (ec != 0)
		{
			return ec;
		}
	}

	memset(path->sir, 0, sizeof(path->sir));
	memcpy(&path->sir[FX_SIR_NAME], name, sizeof(name));
	_fx_put2(&path->sir[FX_SIR_NUMBER], volume_number);
	_fx_today(&path->sir[FX_SIR_DATE]);
	path->sir[FX_SIR_MAX_TRACK] = (u_char)path->max_track;
	path->sir[FX_SIR_MAX_SECTOR] = (u_char)path->max_sector;
	_fx_set_free_chain(path, 1, 1, path->max_track, path->max_sector, _fx_total_sectors(path));
	ec = _fx_flush(path);

	for (number = FLEX_DIR_SECTOR; number <= track0_sectors && ec == 0; number++)
	{
		memset(sector, 0, sizeof(sector));
		if (number < track0_sectors)
		{
			sector[1] = (u_char)(number + 1);
		}
		ec = _fx_write_sector(path, 0, number, sector);
	}

	for (track = 1; track <= path->max_track && ec == 0; track++)
	{
		for (number = 1; number <= path->max_sector && ec == 0; number++)
		{
			memset(sector, 0, sizeof(sector));
			if (number < path->max_sector)
			{
				sector[0] = (u_char)track;
				sector[1] = (u_char)(number + 1);
			}
			else if (track < path->max_track)
			{
				sector[0] = (u_char)(track + 1);
				sector[1] = 1;
			}
			ec = _fx_write_sector(path, track, number, sector);
		}
	}

	return ec;
}

error_code _flex_dskinit(const char *pathname, diskimage_format format, u_int tracks, u_int sides,
			 int double_density, int padded, const char *volume_name, u_int volume_number)
{
	u_int per_side = double_density ? FLEX_DD_SECTORS : FLEX_SD_SECTORS;
	u_int track0_sectors = FLEX_SD_SECTORS * sides;
	struct _flex_path_id path;
	u_char name[11];
	error_code ec;

	if (tracks < 2 || tracks > 256 || (sides != 1 && sides != 2) || volume_number > 0xFFFF)
	{
		return EOS_IA;
	}
	if (volume_name != NULL && volume_name[0] != '\0' && _fx_parse_name(volume_name, name) != 0)
	{
		return EOS_BPNAM;
	}

	memset(&path, 0, sizeof(path));
	path.max_track = tracks - 1;
	path.max_sector = per_side * sides;
	path.sides = sides;

	switch (format)
	{
	case DISKIMAGE_RAW:
		path.linear = 1;
		path.track0_slots = padded ? path.max_sector : track0_sectors;
		ec = create_raw(pathname, path.track0_slots + path.max_track * path.max_sector);
		break;

	case DISKIMAGE_JVC:
	case DISKIMAGE_VDK:
		/* These have no different track 0. */
		path.track0_side0 = per_side;
		ec = _diskimage_create(pathname, format, sides, tracks, FLEX_SECTOR_SIZE, per_side, 0, 1, 0);
		break;

	case DISKIMAGE_DMK:
	case DISKIMAGE_SDF:
		path.track0_side0 = FLEX_SD_SECTORS;
		ec = _diskimage_create_ex(pathname, format, sides, tracks, FLEX_SECTOR_SIZE, per_side, FLEX_SD_SECTORS, 1, 0,
					  DISKIMAGE_CREATE_FM_TRACK0 | DISKIMAGE_CREATE_SPAN_SIDES |
					  (double_density ? 0 : DISKIMAGE_CREATE_SINGLE_DENSITY));
		break;

	default:
		return EOS_IA;
	}
	if (ec != 0)
	{
		return ec;
	}

	ec = _diskimage_open(&path.disk, pathname, FAM_READ | FAM_WRITE);
	if (ec == 0)
	{
		ec = format_sectors(&path, track0_sectors, volume_name, volume_number);
		if (_diskimage_close(path.disk) != 0 && ec == 0)
		{
			ec = EOS_WRITE;
		}
	}
	if (ec != 0)
	{
		remove(pathname);
	}
	return ec;
}

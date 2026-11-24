/********************************************************************
 * libflexdisk.c - Sectors, geometry and the SIR
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cococonv.h"
#include "libflexint.h"


u_int _fx_get2(const u_char *p)
{
	return (p[0] << 8) | p[1];
}

void _fx_put2(u_char *p, u_int value)
{
	p[0] = (u_char)(value >> 8);
	p[1] = (u_char)value;
}

/* Whether SOURCE_DATE_EPOCH is set to a time, which *when is set to. */
static int source_date_epoch(time_t *when)
{
	char *value = getenv("SOURCE_DATE_EPOCH");
	char *end;
	long long seconds;

	if (value == NULL || *value == '\0')
	{
		return 0;
	}
	seconds = strtoll(value, &end, 10);
	if (*end != '\0' || seconds < 0)
	{
		return 0;
	}
	*when = (time_t)seconds;
	return 1;
}

void _fx_date(time_t when, u_char *date)
{
	struct tm *parts;

	/* SOURCE_DATE_EPOCH is read in UTC, so the date doesn't depend on the
	   time zone of the machine making the disk. */
	if (source_date_epoch(&when))
	{
		parts = gmtime(&when);
	}
	else
	{
		parts = localtime(&when);
	}
	if (parts == NULL)
	{
		memset(date, 0, 3);
		return;
	}
	date[0] = (u_char)(parts->tm_mon + 1);
	date[1] = (u_char)parts->tm_mday;
	date[2] = (u_char)(parts->tm_year % 100);
}

void _fx_today(u_char *date)
{
	_fx_date(time(NULL), date);
}

int _flex_reproducible_date(flex_file_stat *stat)
{
	time_t when;

	if (!source_date_epoch(&when))
	{
		return 0;
	}
	_flex_stat_date(stat, when);
	return 1;
}

void _flex_stat_date(flex_file_stat *stat, time_t when)
{
	u_char date[3];

	_fx_date(when, date);
	stat->month = date[0];
	stat->day = date[1];
	stat->year = date[2];
}

/* Sectors on side 0 of a track, when the image has sides. */
static u_int side0_sectors(flex_path_id path, u_int track)
{
	if (track == 0)
	{
		return path->track0_side0;
	}
	return path->sides == 2 ? path->max_sector / 2 : path->max_sector;
}

int _fx_valid_address(flex_path_id path, u_int track, u_int sector)
{
	if (sector == 0 || track > path->max_track)
	{
		return 0;
	}
	if (track == 0)
	{
		return path->linear ? sector <= path->track0_slots : sector <= 255;
	}
	return sector <= path->max_sector;
}

u_int _fx_total_sectors(flex_path_id path)
{
	return path->max_track * path->max_sector;
}

/* The position of a sector in a raw image. */
static u_int linear_index(flex_path_id path, u_int track, u_int sector)
{
	if (track == 0)
	{
		return sector - 1;
	}
	return path->track0_slots + (track - 1) * path->max_sector + sector - 1;
}

/* Find where a sector is in the image.  On a second side, a sector can be
   recorded with its FLEX number or numbered again from 1, as JVC and VDK
   images, which record no numbers, have it. */
typedef error_code (*sector_access)(flex_path_id path, u_int cylinder, u_int side, u_int sector, void *buffer);

static error_code access_sector(flex_path_id path, u_int track, u_int sector, void *buffer, sector_access access)
{
	error_code ec;
	u_int first_side;

	if (!_fx_valid_address(path, track, sector))
	{
		return EOS_SECT;
	}

	if (path->linear)
	{
		diskimage_sector_info info;

		ec = _diskimage_query_sector(path->disk, linear_index(path, track, sector), &info);
		if (ec != 0)
		{
			return EOS_SECT;
		}
		return access(path, info.cylinder, info.side, info.sector, buffer);
	}

	first_side = side0_sectors(path, track);
	if (path->sides < 2 || sector <= first_side)
	{
		return access(path, track, 0, sector, buffer);
	}

	ec = access(path, track, 1, sector, buffer);
	if (ec == EOS_SECT)
	{
		ec = access(path, track, 1, sector - first_side, buffer);
	}
	return ec;
}

static error_code read_at(flex_path_id path, u_int cylinder, u_int side, u_int sector, void *buffer)
{
	u_int size = FLEX_SECTOR_SIZE;
	error_code ec = _diskimage_read_sector(path->disk, cylinder, side, sector, buffer, &size);

	if (ec == EOS_IA || (ec == 0 && size != FLEX_SECTOR_SIZE))
	{
		return EOS_WT;
	}
	return ec;
}

static error_code write_at(flex_path_id path, u_int cylinder, u_int side, u_int sector, void *buffer)
{
	return _diskimage_write_sector(path->disk, cylinder, side, sector, buffer, FLEX_SECTOR_SIZE);
}

/* EOS_CRC still delivers the data as recorded. */
error_code _fx_read_sector(flex_path_id path, u_int track, u_int sector, u_char *buffer)
{
	return access_sector(path, track, sector, buffer, read_at);
}

error_code _fx_write_sector(flex_path_id path, u_int track, u_int sector, const u_char *buffer)
{
	return access_sector(path, track, sector, (void *)buffer, write_at);
}

void _fx_decode_sir(const u_char *sector, flex_sir *sir)
{
	memcpy(sir->volume_name, &sector[FX_SIR_NAME], FX_SIR_NAME_SIZE);
	sir->volume_name[FX_SIR_NAME_SIZE] = '\0';
	sir->volume_number = _fx_get2(&sector[FX_SIR_NUMBER]);
	sir->free_track = sector[FX_SIR_FREE];
	sir->free_sector = sector[FX_SIR_FREE + 1];
	sir->last_free_track = sector[FX_SIR_LAST_FREE];
	sir->last_free_sector = sector[FX_SIR_LAST_FREE + 1];
	sir->free_sectors = _fx_get2(&sector[FX_SIR_FREE_COUNT]);
	sir->month = sector[FX_SIR_DATE];
	sir->day = sector[FX_SIR_DATE + 1];
	sir->year = sector[FX_SIR_DATE + 2];
	sir->max_track = sector[FX_SIR_MAX_TRACK];
	sir->max_sector = sector[FX_SIR_MAX_SECTOR];
}

void _fx_set_free_chain(flex_path_id path, u_int first_track, u_int first_sector,
			u_int last_track, u_int last_sector, u_int count)
{
	path->sir[FX_SIR_FREE] = (u_char)first_track;
	path->sir[FX_SIR_FREE + 1] = (u_char)first_sector;
	path->sir[FX_SIR_LAST_FREE] = (u_char)last_track;
	path->sir[FX_SIR_LAST_FREE + 1] = (u_char)last_sector;
	_fx_put2(&path->sir[FX_SIR_FREE_COUNT], count);
	path->sir_dirty = 1;
}

/* Track 0 of a raw image is either single density, with 10 sectors on each
   side, or padded to the size of the others.  Only the image's size tells. */
static u_int raw_track0_slots(u_int max_sector, u_int slots)
{
	u_int single = (max_sector >= 2 * FLEX_SD_SECTORS && max_sector % 2 == 0) ? 2 * FLEX_SD_SECTORS : FLEX_SD_SECTORS;

	if (max_sector <= single || slots % max_sector == 0)
	{
		return max_sector;
	}
	if (slots >= single && (slots - single) % max_sector == 0)
	{
		return single;
	}
	return max_sector;
}

error_code _fx_load_geometry(flex_path_id path)
{
	diskimage_geometry geometry;
	error_code ec = _diskimage_query_geometry(path->disk, &geometry);

	if (ec != 0)
	{
		return ec;
	}

	path->linear = geometry.format == DISKIMAGE_RAW;
	path->sides = geometry.sides;
	path->track0_side0 = geometry.sectors_track0;

	/* Enough to reach the SIR until its geometry is known. */
	path->max_track = 0;
	path->max_sector = FLEX_SIR_SECTOR;
	path->track0_slots = FLEX_SIR_SECTOR;
	if (!path->linear && path->track0_side0 < FLEX_SIR_SECTOR)
	{
		return EOS_WT;
	}

	ec = _fx_read_sector(path, 0, FLEX_SIR_SECTOR, path->sir);
	if (ec == EOS_SECT)
	{
		return EOS_WT;
	}
	if (ec != 0)
	{
		return ec;
	}

	path->max_track = path->sir[FX_SIR_MAX_TRACK];
	path->max_sector = path->sir[FX_SIR_MAX_SECTOR];
	if (path->max_track == 0 || path->max_sector < FLEX_DIR_SECTOR)
	{
		return EOS_WT;
	}
	if (path->linear)
	{
		path->track0_slots = raw_track0_slots(path->max_sector, geometry.total_sectors);
	}
	return 0;
}

error_code _fx_flush(flex_path_id path)
{
	error_code ec;

	if (!path->sir_dirty)
	{
		return 0;
	}
	ec = _fx_write_sector(path, 0, FLEX_SIR_SECTOR, path->sir);
	if (ec == 0)
	{
		path->sir_dirty = 0;
	}
	return ec;
}

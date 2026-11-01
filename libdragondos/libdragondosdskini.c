/********************************************************************
 * libdragondosdskini.c - Make a blank disk, as DSKINI at $C3BC of the ROM
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "libdragondosint.h"

#define DSKINI_INTERLEAVE	2
#define DSKINI_FILL		0xE5
#define DSKINI_EMPTY_ENTRY	0x89


error_code _dragondos_dskini(const char *pathname, diskimage_format format, u_int tracks, u_int sides)
{
	dragondos_path_id path;
	u_int sectors_per_track = DRAGONDOS_TRACK_SECTORS * sides;
	u_int lsn, i;
	error_code ec, close_ec;

	if (tracks <= 20 || (sides != 1 && sides != 2) || tracks * sectors_per_track > DD_MAX_LSN)
	{
		return EOS_IA;
	}

	ec = _diskimage_create(pathname, format, sides, tracks, DRAGONDOS_SECTOR_SIZE, DRAGONDOS_TRACK_SECTORS,
			       0, DSKINI_INTERLEAVE, DSKINI_FILL);
	if (ec != 0)
	{
		return ec;
	}

	path = calloc(1, sizeof(*path));
	if (path == NULL)
	{
		return EOS_OM;
	}

	ec = _diskimage_open(&path->disk, pathname, FAM_READ | FAM_WRITE);
	if (ec != 0)
	{
		free(path);
		return ec;
	}

	/* A raw image has no sides. */
	path->linear = format == DISKIMAGE_RAW && sides == 2;

	path->track[0][0xFC] = (u_char)tracks;
	path->track[0][0xFD] = (u_char)sectors_per_track;
	path->track[0][0xFE] = (u_char)~tracks;
	path->track[0][0xFF] = (u_char)~sectors_per_track;
	ec = _dd_set_geometry(path, path->track[0]);

	for (i = 2; i < DRAGONDOS_TRACK_SECTORS; i++)
	{
		u_int entry;

		for (entry = 0; entry < DD_ENTRIES_PER_SECTOR; entry++)
		{
			path->track[i][entry * DD_ENTRY_SIZE] = DSKINI_EMPTY_ENTRY;
		}
	}
	memset(path->track_loaded, 1, sizeof(path->track_loaded));
	memset(path->track_dirty, 1, sizeof(path->track_dirty));

	for (lsn = 0; ec == 0 && lsn < path->total_sectors; lsn++)
	{
		ec = _dd_release(path, lsn, 1);
	}

	/* The directory track and its copy are in use. */
	for (i = 0; ec == 0 && i < DRAGONDOS_TRACK_SECTORS; i++)
	{
		u_int offset = path->directory_lsn + i;
		u_int backup = path->directory_lsn - DD_BACKUP_TRACKS_BACK * sectors_per_track + i;

		path->track[0][offset / 8] &= (u_char)~(1 << (offset % 8));
		path->track[0][backup / 8] &= (u_char)~(1 << (backup % 8));
	}

	if (ec == 0)
	{
		ec = _dd_flush(path);
	}

	close_ec = _diskimage_close(path->disk);
	free(path);
	return ec != 0 ? ec : close_ec;
}

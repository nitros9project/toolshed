/********************************************************************
 * seek.c - Disk BASIC Seek routine
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "decbpath.h"


error_code _decb_seek(decb_path_id path, int pos, int mode)
{
	error_code ec = 0;


	if (path->israw == 1)
	{
		long long base = 0;

		if (mode == SEEK_CUR)
		{
			base = path->filepos;
		}
		else if (mode == SEEK_END)
		{
			diskimage_geometry geometry;

			ec = _diskimage_query_geometry(path->disk, &geometry);

			if (ec != 0)
			{
				return ec;
			}

			base = (long long) geometry.cylinders * geometry.sides * geometry.sectors_per_track * 256;
		}

		if (base + pos < 0)
		{
			return EOS_SE;
		}

		path->filepos = base + pos;
	}
	else
	{
		switch (mode)
		{
		case SEEK_SET:
			path->filepos = pos;
			break;

		case SEEK_CUR:
			path->filepos = path->filepos + pos;
			break;

		case SEEK_END:
			fprintf(stderr,
				"_decb_seek(): SEEK_END not implemented.\n");
			break;
		}
	}


	return ec;
}



error_code _decb_seekdir(decb_path_id path, int entry, int mode)
{
	error_code ec = 0;

	/* 1. Determine the seek type. */
	switch (mode)
	{
	case SEEK_SET:
		break;

	case SEEK_CUR:
		entry += path->directory_entry_index;
		break;

	case SEEK_END:
		break;
	}

	if (entry < 0 || entry > 72)
	{
		/* 1. Illegal entry number -- return error. */

		ec = EOS_EOF;
	}

	path->directory_entry_index = entry;

	return ec;
}



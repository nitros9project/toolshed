/********************************************************************
 * seek.c - OS-9 Seek routine
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "os9path.h"


error_code _os9_seek(os9_path_id path, int pos, int mode)
{
	error_code ec = 0;


	if (path->israw == 1)
	{
		if (pos != 0 && mode == SEEK_SET)
		{
			if (path->t0s != path->spt)
			{
				fprintf(stderr, "Error: _os9_seek() SET_SEEK not implemented on raw paths with a short track zero.\n");
				exit(-1);
			}
		}

		fseek(path->fd, pos, mode);
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
				"_os9_seek(): SEEK_END not implemented.\n");
			exit(0);
			break;
		}
	}


	return ec;
}


/*
 * _os9_lsn_fseek()
 *
 * Seek to proper LSN accounting for short track zero.
 */

int _os9_lsn_fseek(os9_path_id path, int lsn)
{
	long offset;

	if (lsn >= path->t0s)
	{
		/* Skip past missing sectors */
		lsn += path->spt - path->t0s;
	}

	offset = lsn * path->bps;

	return fseek(path->fd, offset, SEEK_SET);
}

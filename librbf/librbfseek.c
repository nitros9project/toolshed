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

		if (fseek(path->fd, pos, mode) == 0)
		{
			path->filepos = ftell(path->fd);
		}
		else
		{
			ec = -1; /* Return failure code if fseek failed */
		}
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


/*
 * seek_run()
 *
 * Seek to a byte offset past the start of a run of contiguous LSNs. Returns
 * how many of the next 'size' bytes are contiguous in the image file. That is
 * less than 'size' only when the transfer reaches the first LSN of a short
 * track zero's missing sectors.
 */

static u_int seek_run(os9_path_id path, int lsn, u_int offset, u_int size)
{
	u_int cur_lsn = lsn + offset / path->bps;
	u_int sector_offset = offset % path->bps;

	if (cur_lsn < path->t0s)
	{
		u_int room = (path->t0s - cur_lsn) * path->bps - sector_offset;

		if (size > room)
		{
			size = room;
		}
	}

	_os9_lsn_fseek(path, cur_lsn);
	fseek(path->fd, sector_offset, SEEK_CUR);

	return size;
}


/*
 * _os9_lsn_read()
 *
 * Read from a run of contiguous LSNs, starting 'offset' bytes past 'lsn'.
 * Returns the number of bytes read.
 */

u_int _os9_lsn_read(os9_path_id path, int lsn, u_int offset, void *buffer, u_int size)
{
	char *buf_ptr = buffer;
	u_int total = 0;

	while (total < size)
	{
		u_int run = seek_run(path, lsn, offset + total, size - total);
		u_int count = fread(buf_ptr + total, 1, run, path->fd);

		total += count;

		if (count != run)
		{
			break;
		}
	}

	return total;
}


/*
 * _os9_lsn_write()
 *
 * Write to a run of contiguous LSNs, starting 'offset' bytes past 'lsn'.
 * Returns the number of bytes written.
 */

u_int _os9_lsn_write(os9_path_id path, int lsn, u_int offset, const void *buffer, u_int size)
{
	const char *buf_ptr = buffer;
	u_int total = 0;

	while (total < size)
	{
		u_int run = seek_run(path, lsn, offset + total, size - total);
		u_int count = fwrite(buf_ptr + total, 1, run, path->fd);

		total += count;

		if (count != run)
		{
			break;
		}
	}

	return total;
}

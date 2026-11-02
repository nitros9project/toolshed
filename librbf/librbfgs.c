/********************************************************************
 * gs.c - OS-9 GetStatus routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cocotypes.h"
#include "os9path.h"
#include "cococonv.h"


error_code _os9_gs_attr(os9_path_id path, int *perms)
{
	error_code ec = 0;
	fd_stats fdbuf;


	ec = _os9_gs_fd(path, sizeof(fd_stats), &fdbuf);

	if (ec == 0)
	{
		*perms = fdbuf.fd_att;
	}


	return ec;
}



error_code _os9_gs_eof(os9_path_id path)
{
	error_code ec = 0;
	u_int size;


	/* A raw path's size is the disk's; it has no file descriptor. */

	ec = _os9_gs_size(path, &size);

	if (ec == 0)
	{
		if (path->filepos >= size)
		{
			ec = EOS_EOF;
		}
	}


	return ec;
}



error_code _os9_gs_fd(os9_path_id path, int count, fd_stats * fdbuf)
{
	error_code ec = 0;
	int size;


	/* Read the file descriptor sector */

	size = sizeof(fd_stats);

	if (count < size)
	{
		size = count;
	}

	ec = _os9_lsn_get(path, path->pl_fd_lsn, 0, fdbuf, size);


	return ec;
}



error_code _os9_gs_fd_pathlist(char *pathlist, int count, fd_stats * fdbuf)
{
	error_code ec = 0;
	os9_path_id path;

	/* Open a path to the pathlist, for no access since a file desc needs none */

	ec = _os9_open(&path, pathlist, 0);

	if (ec != 0)
	{
		ec = _os9_open(&path, pathlist, FAM_DIR);

		if (ec != 0)
		{
			return ec;
		}
	}


	ec = _os9_gs_fd(path, count, fdbuf);

	_os9_close(path);


	return ec;
}



error_code _os9_gs_size(os9_path_id path, u_int * size)
{
	error_code ec = 0;
	fd_stats fdbuf;


	/* If path is raw, return entire disk as size */

	if (path->israw == 1)
	{
		*size = int3(path->lsn0->dd_tot) * path->bps;

		return 0;
	}

	ec = _os9_gs_fd(path, sizeof(fdbuf), &fdbuf);

	if (ec != 0)
	{
		return ec;
	}

	*size = int4(fdbuf.fd_siz);


	return ec;
}



error_code _os9_gs_pos(os9_path_id path, u_int * pos)
{
	error_code ec = 0;


	*pos = path->filepos;


	return ec;
}

/********************************************************************
 * ss.c - DECB SetStatus routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cocotypes.h"
#include "decbpath.h"
#include "cococonv.h"


error_code _decb_ss_fd(decb_path_id path, decb_file_stat * stat)
{
	error_code ec = 0;
//      decb_dir_entry  de;


	path->dir_entry.file_type = stat->file_type;
	path->dir_entry.ascii_flag = stat->data_type;

	_decb_seekdir(path, path->this_directory_entry_index, SEEK_SET);
	ec = _decb_writedir(path, &path->dir_entry);


	return ec;
}



error_code _decb_ss_attr(decb_path_id path, int perms)
{
	error_code ec = 0;
#if 0
	ec = _os9_gs_fd(path, sizeof(fd_stats), &fdbuf);
	if (ec == 0)
	{
		fdbuf.fd_att = perms;
		ec = _os9_ss_fd(path, sizeof(fd_stats), &fdbuf);
	}
#endif

	return ec;
}



error_code _decb_ss_size(decb_path_id path, int size)
{
	error_code ec = 0;
#if 0
	fd_stats fdbuf;


	/* if path is raw, return entire disk as size */
	if (path->israw == 1)
	{
		return (ec);
	}

	ec = _os9_gs_fd(path, sizeof(fdbuf), &fdbuf);
	if (ec != 0)
	{
		return (ec);
	}

	_int4(size, fdbuf.fd_siz);

	ec = _os9_ss_fd(path, sizeof(fdbuf), &fdbuf);
#endif

	return ec;
}



error_code _decb_ss_sector(decb_path_id path, int track, int sector,
			   char *buffer)
{
	return _decb_slot_write(path, path->disk_offset / 256 + (long long) track * 18 + (sector - 1), buffer);
}



error_code _decb_ss_granule(decb_path_id path, int granule, char *buffer)
{
	error_code ec = 0;
	long long slot = path->disk_offset / 256 + (long long) granule * 9;
	int step = path->hdbdos_offset > 0 ? 2 : 1;	/* skip unused 1/2 of sector */
	int count;


	/* 1. Account for directory (2 granules) */

	if (granule > 33)
	{
		slot += 18;
	}


	/* 2. Write buffer to granule. */

	for (count = 0; count < 9 && ec == 0; count++)
	{
		ec = _decb_slot_write(path, slot + (long long) count * step, buffer + count * 256);
	}


	/* 3. Return status. */

	return ec;
}

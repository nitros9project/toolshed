/********************************************************************
 * gs.c - Disk BASIC GetStat routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "decbpath.h"
#include "cococonv.h"


error_code _decb_gs_fd(decb_path_id path, decb_file_stat * stat)
{
	error_code ec = 0;
	u_int size;


	stat->file_type = path->dir_entry.file_type;
	stat->data_type = path->dir_entry.ascii_flag;

	ec = _decb_gs_size(path, &size);

	if (ec == 0)
	{
		stat->file_size = size;
	}


	return ec;
}



error_code _decb_gs_fd_pathlist(char *pathlist, decb_file_stat * statbuf)
{
	error_code ec = 0;
	decb_path_id path;

	/* Open a path to the pathlist */

	ec = _decb_open(&path, pathlist, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}


	ec = _decb_gs_fd(path, statbuf);

	_decb_close(path);


	return ec;
}



error_code _decb_gs_eof(decb_path_id path)
{
	error_code ec = 0;
	u_int filesize;


	ec = _decb_gs_size(path, &filesize);

	if (ec == 0 && path->filepos >= filesize)
	{
		ec = EOS_EOF;
	}


	return ec;
}



/*
 * _decb_gs_chain()
 *
 * Follow a file's chain of granules in the FAT.  granules gets how many there
 * are, and last the granule whose FAT entry ends the chain.  The FAT has 256
 * entries, so a chain longer than that goes round in a loop, and EOS_WT says
 * the disk is damaged rather than following it forever.
 */
error_code _decb_gs_chain(decb_path_id path, u_int first, u_int *granules, u_int *last)
{
	u_int granule = first & 0xFF;
	u_int count = 1;

	while (path->FAT[granule] < 0xC0)
	{
		if (count >= sizeof(path->FAT))
		{
			return EOS_WT;
		}
		granule = path->FAT[granule];
		count++;
	}

	*granules = count;
	*last = granule;

	return 0;
}


error_code _decb_gs_size(decb_path_id path, u_int * size)
{
	error_code ec = 0;
	int curr_granule;
	int sectors_in_last_granule;

	*size = 0;

	/* A raw path's size is what _decb_read will give of the disk. */

	if (path->israw == 1)
	{
		*size = 35 * (18 * 256);

		return 0;
	}

	if (path->fat_ec != 0)
	{
		return path->fat_ec;
	}


	/* 1. The following code is for DECB paths. */

	{
		u_int granules, last;

		ec = _decb_gs_chain(path, path->dir_entry.first_granule, &granules, &last);
		if (ec != 0)
		{
			return ec;
		}

		curr_granule = last;
		*size = (granules - 1) * 2304;
	}

	sectors_in_last_granule = (path->FAT[curr_granule] & 0x3f) - 1;
	sectors_in_last_granule =
		sectors_in_last_granule < 0 ? 0 : sectors_in_last_granule;

	*size += (256 * sectors_in_last_granule) +
		int2(path->dir_entry.last_sector_size);


	return ec;
}



error_code _decb_gs_size_pathlist(char *pathlist, u_int * size)
{
	error_code ec = 0;
	decb_path_id path;

	/* Open a path to the pathlist */

	ec = _decb_open(&path, pathlist, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}


	ec = _decb_gs_size(path, size);

	_decb_close(path);


	return ec;
}



error_code _decb_gs_pos(decb_path_id path, u_int * pos)
{
	error_code ec = 0;


	*pos = path->filepos;

	return ec;
}



error_code _decb_gs_sector(decb_path_id path, int track, int sector,
			   char *buffer)
{
	return _decb_slot_read(path, path->disk_offset / 256 + (long long) track * 18 + (sector - 1), buffer);
}



error_code _decb_gs_granule(decb_path_id path, int granule, char *buffer)
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


	/* 2. Read granule into buffer. */

	for (count = 0; count < 9 && ec == 0; count++)
	{
		ec = _decb_slot_read(path, slot + (long long) count * step, buffer + count * 256);
	}


	/* 3. Return status. */

	return ec;
}

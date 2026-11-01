/********************************************************************
 * kill.c - Kill Disk BASIC file
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "cocotypes.h"
#include "decbpath.h"
#include "cococonv.h"


error_code _decb_kill(char *pathlist)
{
	error_code ec = 0;
	decb_path_id path;
	int curr_granule;


	/* 1. Open a path. */

	ec = _decb_open(&path, pathlist, FAM_READ | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}


	/* 2. The file's granules are found in the FAT. */

	if (path->fat_ec != 0)
	{
		ec = path->fat_ec;

		_decb_close(path);

		return ec;
	}


	/* 3. Erase the entry by writing a nul as the first byte and write out sector. */

	path->dir_entry.filename[0] = '\0';

	_decb_seekdir(path, path->this_directory_entry_index, SEEK_SET);

	ec = _decb_writedir(path, &path->dir_entry);

	if (ec != 0)
	{
		_decb_close(path);

		return ec;
	}


	/* 4. Clear the granules in the FAT used by this entry. */

	curr_granule = path->dir_entry.first_granule;

	while (path->FAT[curr_granule] < 0xC0)
	{
		int next_granule;


		next_granule = path->FAT[curr_granule];

		path->FAT[curr_granule] = 0xFF;

		curr_granule = next_granule;
	}

	path->FAT[curr_granule] = 0xFF;


	/* 5. Close the path. */

	return _decb_close(path);
}

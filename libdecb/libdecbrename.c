/********************************************************************
 * rename.c - Rename file entry for Disk BASIC
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "cocotypes.h"
#include "cocosys.h"
#include "decbpath.h"
#include "cococonv.h"
#include "util.h"


error_code _decb_rename(char *pathlist, char *new_name)
{
	decb_dir_entry dirent;

	return _decb_rename_ex(pathlist, new_name, &dirent);
}



error_code _decb_rename_ex(char *pathlist, char *new_name,
			   decb_dir_entry * dirent)
{
	error_code ec = 0;
	char nfilename[8], nextension[3];
	decb_path_id path;
	char *old_name;
	error_code read_ec;

	/* 1. Test if path is native. */
	if ((old_name = strchr(pathlist, ',')) == NULL)
	{
		return EOS_BPNAM;
	}
	old_name++;

	/* 2. Validate old and new name */
	ec = _decb_prsnam(old_name);
	if (ec != 0)
	{
		return ec;
	}

	ec = _decb_prsnam(new_name);
	if (ec != 0)
	{
		return ec;
	}

	/* 3. Convert the new filename to the format required by dentry */
	CStringToDECBString((unsigned char *) nfilename,
			    (unsigned char *) nextension,
			    (unsigned char *) new_name);

	/* 4. Open a path to the file */
	ec = _decb_open(&path, pathlist, FAM_READ | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}

	/* 5. _decb_open found the file without regard to case, as every
	      command does, so that is the entry to rename. */

	ec = 0;

	/* See if another file in this directory has the same name as our
	   destination.  Case doesn't tell names apart, but the file's own
	   entry is not in the way, so only the case of a name can change. */

	_decb_seekdir(path, 0, SEEK_SET);

	while ((read_ec = _decb_readdir(path, dirent)) == 0)
	{
		if (path->directory_entry_index - 1 == path->this_directory_entry_index)
		{
			continue;
		}

		if (!strncasecmp((char *) dirent->filename, nfilename, 8)
		    && !strncasecmp((char *) dirent->file_extension, nextension,
				3))
		{
			ec = EOS_FAE;

			break;
		}
	}

	if (ec == EOS_FAE || read_ec != EOS_PNNF)
	{
		_decb_close(path);

		return ec == EOS_FAE ? ec : read_ec;
	}

	/* Rename the source and write its entry back to the image. */

	*dirent = path->dir_entry;
	memcpy(dirent->filename, nfilename, 8);
	memcpy(dirent->file_extension, nextension, 3);

	_decb_seekdir(path, path->this_directory_entry_index, SEEK_SET);
	ec = _decb_writedir(path, dirent);

	{
		error_code close_ec = _decb_close(path);

		if (ec == 0)
		{
			ec = close_ec;
		}
	}

	return ec;
}

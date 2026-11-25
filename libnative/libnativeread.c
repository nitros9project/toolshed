/********************************************************************
 * read.c - Native Read routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "nativepath.h"


error_code _native_read(native_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	size_t ret_size;


	/* 1. Check the mode. */

	if ((path->mode & FAM_DIR) != 0 || (path->mode & FAM_READ) == 0)
	{
		/* 1. Caller must use '_native_readdir' to read directories. */

		ec = EOS_BMODE;
	}
	else
	{
		/* 1. Perform the read. */

		ret_size = fread(buffer, 1, *size, path->fd);

		if (ret_size <= 0 && *size > 0)
		{
			/* 1. We have reached end of file. */

			ec = EOS_EOF;
		}

		*size = ret_size;
	}


	return ec;
}


error_code _native_readdir(native_path_id path, native_dir_entry * dirent)
{
	error_code ec = 0;
	native_dir_entry *e;

	/* 1. Check the mode. */

	if ((path->mode & FAM_DIR) == 0 || (path->mode & FAM_READ) == 0)
	{
		/* 1. Must be a directory. */

		return EOS_BMODE;
	}


	e = readdir(path->dirhandle);

	if (e == NULL)
	{
		return EOS_EOF;
	}

	memcpy(dirent, e, sizeof(native_dir_entry));

	return ec;
}

error_code _native_ncpy_name(native_dir_entry e, u_char * name, size_t len)
{
	error_code ec = 0;

	/* Copy name from dir entry to suppilied buffer */
	strncpy((char *) name, e.d_name, len);

	return ec;

}

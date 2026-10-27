/********************************************************************
 * rename.c - Rename file entry for OS-9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "cocotypes.h"
#include "cococonv.h"
#include "cocosys.h"
#include "nativepath.h"



error_code _native_rename(char *pathlist, char *new_name)
{
	error_code ec = 0;
	char *a = strrchr(pathlist, '/');
	size_t prefix_length = a == NULL ? 0 : a - pathlist + 1;
	char *dest = malloc(prefix_length + strlen(new_name) + 1);


	if (dest == NULL)
	{
		return 1;
	}


	/* Construct destination path name */

	memcpy(dest, pathlist, prefix_length);
	strcpy(dest + prefix_length, new_name);

	if (rename(pathlist, dest) != 0)
	{
		ec = errno;
	}

	free(dest);


	return ec;
}

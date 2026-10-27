/********************************************************************
 * open.c - Native open/create routines
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "cocotypes.h"
#include "cococonv.h"
#include "cocosys.h"
#include "nativepath.h"


static int init_pd(native_path_id * path, int mode);
static int term_pd(native_path_id path);


static int init_pd(native_path_id * path, int mode)
{
	/* 1. Allocate path structure and initialize it. */

	*path = malloc(sizeof(struct _native_path_id));

	if (*path == NULL)
	{
		return 1;
	}


	/* 2. Clear out newly allocated path structure. */

	memset(*path, 0, sizeof(**path));

	(*path)->mode = mode;


	return 0;
}



static int term_pd(native_path_id path)
{
	/* 1. Deallocate path structure. */

	free(path);

	return 0;
}



/*
 * _native_create()
 *
 * Create a file on the native fs
 */
error_code _native_create(native_path_id * path, char *pathlist, int mode,
			  int perms)
{
	error_code ec = 0;
	char *nativeMode = "r";


	/* 1. Initialize path. */

	if (init_pd(path, mode) != 0)
	{
		return -1;
	}


	/* 2. Convert passed mode to native file mode. */

	if (mode & (FAM_WRITE | FAM_READ))
	{
		nativeMode = "wb+";
	}
	else if (mode & FAM_READ)
	{
		nativeMode = "rb";
	}
	else if (mode & FAM_WRITE)
	{
		nativeMode = "wb";
	}

	strncpy((*path)->pathlist, pathlist, sizeof((*path)->pathlist) - 1);
	(*path)->pathlist[sizeof((*path)->pathlist) - 1] = '\0';

	(*path)->fd = fopen(pathlist, nativeMode);

	if ((*path)->fd == NULL)
	{
		ec = UnixToCoCoError(errno);
	}

	if (ec != 0)
	{
		free(*path);
		*path = NULL;
	}
	return ec;
}



/*
 * _native_open()
 *
 * Open a file on the native fs
 */
error_code _native_open(native_path_id * path, char *pathlist, int mode)
{
	error_code ec = 0;
	char *nativeMode = "r";


	/* 1. Initialize path. */

	if (init_pd(path, mode) != 0)
	{
		return -1;
	}


	/* 2. Determine if this is a directory. */

	if (mode & FAM_DIR)
	{
		struct stat statbuf;


		ec = stat(pathlist, &statbuf);

		if (ec != 0)
		{
			term_pd(*path);
			return UnixToCoCoError(errno);
		}

		if (statbuf.st_mode & S_IFDIR)
		{
			/* 1. Open as a directory. */

			(*path)->dirhandle = opendir(pathlist);
			ec = 0;
		}
		else
		{
			term_pd(*path);

			ec = EOS_BMODE;
		}

		return ec;
	}


	/* 2. Convert passed mode to native file mode. */

	if ((mode & (FAM_WRITE | FAM_READ)) == (FAM_WRITE | FAM_READ))
	{
		nativeMode = "rb+";
	}
	else if (mode & FAM_READ)
	{
		nativeMode = "rb";
	}
	else if (mode & FAM_WRITE)
	{
		nativeMode = "wb";
	}

	strncpy((*path)->pathlist, pathlist, sizeof((*path)->pathlist) - 1);
	(*path)->pathlist[sizeof((*path)->pathlist) - 1] = '\0';

	if ((mode & (FAM_WRITE | FAM_READ)) == FAM_WRITE)
	{
		int flags = O_WRONLY;
		int fd;

#if defined(WIN32)
		flags |= O_BINARY;
#endif
		/* fopen("wb") truncates; fdopen preserves the existing file. */
		fd = open(pathlist, flags);
		if (fd >= 0)
		{
			(*path)->fd = fdopen(fd, nativeMode);
			if ((*path)->fd == NULL)
			{
				int saved_errno = errno;

				close(fd);
				errno = saved_errno;
			}
		}
	}
	else
	{
		(*path)->fd = fopen(pathlist, nativeMode);
	}

	if ((*path)->fd == NULL)
	{
		ec = UnixToCoCoError(errno);
	}

	if (ec != 0)
	{
		free(*path);
		*path = NULL;
	}
	return ec;
}



/*
 * _native_close()
 *
 * Close a path to a file
 */
error_code _native_close(native_path_id path)
{
	if (path != NULL)
	{
		/* 1. This is a valid path. */

		if (path->mode & FAM_DIR)
		{
#if defined(WIN32)
			path->dirhandle = NULL;
#else
			closedir(path->dirhandle);
#endif
		}
		else
		{
			fclose(path->fd);
		}

		term_pd(path);
	}


	return 0;
}

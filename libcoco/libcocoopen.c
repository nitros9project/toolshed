/********************************************************************
 * open.c - CoCo open/create routines
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <sys/stat.h>

#include "cococonv.h"
#include "cocosys.h"
#include "cocotypes.h"
#include "cocopath.h"

/*
 * _coco_create()
 *
 * Create a file
 */
error_code _coco_create(coco_path_id * path, char *pathlist, int mode,
			coco_file_stat * fstat)
{
	error_code ec = 0;

	/* 1. Allocate memory for the path id. */

	*path = malloc(sizeof(struct _coco_path_id));

	if (*path == NULL)
	{
		return EOS_BPNAM;
	}

	/* 2. Determine the pathlist type. */

	ec = _coco_identify_image(pathlist, &(*path)->type);

	if (ec != 0)
	{
		free(*path);
		*path = NULL;
		return ec;
	}

	/* 3. Call appropriate create function. */

	switch ((*path)->type)
	{
	case NATIVE:
		ec = _native_create(&((*path)->path.native), pathlist, mode,
				    fstat->perms);
		break;

	case OS9:
		ec = _os9_create(&((*path)->path.os9), pathlist, mode,
				 fstat->perms);
		break;

	case DECB:
		ec = _decb_create(&((*path)->path.decb), pathlist, mode,
				  fstat->file_type, fstat->data_type);
		break;

	case DRAGONDOS:
		ec = _dragondos_create(&((*path)->path.dragondos), pathlist, mode);
		break;

	case CECB:
		ec = _cecb_create(&((*path)->path.cecb), pathlist, mode,
				  fstat->file_type, fstat->data_type,
				  fstat->gap_flag, fstat->ml_load_address,
				  fstat->ml_exec_address);
		break;

	}

	if (ec != 0)
	{
		free(*path);
		*path = NULL;
	}
	return ec;
}


/*
 * _coco_open()
 *
 * Open a path to a file or directory.
 *
 * Legal pathnames are:
 *
 * 1. imagename,@     (considered to be a 'raw' open of the image)
 * 2. imagename,      (considered to be an open of the root directory)
 * 3. imagename,file  (considered to be a file or directory open within the image)
 * 4. imagename       (considered to be a native open)
 *
 * The presence of a comma in the pathlist indicates that at the least, a non-native open will
 * be performed.
 */
error_code _coco_open(coco_path_id * path, char *pathlist, int mode)
{
	error_code ec = 0;

	/* 1. Allocate memory for the path id. */

	*path = malloc(sizeof(struct _coco_path_id));

	if (*path == NULL)
	{
		return EOS_BPNAM;
	}

	/* 2. Determine the pathlist type. */

	ec = _coco_identify_image(pathlist, &(*path)->type);

	if (ec != 0)
	{
		free(*path);
		*path = NULL;
		return ec;
	}

	/* 3. Call appropriate open function. */

	switch ((*path)->type)
	{
	case NATIVE:
		ec = _native_open(&((*path)->path.native), pathlist, mode);
		break;

	case OS9:
		ec = _os9_open(&((*path)->path.os9), pathlist, mode);
		break;

	case DECB:
		ec = _decb_open(&((*path)->path.decb), pathlist, mode);
		break;

	case DRAGONDOS:
		ec = _dragondos_open(&((*path)->path.dragondos), pathlist, mode);
		break;

	case CECB:
		ec = _cecb_open(&((*path)->path.cecb), pathlist, mode);
		break;

	}

	if (ec != 0)
	{
		free(*path);
		*path = NULL;
	}
	return ec;
}


/*
 * _coco_close()
 *
 * Close a file
 */
error_code _coco_close(coco_path_id path)
{
	error_code ec = 0;

	/* 1. Call appropriate close function. */

	switch (path->type)
	{
	case NATIVE:
		ec = _native_close(path->path.native);
		break;

	case OS9:
		ec = _os9_close(path->path.os9);
		break;

	case DECB:
		ec = _decb_close(path->path.decb);
		break;

	case DRAGONDOS:
		ec = _dragondos_close(path->path.dragondos);
		break;

	case CECB:
		ec = _cecb_close(path->path.cecb);
	}

	free(path);

	return ec;
}


/*
 * not_os9_type()
 *
 * Disk BASIC disks have nothing to identify them, so they are what remains.
 */
static _path_type not_os9_type(const char *image)
{
	int is_dragondos = 0;

	_dragondos_identify(image, &is_dragondos);

	return is_dragondos ? DRAGONDOS : DECB;
}


/*
 * _coco_identify_image()
 *
 * Determines if the passed <image,path> pathlist is native, OS-9, Disk BASIC, Cassette BASIC
 * or Dragon DOS.
 */
error_code _coco_identify_image(char *pathlist, _path_type * type)
{
	error_code ec = 0;
	char *p, *path, *colon;
	char *tmppathlist;
	native_path_id native;

	if (strchr(pathlist, ',') == NULL)
	{
		/* 1. No native/coco delimiter in pathlist, it's native. */

		*type = NATIVE;

		return ec;
	}

	/* 2. Check validity of pathlist. */

	tmppathlist = strdup(pathlist);

	p = strtok(tmppathlist, ",");

	if (p == NULL)		/* if only comma */
	{
		free(tmppathlist);

		return EOS_BPNAM;
	}

	/* 2a. Check for a colon followed by number in path part */

	path = strtok(NULL, "");
	if (path && (colon = strchr(path, ':')) && isdigit((unsigned char)colon[1]))
	{
		*type = DECB;

		free(tmppathlist);
		return ec;
	}

	/* 2b. Check for .cas file extension. */

	if (strendcasecmp(p, CAS_FILE_EXTENSION) == 0)
	{
		*type = CECB;

		free(tmppathlist);
		return ec;
	}

	/* 2c. Check for .c10 file extension. */

	if (strendcasecmp(p, C10_FILE_EXTENSION) == 0)
	{
		*type = CECB;

		free(tmppathlist);
		return ec;
	}

	/* 3. Determine if this is an OS-9, DECB or CECB image. */

	if (_native_open(&native, tmppathlist, FAM_READ) == 0)
	{
		u_char sector_buffer[256];
		u_int size = sizeof(sector_buffer);


		/* 1. Read the start of the file. */

		if (_native_read(native, sector_buffer, &size) != 0 || size < sizeof(sector_buffer))
		{
			ec = EOS_BPNAM;
		}

		_native_close(native);

		if (ec == 0)
		{
			int is_os9 = 0;

			/* 0. Look for WAV file marker */

			if (strncmp((char *) sector_buffer, "RIFF", 4) == 0)
			{
				*type = CECB;
			}
			else
			{
				/* 1. Look for markers that this is an OS-9 disk image. */

				_os9_identify(tmppathlist, &is_os9);

				/* 2. If not, it is probably a DECB or Dragon DOS disk image. */

				*type = is_os9 ? OS9 : not_os9_type(tmppathlist);
			}
		}
	}
	else
	{
		/* The image file itself could not be opened (does not exist, or is
		   unreadable) - that is "pathname not found", not a malformed name.
		   Returning EOS_BPNAM here made every os9/decb/cecb command report a
		   missing image as "badly formed pathname", pointing at the wrong
		   thing entirely. */
		ec = EOS_PNNF;
	}

	free(tmppathlist);

	return ec;
}

/*
 * _coco_detect_filesystems()
 *
 * Determine which filesystems a disk image could hold, from a pathname or an
 * <image,path> pathlist.  What is found is only what could be there: a disk
 * can hold several filesystems, and Disk BASIC has nothing to identify it. A
 * file that is not a disk image holds none.
 */
error_code _coco_detect_filesystems(const char *pathlist, u_int *filesystems)
{
	diskimage_format format;
	const char *comma = strchr(pathlist, ',');
	char *image;
	error_code ec;

	*filesystems = 0;

	image = strdup(pathlist);
	if (image == NULL)
	{
		return EOS_OM;
	}
	if (comma != NULL)
	{
		image[comma - pathlist] = '\0';
	}

	ec = _diskimage_identify(image, &format);
	if (ec == 0 && format != DISKIMAGE_UNKNOWN)
	{
		int found = 0;

		if (_os9_identify(image, &found) == 0 && found)
		{
			*filesystems |= COCO_FS_OS9;
		}
		if (_decb_identify(image, &found) == 0 && found)
		{
			*filesystems |= COCO_FS_DECB;
		}
		if (_dragondos_identify(image, &found) == 0 && found)
		{
			*filesystems |= COCO_FS_DRAGONDOS;
		}
	}

	free(image);

	return ec;
}


/*
 * _coco_filesystem_names()
 *
 * Name filesystems, separated by commas.
 */
void _coco_filesystem_names(u_int filesystems, char *text, size_t size)
{
	snprintf(text, size, "%s%s%s%s%s",
		 (filesystems & COCO_FS_OS9) ? "OS-9" : "",
		 (filesystems & COCO_FS_OS9) && (filesystems & ~COCO_FS_OS9) ? ", " : "",
		 (filesystems & COCO_FS_DECB) ? "Disk BASIC" : "",
		 (filesystems & COCO_FS_DECB) && (filesystems & COCO_FS_DRAGONDOS) ? ", " : "",
		 (filesystems & COCO_FS_DRAGONDOS) ? "Dragon DOS" : "");
}


/*
 * _coco_print_other_filesystems()
 *
 * Finish a directory listing of one filesystem by naming any others the
 * image could hold.
 */
void _coco_print_other_filesystems(const char *pathlist, u_int current)
{
	u_int filesystems;

	if (_coco_detect_filesystems(pathlist, &filesystems) == 0 && (filesystems & ~current) != 0)
	{
		char names[64];

		_coco_filesystem_names(filesystems & ~current, names, sizeof(names));
		printf("\nOther possible filesystems: %s\n", names);
	}
}


/* Say why an existing host file might be a disk image, or NULL if it doesn't
   seem to be.  Raw images have nothing to identify them but a size in whole
   sectors, which small files often have by chance, so at least a track is
   needed.  An image cut shorter than that goes unnoticed. */
static const char *disk_image_suspicion(const char *pathname)
{
	static const char *const extensions[] = { ".dsk", ".vdk", ".dmk", ".sdf", ".jvc", ".os9", ".img", ".ima", NULL };
	const long min_suspect_size = 18 * 256;
	const char *dot = strrchr(pathname, '.');
	struct stat existing;
	diskimage_format format = DISKIMAGE_UNKNOWN;
	int i;

	if (stat(pathname, &existing) != 0 || S_ISDIR(existing.st_mode))
	{
		return NULL;
	}

	for (i = 0; dot != NULL && extensions[i] != NULL; i++)
	{
		if (strcasecmp(dot, extensions[i]) == 0)
		{
			return "by its name";
		}
	}

	if (_diskimage_identify(pathname, &format) != 0)
	{
		return NULL;
	}

	switch (format)
	{
	case DISKIMAGE_DMK:
		return "it has a DMK header";
	case DISKIMAGE_SDF:
		return "it has an SDF header";
	case DISKIMAGE_VDK:
		return "it has a VDK header";
	case DISKIMAGE_JVC:
		return existing.st_size >= min_suspect_size ? "it has a JVC header" : NULL;
	case DISKIMAGE_RAW:
		return existing.st_size >= min_suspect_size ? "its size is a whole number of sectors" : NULL;
	default:
		return NULL;
	}
}


/*
 * _coco_refuse_disk_image_target()
 *
 * -r doesn't change this: forgetting a comma mustn't cost a whole disk.
 */
int _coco_refuse_disk_image_target(const char *command, const char *destination)
{
	const char *reason = strchr(destination, ',') == NULL ? disk_image_suspicion(destination) : NULL;

	if (reason == NULL)
	{
		return 0;
	}

	fprintf(stderr, "%s: won't overwrite '%s': it looks like a disk image (%s). "
		"To copy into it, use '%s,'. To replace it, delete it first.\n",
		command, destination, reason, destination);

	return 1;
}


#define BLOCKSIZE 256

/*
 * _coco_open_read_whole_file()
 *
 * Read in entire file without using _coco_gs_size().
 */
error_code _coco_open_read_whole_file(coco_path_id * path, char *pathlist,
				      int mode, u_char ** buffer,
				      u_int * size)
{
	error_code ec = 0;
	u_int size2, size3;
	u_char *buffer2;

	ec = _coco_open(path, pathlist, mode);
	if (ec != 0)
		return ec;

	*size = 0;
	size3 = BLOCKSIZE;
	*buffer = malloc(size3);

	if (*buffer == NULL)
		return -1;

	while (_coco_gs_eof(*path) == 0)
	{
		while ((*size + BLOCKSIZE) > size3)
		{
			size3 += BLOCKSIZE;
			buffer2 = realloc(*buffer, size3);

			if (buffer2 == NULL)
				return -1;

			*buffer = buffer2;
		}

		size2 = BLOCKSIZE;
		ec = _coco_read(*path, &((*buffer)[*size]), &size2);
		*size += size2;
		if (ec != 0)
			return ec;
	}

	return ec;
}

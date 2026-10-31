/********************************************************************
 * open.c - CoCo open/create routines
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

	case CECB:
		ec = _cecb_close(path->path.cecb);
	}

	free(path);

	return ec;
}


/*
 * _coco_identify_image()
 *
 * Determines if the passed <image,path> pathlist is native, OS-9, Disk BASIC or Cassette BASIC.
 */
error_code _coco_identify_image(char *pathlist, _path_type * type)
{
	error_code ec = 0;
	char *p, *path, *colon;
	char *tmppathlist;
	FILE *fp;

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

	fp = fopen(tmppathlist, "rb");

	if (fp != NULL)
	{
		u_char sector_buffer[256];


		/* 1. Read sector 0. */

		if (fread(sector_buffer, 1, 256, fp) < 256)
		{
			ec = EOS_BPNAM;
		}
		else
		{
			Lsn0_sect os9_sector = (Lsn0_sect) sector_buffer;
			int dir_sector_offset;
			int bps = 256;
			int is_osk = (memcmp(os9_sector->dd_sync, "Cruz", 4) == 0);
			int spt = int2(os9_sector->dd_spt);
			int t0s = int2(is_osk ? os9_sector->dd_opt.m68k.pd_t0s
					      : os9_sector->dd_opt.m6809.pd_t0s);
			int dir_lsn = int3(os9_sector->dd_dir) + 1;

			/* 0. Look for WAV file marker */

			if (strncmp((char *) sector_buffer, "RIFF", 4) == 0)
				*type = CECB;
			else
			{
				/* 1. Look for markers that this is an OS-9 disk image. */

				/* First, assume bps value is valid and get it */

				if (int1(os9_sector->dd_lsnsize) > 0)
				{
					bps = int1(os9_sector->dd_lsnsize) *
						256;
				}

				/* Then, check out the dir sector for .. and . entries. */

				/* Skip the sectors missing from a short track 0. */

				if (t0s > 0 && t0s < spt && dir_lsn >= t0s)
				{
					dir_lsn += spt - t0s;
				}

				dir_sector_offset = dir_lsn * bps;

				fseek(fp, dir_sector_offset, SEEK_SET);

				if (fread(sector_buffer, 1, 256, fp) < 256)
				{
					*type = DECB;
				}
				else
				{
					if (sector_buffer[0] == 0x2E
					    && sector_buffer[1] == 0xAE
					    && sector_buffer[32] == 0xAE)
					{
						/* 1. This is likely an OS-9 disk image. */

						*type = OS9;
					}
					else
					{
						/* 1. This is probably a DECB disk image. */

						*type = DECB;
					}
				}
			}
		}

		fclose(fp);
	}
	else
	{
		ec = EOS_BPNAM;
	}

	free(tmppathlist);

	return ec;
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

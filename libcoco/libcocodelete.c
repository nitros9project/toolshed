/********************************************************************
 * delete.c - Delete file entry for OS-9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "cocotypes.h"
#include "cocopath.h"


error_code _coco_delete(char *pathlist)
{
	error_code ec = 0;
	_path_type disk_type;


	/* 1. Determine the path type. */

	_coco_identify_image(pathlist, &disk_type);


	/* 2. Call appropriate function. */

	switch (disk_type)
	{
	case NATIVE:
		ec = _native_delete(pathlist);
		break;

	case OS9:
		ec = _os9_delete(pathlist);
		break;

	case DECB:
		ec = _decb_kill(pathlist);
		break;

	case DRAGONDOS:
		ec = _dragondos_kill(pathlist);
		break;

	case CECB:
		fprintf(stderr,
			"_coco_delete not implemented in libcecb yet.\n");
		ec = -1;
		break;
	}


	return ec;
}



error_code _coco_delete_directory(char *pathlist)
{
	error_code ec = 0;
	_path_type disk_type;


	/* 1. Determine the path type. */

	_coco_identify_image(pathlist, &disk_type);


	/* 2. Call appropriate function. */

	switch (disk_type)
	{
	case NATIVE:
		ec = _native_delete_directory(pathlist);
		break;

	case OS9:
		ec = _os9_delete_directory(pathlist);
		break;

	case DECB:
		ec = EOS_BPNAM;
		break;

	case DRAGONDOS:
		ec = EOS_BPNAM;
		break;

	case CECB:
		fprintf(stderr,
			"_coco_delete_directory not implemented in libcecb yet.\n");
		ec = -1;
		break;
	}


	return ec;
}

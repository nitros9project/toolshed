/********************************************************************
 * makdir.c - Create an os9 directory
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cocotypes.h"
#include "cocopath.h"


error_code _coco_makdir(char *pathlist)
{
	error_code ec = 0;
	_path_type disk_type;


	/* 1. Determine the path type. */

	_coco_identify_image(pathlist, &disk_type);


	/* 2. Call appropriate function. */

	switch (disk_type)
	{
	case NATIVE:
		ec = _native_makdir(pathlist);
		break;

	case OS9:
		ec = _os9_makdir(pathlist);
		break;

	case DECB:
		ec = EOS_FNA;
		break;

	case DRAGONDOS:
		ec = EOS_FNA;
		break;

	case CECB:
		fprintf(stderr,
			"_coco_makdir not implemented in libcecb yet.\n");
		ec = -1;
		break;
	}


	return ec;
}

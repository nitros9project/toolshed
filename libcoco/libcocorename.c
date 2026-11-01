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
#include "nativepath.h"
#include "cococonv.h"
#include "cocosys.h"


error_code _coco_rename_ex(char *pathlist, char *new_name,
			   coco_dir_entry * dentry)
{
	error_code ec = 0;
	_path_type type;

	/* 1. Call appropriate function. */

	ec = _coco_identify_image(pathlist, &type);

	if (ec != 0)
	{
		return ec;
	}

	switch (type)
	{
	case NATIVE:
		ec = _native_rename(pathlist, new_name);
		break;

	case OS9:
		ec = _os9_rename_ex(pathlist, new_name, &dentry->dentry.os9);
		break;

	case DECB:
		ec = _decb_rename_ex(pathlist, new_name,
				     &dentry->dentry.decb);
		break;

	case DRAGONDOS:
		ec = _dragondos_rename_ex(pathlist, new_name,
					  &dentry->dentry.dragondos);
		break;

	case CECB:
		fprintf(stderr,
			"_coco_rename not implemented in libcecb yet.\n");
		ec = -1;
		break;
	}

	return ec;
}


error_code _coco_rename(char *pathlist, char *new_name)
{
	coco_dir_entry dentry;

	return _coco_rename_ex(pathlist, new_name, &dentry);
}

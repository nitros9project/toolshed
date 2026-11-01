/********************************************************************
 * read.c - CoCo Read routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "cocopath.h"



error_code _coco_read(coco_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;


	/* 1. Call appropriate function. */

	switch (path->type)
	{
	case NATIVE:
		ec = _native_read(path->path.native, buffer, size);
		break;

	case OS9:
		ec = _os9_read(path->path.os9, buffer, size);
		break;

	case DECB:
		ec = _decb_read(path->path.decb, buffer, size);
		break;

	case DRAGONDOS:
		ec = _dragondos_read(path->path.dragondos, buffer, size);
		break;

	case CECB:
		ec = _cecb_read(path->path.cecb, buffer, size);
		break;
	}


	return ec;
}



error_code _coco_readdir(coco_path_id path, coco_dir_entry * e)
{
	error_code ec = 0;


	/* 1. Call appropriate function. */

	switch (path->type)
	{
	case NATIVE:
		ec = _native_readdir(path->path.native, &e->dentry.native);
		break;

	case OS9:
		ec = _os9_readdir(path->path.os9, &e->dentry.os9);
		break;

	case DECB:
		ec = _decb_readdir(path->path.decb, &e->dentry.decb);
		break;

	case DRAGONDOS:
		/* Only entries that begin a file have a name to report. */
		do
		{
			ec = _dragondos_readdir(path->path.dragondos, &e->dentry.dragondos);
		}
		while (ec == 0 && (e->dentry.dragondos.flags & (DRAGONDOS_FLAG_DELETED | DRAGONDOS_FLAG_CONTINUATION)));
		break;

	case CECB:
		fprintf(stderr,
			"_coco_readdir not implemented in libcecb yet.\n");
		ec = -1;
		break;
	}

	e->type = path->type;

	return ec;
}

error_code _coco_ncpy_name(coco_dir_entry * e, u_char * name, size_t len)
{
	error_code ec = 0;

	switch (e->type)
	{
	case NATIVE:
		ec = _native_ncpy_name(e->dentry.native, name, len);
		break;

	case OS9:
		ec = _os9_ncpy_name(e->dentry.os9, name, len);
		break;

	case DECB:
		ec = _decb_ncpy_name(e->dentry.decb, name, len);
		break;

	case DRAGONDOS:
		ec = _dragondos_ncpy_name(&e->dentry.dragondos, (char *) name, len);
		break;

	case CECB:
		ec = _cecb_ncpy_name(e->dentry.cecb, name, len);
		break;
	}

	return ec;
}

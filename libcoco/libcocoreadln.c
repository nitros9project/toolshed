/********************************************************************
 * readln.c - CoCo ReadLn routine
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "cocopath.h"


error_code _coco_readln(coco_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;


	/* 1. Call appropriate function. */

	switch (path->type)
	{
	case NATIVE:
		ec = _native_readln(path->path.native, buffer, size);
		break;

	case OS9:
		ec = _os9_readln(path->path.os9, buffer, size);
		break;

	case DECB:
		ec = _decb_readln(path->path.decb, buffer, size);
		break;

	case DRAGONDOS:
		ec = _dragondos_readln(path->path.dragondos, buffer, size);
		break;

	case CECB:
		ec = _cecb_readln(path->path.cecb, buffer, size);
		break;
	}


	return ec;
}

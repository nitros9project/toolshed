/********************************************************************
 * seek.c - CoCo Seek routine
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "nativepath.h"



error_code _coco_seek(coco_path_id path, int pos, int mode)
{
	error_code ec = 0;


	/* 1. Call appropriate function. */

	switch (path->type)
	{
	case NATIVE:
		ec = _native_seek(path->path.native, pos, mode);
		break;

	case OS9:
		ec = _os9_seek(path->path.os9, pos, mode);
		break;

	case DECB:
		ec = _decb_seek(path->path.decb, pos, mode);
		break;

	case CECB:
		ec = _cecb_seek(path->path.cecb, pos, mode);
		break;
	}


	return ec;
}

/********************************************************************
 * readln.c - CoCo ReadLn routine
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "nativepath.h"


error_code _native_readln(native_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	u_int i;
	char *buf_ptr = buffer;


	for (i = 0; i < *size; i++)
	{
		int ret;

		ret = fread(buf_ptr, 1, 1, path->fd);

		if (ret == 0)
		{
			*size = i;

			if (*size == 0)
			{
				/* 1. We haven't read a char so we must be at EOF. */

				return EOS_EOF;
			}
			else
			{
				return 0;
			}
		}

		if (*buf_ptr == 0x0A)
		{
			*buf_ptr = 0x0D;
			*size = i + 1;
			break;
		}

		buf_ptr++;
	}


	return ec;
}

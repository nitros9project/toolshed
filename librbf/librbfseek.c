/********************************************************************
 * seek.c - OS-9 Seek routine
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "os9path.h"


error_code _os9_seek(os9_path_id path, int pos, int mode)
{
	error_code ec = 0;


	if (path->israw == 1)
	{
		long long base = 0;

		if (mode == SEEK_CUR)
		{
			base = path->filepos;
		}
		else if (mode == SEEK_END)
		{
			base = (long long) int3(path->lsn0->dd_tot) * path->bps;
		}

		if (base + pos < 0)
		{
			return EOS_SE;
		}

		path->filepos = base + pos;
	}
	else
	{
		switch (mode)
		{
		case SEEK_SET:
			path->filepos = pos;
			break;

		case SEEK_CUR:
			path->filepos = path->filepos + pos;
			break;

		case SEEK_END:
			fprintf(stderr,
				"_os9_seek(): SEEK_END not implemented.\n");
			exit(0);
			break;
		}
	}


	return ec;
}

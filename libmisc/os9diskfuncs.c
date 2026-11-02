/********************************************************************
 * os9diskfuncs.c - Miscellaneous OS-9 file functions
 *
 * $Id$
 ********************************************************************/
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef BDS
#include <unistd.h>
#endif
#include <cocotypes.h>
#include <os9path.h>


/*
 * read_lsn()
 *
 * read the passed logical sector number
 */
int read_lsn(os9_path_id path, int lsn, void *buffer)
{
	return _os9_lsn_read(path, lsn, 0, buffer, path->bps);
}


/*
 * show_attrs()
 *
 * Prints textual representation of file attributes to standard output
 */
void show_attrs(int attr_byte)
{
	int i;

	/* print attributes */
	for (i = 7; i >= 0; i--)
	{
		char *attrs = "rwerwesd";

		if (attr_byte & (1 << i))
		{
			/* bit set, print attr */
			printf("%c", attrs[i]);
		}
		else
		{
			printf("-");
		}
	}
}

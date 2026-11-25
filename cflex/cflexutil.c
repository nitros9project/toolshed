/********************************************************************
 * cflexutil.c - Helpers the cflex commands share
 ********************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <toolshed.h>

#include "cflex.h"


int cflex_disk_pathlist(const char *disk, char *pathlist, size_t size)
{
	int length = snprintf(pathlist, size, "%s%s", disk, strchr(disk, ',') == NULL ? "," : "");

	return (length < 0 || (size_t) length >= size) ? EOS_BPNAM : 0;
}


void cflex_print_field(const unsigned char *field, int length, int pad)
{
	int i;

	for (i = 0; i < length; i++)
	{
		if (field[i] == 0)
		{
			if (pad)
			{
				putchar(' ');
			}
		}
		else if (isprint(field[i]))
		{
			putchar(field[i]);
		}
		else
		{
			printf("\\%o", field[i]);
		}
	}
}

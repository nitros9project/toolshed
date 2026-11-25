/********************************************************************
 * dgndosutil.c - Helpers the dgndos commands share
 ********************************************************************/
#include <stdio.h>
#include <string.h>
#include <toolshed.h>

#include "dgndos.h"


int dgndos_disk_pathlist(const char *disk, char *pathlist, size_t size)
{
	int length = snprintf(pathlist, size, "%s%s", disk, strchr(disk, ',') == NULL ? "," : "");

	return (length < 0 || (size_t) length >= size) ? EOS_BPNAM : 0;
}

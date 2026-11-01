/********************************************************************
 * dgndosfree.c - Disk utilization reporting utility for Dragon DOS
 ********************************************************************/
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: free {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Displays the amount of free space on an image.\n",
	"Options:\n",
	NULL
};


static error_code do_free(char *disk)
{
	char pathlist[1024];
	dragondos_path_id path;
	u_int sectors;
	error_code ec = dgndos_disk_pathlist(disk, pathlist, sizeof(pathlist));

	if (ec == 0)
	{
		ec = _dragondos_open(&path, pathlist, FAM_READ);
	}
	if (ec != 0)
	{
		return ec;
	}

	ec = _dragondos_gs_free(path, &sectors);
	if (ec == 0)
	{
		printf("Free sectors: %u (%u bytes)\n", sectors, sectors * DRAGONDOS_SECTOR_SIZE);
	}

	_dragondos_close(path);
	return ec;
}


int dgndosfree(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (DGNDOS_USAGE_ERROR);
				}
			}
		}
	}

	/* walk command line for pathnames */
	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = do_free(p);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d determining free space for '%s'\n", argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

/********************************************************************
 * decbfree.c - Disk utilization reporting utility for Disk BASIC
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <decbpath.h>
#include <toolshed.h>


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: free {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Displays the amount of free space on an image.\n",
	"Options:\n",
	NULL
};


int decbfree(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	u_int free_granules;

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
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (EOS_IA);
				}
			}
		}
	}

	/* walk command line for pathnames */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			p = argv[i];
		}


		ec = TSDECBFree(p, &free_granules);

		if (ec == 0)
		{
			printf("Free granules: %d (%d bytes)\n",
			       free_granules, free_granules * (4608 / 2));
		}
		else
		{
			fprintf(stderr,
				"%s: error %d determining free space for '%s'\n",
				argv[0], ec, p);
			return (ec);
		}
	}

	if (argv[1] == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (0);
}

/********************************************************************
 * os9free.c - Disk utilization utility for OS-9
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <queue.h>
#include <toolshed.h>


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: free {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Displays the amount of free space on an image.\n",
	"Options:\n",
	NULL
};


int os9free(int argc, char *argv[])
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
		char dname[64];
		u_int month, day, year;
		u_int bps, total_sectors, bytes_free, free_sectors,
			largest_free_block, sectors_per_cluster;
		u_int largest_count, sector_count;
		u_int postfixDivisor = 1;
		char postfix[4];

		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			p = argv[i];
		}

		ec = TSRBFFree(p, dname, &month, &day, &year, &bps,
			       &total_sectors, &bytes_free, &free_sectors,
			       &largest_free_block, &sectors_per_cluster,
			       &largest_count, &sector_count);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);
			return (ec);
		}
		printf("\n\"%s\" created on %02d/%02d/%04d\n",
		       dname, month, day, year);

		if ((total_sectors * bps) < (1024 * 1024))
		{
			postfixDivisor = 1024;
			strcpy(postfix, "KB");
		}
		else if ((total_sectors * bps) < (1024 * 1024 * 1024))
		{
			postfixDivisor = (1024 * 1024);
			strcpy(postfix, "MB");
		}
		else
		{
			postfixDivisor = (1024 * 1024 * 1024);
			strcpy(postfix, "GB");
		}

		printf("Capacity: %d%s (%d sectors) %d-sector clusters\n",
		       (total_sectors * bps) / postfixDivisor, postfix,
		       total_sectors, sectors_per_cluster);
		printf("%d Free sectors, largest block %d sectors\n",
		       free_sectors, largest_free_block);

		if (bytes_free < (1024 * 1024))
		{
			postfixDivisor = 1024;
			strcpy(postfix, "KB");
		}
		else if (bytes_free < (1024 * 1024 * 1024))
		{
			postfixDivisor = (1024 * 1024);
			strcpy(postfix, "MB");
		}
		else
		{
			postfixDivisor = (1024 * 1024 * 1024);
			strcpy(postfix, "GB");
		}

		printf("Free space: %d%s (%d bytes)\n",
		       bytes_free / postfixDivisor, postfix, bytes_free);

		printf("\n");
	}

	if (argv[1] == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (0);
}

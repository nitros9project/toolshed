/********************************************************************
 * os9fstat.c - File status utility for os9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cocopath.h>


static int do_fstat(char **argv, char *p);

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: fstat {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display the file descriptor sector for a file.\n",
	"Options:\n",
	NULL
};


int os9fstat(int argc, char *argv[])
{
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
				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (0);
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

			do_fstat(argv, p);
			printf("\n");
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (0);
}


static int do_fstat(char **argv, char *p)
{
	error_code ec = 0;
	os9_path_id path;
	char plural;

	/* open a path to the device, for no access since a file desc needs none */
	ec = _os9_open(&path, p, 0);
	if (ec != 0)
	{
		ec = _os9_open(&path, p, FAM_DIR);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);
			return (ec);
		}
	}

	{
		fd_stats fdbuf;
		int size = sizeof(fdbuf);
		int i;

		_os9_gs_fd(path, size, &fdbuf);

		printf("File Information for %s\n", p);
		printf("  Attributes         : ");
		{
			char attrs[9];

			OS9AttrToString(fdbuf.fd_att, attrs);
			printf("%s", attrs);
		}
		printf("\n");
		printf("  Owner              : %d.%-3d\n", fdbuf.fd_own[0],
		       fdbuf.fd_own[1]);
		printf("  Last modified date : %02d/%02d/%4d %02d:%02d\n",
		       fdbuf.fd_dat[1], fdbuf.fd_dat[2],
		       fdbuf.fd_dat[0] + 1900, fdbuf.fd_dat[3],
		       fdbuf.fd_dat[4]);
		printf("  Link count         : %d\n", fdbuf.fd_lnk);
		printf("  File size          : %1$d ($%1$X)\n",
		       int4(fdbuf.fd_siz));
		printf("  Creation date      : %02d/%02d/%4d\n",
		       fdbuf.fd_creat[1], fdbuf.fd_creat[2],
		       fdbuf.fd_creat[0] + 1900);

		printf("  Segment list:\n");
		for (i = 0; i < NUM_SEGS; i++)
		{
			if (int3(fdbuf.fd_seg[i].lsn) == 0)
			{
				break;
			}

			if (int2(fdbuf.fd_seg[i].num) < 2)
				plural = ' ';
			else
				plural = 's';

			printf("    %2d. LSN%d ($%X)   %d sector%c\n", i + 1,
			       i + 1, int3(fdbuf.fd_seg[i].lsn),
			       int2(fdbuf.fd_seg[i].num), plural);
		}
	}

	_os9_close(path);

	return (0);
}

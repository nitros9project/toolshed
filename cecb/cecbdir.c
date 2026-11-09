/********************************************************************
 * cecbdir.c - Directory utility for decb
 *
 * $Id$
 ********************************************************************/
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "cocotypes.h"
#include "cecbpath.h"

static int do_dir(char **argv, char *p);

/* Help Message */
static char const *const helpMessage[] = {
	"Syntax: dir {[<opts>]} {<dir> [<...>]} {[<opts>]}\n",
	"Usage:  Display the contents of a cassette image.\n",
	"Options:\n",
	NULL
};


int cecbdir(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;


	if (argv[1] == NULL)
	{
		show_help(helpMessage);

		return 0;
	}

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
					return 0;

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return 0;
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

		ec = do_dir(argv, p);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);

			return ec;
		}
	}


	return 0;
}

/* Each call reads further along the tape, and every error but a bad
   checksum ends the listing, so this always ends. */
static int do_dir(char **argv, char *p)
{
	error_code ec = 0;
	error_code first_error = 0;
	cecb_path_id path;
	char asciiflag;
	cecb_dir_entry dir_entry;
	int i;

	ec = _cecb_open(&path, p, FAM_READ | FAM_RAW);

	if (ec == 0)
	{
		printf("Directory of: %s\n\n", p);

		while (1)
		{
			ec = _cecb_read_next_dir_entry(path, &dir_entry);

			if (ec == EOS_EOF)
			{
				break;
			}

			if (ec != 0 && ec != EOS_CRC)
			{
				first_error = ec;
				break;
			}

			switch (dir_entry.ascii_flag)
			{
			case 0x00:
				asciiflag = 'B';
				break;
			case 0xFF:
				asciiflag = 'A';
				break;
			default:
				asciiflag = '?';
				break;
			}

			putchar(ec == EOS_CRC ? '!' : ' ');
			putchar(' ');

			/* print escaped filename */
			for (i = 0; i < 8; i++)
			{
				if (isprint(dir_entry.filename[i]))
				{
					putchar(dir_entry.filename[i]);
				}
				else
				{
					printf("\\%o", dir_entry.filename[i]);
				}
			}

			printf(" %d ($%02x) %c ($%02x)\n",
				dir_entry.file_type, dir_entry.file_type, asciiflag,
				dir_entry.ascii_flag);
		}

		ec = _cecb_close(path);

		if (first_error != 0)
		{
			ec = first_error;
		}
	}

	printf("\n");

	return ec;
}

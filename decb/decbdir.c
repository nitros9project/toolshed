/********************************************************************
 * decbdir.c - Directory utility for decb
 *
 * $Id$
 ********************************************************************/
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "cocotypes.h"
#include "decbpath.h"

static int do_dir(char **argv, char *p);


/* Help Message */
static char const *const helpMessage[] = {
	"Syntax: dir {[<opts>]} {<dir> [<...>]} {[<opts>]}\n",
	"Usage:  Display the contents of a directory.\n",
	"Options:\n",
	NULL
};


int decbdir(int argc, char *argv[])
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



static int do_dir(char **argv, char *p)
{
	error_code ec = 0;
	char decbpathlist[256], sector[256];
	decb_path_id path;
	decb_dir_entry de;


	/* 1. If a comma isn't present in the string, then add it so that the path is opened as a non-native path. */

	memset(decbpathlist, 0, 256);

	if (strchr(p, ',') == NULL)
	{
		char *q = strchr(p, ':');

		if (q == NULL)
		{
			strcpy(decbpathlist, p);
			strcat(decbpathlist, ",");
		}
		else
		{
			strncpy(decbpathlist, p, q - p);
			strcat(decbpathlist, ",");
			strcat(decbpathlist, q);
		}
	}
	else
	{
		strcpy(decbpathlist, p);
	}


	/* 3. Open a path to the device. */

	ec = _decb_open(&path, decbpathlist, FAM_READ);

	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n", argv[0], ec,
			decbpathlist);

		return ec;
	}
	else
	{
		printf("Directory of: %s\n\n", decbpathlist);
	}


	/* 4. Obtain HDB-DOS disk name, if any. */

	ec = _decb_gs_sector(path, 17, 17, sector);

	if (ec == 0 && sector[0] != '\xFF')
	{
		printf("%s\n", sector);
	}


	/* 5. Read and print each directory entry */

	/* Sizes of the files come from the FAT. */

	ec = path->fat_ec;

	while (ec == 0 && (ec = _decb_readdir(path, &de)) == 0)
	{
		char asciiflag;
		int granule_size = 1, i;
		int curr_granule;


		if (de.filename[0] == 0 || de.filename[0] == 255)
		{
			continue;
		}


		switch (de.ascii_flag)
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

		curr_granule = de.first_granule;

		while (path->FAT[curr_granule] < 0xC0)
		{
			curr_granule = path->FAT[curr_granule];

			granule_size++;
		}

		/* print escaped filename */

		for (i = 0; i < 8; i++)
		{
			if (isprint((unsigned char)de.filename[i]))
			{
				putchar(de.filename[i]);
			}
			else
			{
				printf("\\%o", de.filename[i]);
			}
		}

		putchar(' ');

		/* print escaped extension */

		for (i = 0; i < 3; i++)
		{
			if (isprint((unsigned char)de.file_extension[i]))
			{
				putchar(de.file_extension[i]);
			}
			else
			{
				printf("\\%o", de.file_extension[i]);
			}
		}

		printf("  %1.1d  %c  %d\n", de.file_type, asciiflag,
		       granule_size);
	}

	/* return success if reached end of directory */

	if (ec == EOS_PNNF)
		ec = 0;

	return ec;
}

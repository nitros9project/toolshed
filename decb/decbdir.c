/********************************************************************
 * decbdir.c - Directory utility for decb
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

		_coco_print_other_filesystems(p, COCO_FS_DECB);
	}


	return 0;
}



/* Print bytes, escaping what can't be printed. */
static void print_escaped(const u_char *text, int length)
{
	int i;

	for (i = 0; i < length; i++)
	{
		if (isprint(text[i]))
		{
			putchar(text[i]);
		}
		else
		{
			printf("\\%o", text[i]);
		}
	}
}


/* Entries that can't be sized don't stop the rest being listed.  The first
   error is returned once all that can be reported has been. */
static int do_dir(char **argv, char *p)
{
	error_code ec = 0;
	error_code first_error = 0;
	char decbpathlist[256];
	u_char sector[256];
	decb_path_id path;
	decb_dir_entry de;
	char *colon;
	int length;


	/* 1. If a comma isn't present in the string, then add it so that the path is opened as a non-native path. */

	if (strchr(p, ',') != NULL)
	{
		length = snprintf(decbpathlist, sizeof(decbpathlist), "%s", p);
	}
	else if ((colon = strchr(p, ':')) == NULL)
	{
		length = snprintf(decbpathlist, sizeof(decbpathlist), "%s,", p);
	}
	else
	{
		length = snprintf(decbpathlist, sizeof(decbpathlist), "%.*s,%s",
				  (int) (colon - p), p, colon);
	}

	if (length < 0 || length >= (int) sizeof(decbpathlist))
	{
		fprintf(stderr, "%s: pathname too long '%s'\n", argv[0], p);

		return EOS_BPNAM;
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


	/* 4. Obtain HDB-DOS disk name, if any.  Nothing promises it ends. */

	ec = _decb_gs_sector(path, 17, 17, (char *) sector);

	if (ec == 0 && sector[0] != 0xFF)
	{
		print_escaped(sector, (int) strnlen((char *) sector, sizeof(sector)));
		putchar('\n');
	}


	/* 5. Read and print each directory entry.  _decb_readdir moves on even
	 *    when a sector can't be read, and stops after the last of the 72
	 *    entries, so this always ends.
	 */

	while ((ec = _decb_readdir(path, &de)) != EOS_PNNF)
	{
		char asciiflag;
		u_int granules, last;
		error_code chain_ec;

		if (ec != 0)
		{
			if (first_error == 0)
			{
				first_error = ec;
			}
			continue;
		}

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

		print_escaped(de.filename, 8);
		putchar(' ');
		print_escaped(de.file_extension, 3);

		/* Sizes of the files come from the FAT, whose chains may loop. */

		chain_ec = path->fat_ec;
		if (chain_ec == 0)
		{
			chain_ec = _decb_gs_chain(path, de.first_granule, &granules, &last);
		}

		if (chain_ec != 0)
		{
			printf("  %1.1d  %c  ?\n", de.file_type, asciiflag);

			if (first_error == 0)
			{
				first_error = chain_ec;
			}
		}
		else
		{
			printf("  %1.1d  %c  %u\n", de.file_type, asciiflag,
			       granules);
		}
	}

	/* reached end of directory: success, unless something could not be listed */

	_decb_close(path);

	return first_error;
}

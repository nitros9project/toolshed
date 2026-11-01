/********************************************************************
 * decbattr.c - File attribute utility for Disk BASIC
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


static int do_getattr(char **argv, char *p);
static int do_setattr(char **argv, char *p, int attrSetMask,
		      int attrResetMask, int quiet);
static void show_attr(decb_file_stat * fstat);


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: attr {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display or modify file attributes.\n",
	"Options:\n",
	"     -0        BASIC program\n",
	"     -1        BASIC data file\n",
	"     -2        Machine-language program\n",
	"     -3        Text file\n",
	"     -a        ASCII file\n",
	"     -b        Binary file\n",
	NULL
};


int decbattr(int argc, char *argv[])
{
	char *p = NULL;
	int i;
	int file_type = -1;
	int data_type = -1;
	int quiet = 0;
	error_code ec;
	error_code first_error = 0;


	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '0':
					file_type = 0;
					break;
				case '1':
					file_type = 1;
					break;
				case '2':
					file_type = 2;
					break;
				case '3':
					file_type = 3;
					break;
				case 'a':
					data_type = 255;
					break;
				case 'b':
					data_type = 0;
					break;
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

			if (file_type == -1 && data_type == -1)
			{
				/* No options were specified */

				ec = do_getattr(argv, p);
			}
			else
			{
				/* Options were specified */

				ec = do_setattr(argv, p, file_type, data_type,
						quiet);
			}
			/* Keep failures visible even if a later file succeeds. */
			if (ec != 0 && first_error == 0)
			{
				first_error = ec;
			}
			if (quiet == 0)
			{
				printf("\n");
			}
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (first_error);
}


static int do_getattr(char **argv, char *p)
{
	error_code ec = 0;
	decb_path_id path;

	/* open a path to the device */

	ec = _decb_open(&path, p, FAM_READ);

	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n", argv[0], ec,
			p);
		return (ec);
	}

	{
		decb_file_stat fdbuf;


		_decb_gs_fd(path, &fdbuf);

		show_attr(&fdbuf);
	}

	_decb_close(path);

	return (0);
}


static int do_setattr(char **argv, char *p, int file_type, int data_type,
		      int quiet)
{
	error_code ec = 0;
	decb_path_id path;

	/* open a path to the device */
	ec = _decb_open(&path, p, FAM_WRITE);

	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n", argv[0], ec,
			p);
		return (ec);
	}

	{
		decb_file_stat fdbuf;

		ec = _decb_gs_fd(path, &fdbuf);

		if (file_type != -1)
		{
			fdbuf.file_type = file_type;
		}
		if (data_type != -1)
		{
			fdbuf.data_type = data_type;
		}

		ec = _decb_ss_fd(path, &fdbuf);

		if (quiet == 0)
		{
			show_attr(&fdbuf);
		}
	}

	_decb_close(path);

	return (0);
}


static void show_attr(decb_file_stat * fstat)
{
	char *file_type = "???";
	char *data_type = "???";


	switch (fstat->file_type)
	{
	case 0:
		file_type = "BASIC program";
		break;

	case 1:
		file_type = "BASIC data file";
		break;

	case 2:
		file_type = "Machine-language program";
		break;

	case 3:
		file_type = "Text editor file";
		break;
	}


	switch (fstat->data_type)
	{
	case 255:
		data_type = "ASCII";
		break;

	case 0:
		data_type = "Binary";
		break;
	}


	printf("File type:  %s (%d)\n", file_type, fstat->file_type);
	printf("Data type:  %s (%d)\n", data_type, fstat->data_type);


	return;
}

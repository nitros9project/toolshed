/********************************************************************
 * decbfstat.c - File status utility for Disk BASIC
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cocotypes.h>
#include <decbpath.h>


static int do_fstat(char **argv, char *p);

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: fstat {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display detailed information about a file.\n",
	"Options:\n",
	NULL
};


int decbfstat(int argc, char *argv[])
{
	char *p = NULL;
	int i;


	/* 1. Walk command line for options. */

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


	/* 2. Walk command line for pathnames. */

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
	decb_path_id path;
	decb_dir_entry dentry;
	u_int curr_granule, size, remaining_bytes = 0;
	int sectors_in_last_granule;


	/* 1. Open a path to the device. */

	ec = _decb_open(&path, p, FAM_READ);

	if (ec != 0)
	{
		ec = _decb_open(&path, p, FAM_DIR | FAM_READ);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);

			return ec;
		}
	}

	if (path->fat_ec != 0)
	{
		ec = path->fat_ec;

		fprintf(stderr, "%s: error %d reading the FAT of '%s'\n",
			argv[0], ec, p);

		_decb_close(path);

		return ec;
	}


	/* 2. Read the directory entry. */

	if (_decb_readdir(path, &dentry) == 0)
	{
		printf("File Information for %s\n", p);
		printf("  File type          : ");

		switch (dentry.file_type)
		{
		case 0:
			printf("BASIC Program\n");
			break;

		case 1:
			printf("Data\n");
			break;

		case 2:
			printf("M/L Program\n");
			break;

		case 3:
			printf("Text Editor Source\n");
			break;

		default:
			printf("???\n");
			break;
		}

		printf("  Data type          : ");

		switch (dentry.ascii_flag)
		{
		case 0:
			printf("Binary\n");
			break;

		case 255:
			printf("ASCII\n");
			break;

		default:
			printf("???\n");
			break;
		}

		_decb_gs_size(path, &size);

		printf("  File size          : %d bytes\n", size);

		printf("  First granule      : %d\n",
		       path->dir_entry.first_granule);

		printf("  Last sector        : %d bytes\n",
		       int2(path->dir_entry.last_sector_size));

		printf("  FAT chain          : ");

		curr_granule = dentry.first_granule;

		while (path->FAT[curr_granule] < 0xC0)
		{
			printf("[%d:2304] ", curr_granule);

			curr_granule = path->FAT[curr_granule];
		}

		sectors_in_last_granule =
			(path->FAT[curr_granule] & 0x3f) - 1;
		sectors_in_last_granule =
			sectors_in_last_granule <
			0 ? 0 : sectors_in_last_granule;

		remaining_bytes =
			(sectors_in_last_granule) * 256 +
			int2(path->dir_entry.last_sector_size);

		printf("[%d:%d] ", curr_granule, remaining_bytes);
	}


	_decb_close(path);


	return ec;
}

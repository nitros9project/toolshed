/********************************************************************
 * cflexfstat.c - File status utility for FLEX
 ********************************************************************/
#include <stdlib.h>
#include <util.h>
#include <flexpath.h>

#include "cflex.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: fstat {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display detailed information about a file.\n",
	"        A file made of the records of a binary file has its addresses shown.\n",
	"Options:\n",
	NULL
};


/* Show what a binary file loads.  Being unable to read the file doesn't stop
   the rest of its details mattering. */
static void print_binary(flex_path_id path, u_int size)
{
	u_char *buffer = malloc(size ? size : 1);
	u_int low, high, transfer, records;
	int has_transfer;
	error_code ec;

	if (buffer == NULL)
	{
		return;
	}

	ec = _flex_read(path, buffer, &size);
	if (ec != 0 && ec != EOS_EOF)
	{
		printf("  Binary             : Unreadable\n");
	}
	else if (_flex_binary_info(buffer, size, &low, &high, &transfer, &has_transfer, &records) == 0)
	{
		printf("  Binary records     : %u\n", records);
		if (low <= high)
		{
			printf("  Load addresses     : $%04X to $%04X\n", low, high);
		}
		if (has_transfer)
		{
			printf("  Transfer address   : $%04X\n", transfer);
		}
		else
		{
			printf("  Transfer address   : None\n");
		}
	}
	free(buffer);
}


static error_code do_fstat(char *file)
{
	flex_path_id path;
	flex_file_stat stat;
	char name[13];
	error_code ec = _flex_open(&path, file, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}

	ec = _flex_gs_fd(path, &stat);
	if (ec == 0)
	{
		ec = _flex_ncpy_name(&path->entry, name, sizeof(name));
	}
	if (ec != 0)
	{
		_flex_close(path);
		return ec;
	}

	printf("File Information for %s\n", file);
	printf("  Filename           : %s\n", name);
	printf("  Write protected    : %s\n", (stat.attributes & FLEX_ATTR_WRITE) ? "Yes" : "No");
	printf("  Delete protected   : %s\n", (stat.attributes & FLEX_ATTR_DELETE) ? "Yes" : "No");
	printf("  Read protected     : %s\n", (stat.attributes & FLEX_ATTR_READ) ? "Yes" : "No");
	printf("  Catalog protected  : %s\n", (stat.attributes & FLEX_ATTR_CATALOG) ? "Yes" : "No");
	printf("  File type          : %s\n", stat.random ? "Random" : "Sequential");
	printf("  Date               : %02u/%02u/%02u\n", stat.month, stat.day, stat.year);
	printf("  Sectors            : %u\n", stat.sectors);
	printf("  Data size          : %u bytes\n", stat.file_size);
	printf("  First sector       : track %u, sector %u\n", stat.start_track, stat.start_sector);
	printf("  Last sector        : track %u, sector %u\n", stat.end_track, stat.end_sector);
	print_binary(path, stat.file_size);

	_flex_close(path);
	return 0;
}


int cflexfstat(int argc, char *argv[])
{
	error_code ec = 0;
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
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (CFLEX_USAGE_ERROR);
				}
			}
		}
	}

	/* 2. Walk command line for pathnames. */

	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = do_fstat(p);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d with '%s'\n", argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

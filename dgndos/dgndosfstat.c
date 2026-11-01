/********************************************************************
 * dgndosfstat.c - File status utility for Dragon DOS
 ********************************************************************/
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


#define FILE_HEADER_SIZE	9
#define FILE_HEADER_START	0x55
#define FILE_HEADER_END		0xAA


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: fstat {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display detailed information about a file.\n",
	"Options:\n",
	NULL
};


/* Print the extents of an entry as [LSN:sectors]. */
static void print_extents(const dragondos_extent *extent, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		if (extent[i].count != 0)
		{
			printf("[%u:%u] ", (extent[i].lsn[0] << 8) | extent[i].lsn[1], extent[i].count);
		}
	}
}


/* Print the header SAVE puts at the start of a file, if it has one.  Being
   unable to read it doesn't stop the rest of the file's details mattering. */
static void print_file_header(dragondos_path_id path)
{
	u_char header[FILE_HEADER_SIZE];
	u_int size = sizeof(header);
	error_code ec = _dragondos_read(path, header, &size);

	if (ec != 0 && ec != EOS_EOF)
	{
		printf("  File header        : Unreadable\n");
		return;
	}

	if (ec == EOS_EOF || size != sizeof(header) || header[0] != FILE_HEADER_START || header[8] != FILE_HEADER_END)
	{
		printf("  File header        : None\n");
		return;
	}

	printf("  File type          : %u (%s)\n", header[1],
	       header[1] == 1 ? "BASIC program" : header[1] == 2 ? "Machine code" : "Unknown");
	printf("  Load address       : $%04X\n", (header[2] << 8) | header[3]);
	printf("  Data length        : %u bytes\n", (header[4] << 8) | header[5]);
	printf("  Exec address       : $%04X\n", (header[6] << 8) | header[7]);
}


static error_code do_fstat(char *file)
{
	dragondos_path_id path;
	dragondos_file_stat stat;
	dragondos_dir_entry entry;
	u_int index;
	int entries = 0;
	char name[13];
	error_code ec = _dragondos_open(&path, file, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}

	index = path->entry_index;
	ec = _dragondos_gs_fd(path, &stat);
	if (ec == 0)
	{
		ec = _dragondos_seekdir(path, index);
	}
	if (ec == 0)
	{
		ec = _dragondos_readdir(path, &entry);
	}
	if (ec == 0)
	{
		ec = _dragondos_ncpy_name(&entry, name, sizeof(name));
	}
	if (ec != 0)
	{
		_dragondos_close(path);
		return ec;
	}

	printf("File Information for %s\n", file);
	printf("  Filename           : %s\n", name);
	printf("  Protected          : %s\n", stat.protected_file ? "Yes" : "No");
	printf("  File size          : %u bytes\n", stat.file_size);
	printf("  First entry        : %u\n", index);
	printf("  Extents            : ");
	print_extents(entry.u.header.extent, 4);

	/* Directory entries chained to the first are limited to the most there are. */
	for (entries = 1; (entry.flags & DRAGONDOS_FLAG_CONTINUED) && entries < DRAGONDOS_MAX_DIR_ENTRIES; entries++)
	{
		ec = _dragondos_seekdir(path, entry.last_or_next);
		if (ec == 0)
		{
			ec = _dragondos_readdir(path, &entry);
		}
		if (ec != 0)
		{
			break;
		}
		print_extents(entry.u.continuation.extent, 7);
	}
	printf("\n  Directory entries  : %d\n", entries);
	print_file_header(path);

	_dragondos_close(path);
	return ec;
}


int dgndosfstat(int argc, char *argv[])
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
					return (DGNDOS_USAGE_ERROR);
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

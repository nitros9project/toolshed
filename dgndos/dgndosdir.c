/********************************************************************
 * dgndosdir.c - Directory utility for Dragon DOS
 ********************************************************************/
#include <ctype.h>
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


/* Help Message */
static char const *const helpMessage[] = {
	"Syntax: dir {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Display the contents of a disk's directory.\n",
	"Options:\n",
	NULL
};


/* Print part of a filename, nul filled, escaping what can't be printed. */
static void print_field(const u_char *field, int length)
{
	int i;

	for (i = 0; i < length; i++)
	{
		if (field[i] == 0)
		{
			putchar(' ');
		}
		else if (isprint(field[i]))
		{
			putchar(field[i]);
		}
		else
		{
			printf("\\%o", field[i]);
		}
	}
}


/* Entries that can't be read don't stop the rest being listed.  The first
   error is returned once all that can be reported has been. */
static error_code do_dir(char *disk)
{
	char pathlist[1024];
	dragondos_path_id path;
	dragondos_dir_entry entry;
	error_code first_error = 0;
	error_code ec;
	u_int free_sectors;
	int files = 0;

	ec = dgndos_disk_pathlist(disk, pathlist, sizeof(pathlist));
	if (ec == 0)
	{
		ec = _dragondos_open(&path, pathlist, FAM_READ);
	}
	if (ec != 0)
	{
		return ec;
	}

	printf("Directory of: %s\n\n", pathlist);

	while ((ec = _dragondos_readdir(path, &entry)) != EOS_EOF)
	{
		u_int size;

		if (ec != 0)
		{
			if (first_error == 0)
			{
				first_error = ec;
			}
			continue;
		}

		if (entry.flags & (DRAGONDOS_FLAG_DELETED | DRAGONDOS_FLAG_CONTINUATION))
		{
			continue;
		}

		print_field(entry.u.header.filename, 8);
		putchar(' ');
		print_field(entry.u.header.file_extension, 3);
		printf("  %c  ", (entry.flags & DRAGONDOS_FLAG_PROTECTED) ? 'P' : '.');

		ec = _dragondos_gs_size_entry(path, path->directory_entry_index - 1, &size);
		if (ec == 0)
		{
			printf("%7u\n", size);
		}
		else
		{
			printf("      ?\n");
			if (first_error == 0)
			{
				first_error = ec;
			}
		}
		files++;
	}

	ec = _dragondos_gs_free(path, &free_sectors);
	if (ec == 0)
	{
		printf("\n%d file%s, %u sectors free (%u bytes)\n", files, files == 1 ? "" : "s",
		       free_sectors, free_sectors * DRAGONDOS_SECTOR_SIZE);
	}
	else if (first_error == 0)
	{
		first_error = ec;
	}

	_dragondos_close(path);
	return first_error;
}


int dgndosdir(int argc, char *argv[])
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
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (DGNDOS_USAGE_ERROR);
				}
			}
		}
	}

	/* walk command line for pathnames */
	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = do_dir(p);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d with '%s'\n", argv[0], ec, p);
			return (ec);
		}

		_coco_print_other_filesystems(p, COCO_FS_DRAGONDOS);
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

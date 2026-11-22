/********************************************************************
 * flexdir.c - Directory utility for FLEX
 ********************************************************************/
#include <util.h>
#include <flexpath.h>

#include "flex.h"


/* Help Message */
static char const *const helpMessage[] = {
	"Syntax: dir {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Display the contents of a disk's directory.\n",
	"        Attributes are W write, D delete, R read and C catalog protected,\n",
	"        and a random file is marked with R after them.\n",
	"Options:\n",
	NULL
};


static void print_attributes(u_int attributes, int random)
{
	printf("%c%c%c%c %c",
	       (attributes & FLEX_ATTR_WRITE) ? 'W' : '.',
	       (attributes & FLEX_ATTR_DELETE) ? 'D' : '.',
	       (attributes & FLEX_ATTR_READ) ? 'R' : '.',
	       (attributes & FLEX_ATTR_CATALOG) ? 'C' : '.',
	       random ? 'R' : ' ');
}


/* Entries that can't be read don't stop the rest being listed.  The first
   error is returned once all that can be reported has been. */
static error_code do_dir(char *disk)
{
	char pathlist[1024];
	flex_path_id path;
	flex_dir_entry entry;
	flex_sir sir;
	error_code first_error = 0;
	error_code ec;
	u_int free_sectors;
	u_int used = 0;
	int files = 0;

	ec = flex_disk_pathlist(disk, pathlist, sizeof(pathlist));
	if (ec == 0)
	{
		ec = _flex_open(&path, pathlist, FAM_READ);
	}
	if (ec != 0)
	{
		return ec;
	}

	_flex_gs_sir(path, &sir);
	printf("Directory of: %s\n\n", pathlist);
	printf("Volume: ");
	flex_print_field((const u_char *)sir.volume_name, 8, 0);
	if (sir.volume_name[8] != '\0')
	{
		putchar('.');
		flex_print_field((const u_char *)&sir.volume_name[8], 3, 0);
	}
	printf("  Number: %u  Created: %02u/%02u/%02u\n\n", sir.volume_number, sir.month, sir.day, sir.year);

	while ((ec = _flex_readdir(path, &entry)) != EOS_EOF)
	{
		u_int sectors;

		if (ec != 0)
		{
			if (first_error == 0)
			{
				first_error = ec;
			}
			break;
		}

		if (!_flex_entry_in_use(&entry))
		{
			continue;
		}

		sectors = _flex_entry_sectors(&entry);
		flex_print_field(entry.filename, 8, 1);
		putchar(' ');
		flex_print_field(entry.file_extension, 3, 1);
		printf("  %5u  ", sectors);
		print_attributes(entry.attributes, entry.random);
		printf("  %02u/%02u/%02u\n", entry.month, entry.day, entry.year);
		used += sectors;
		files++;
	}

	ec = _flex_gs_free(path, &free_sectors);
	if (ec == 0)
	{
		printf("\n%d file%s, %u sectors used, %u sectors free (%u bytes)\n", files, files == 1 ? "" : "s",
		       used, free_sectors, free_sectors * FLEX_DATA_SIZE);
	}
	else if (first_error == 0)
	{
		first_error = ec;
	}

	_flex_close(path);
	return first_error;
}


int flexdir(int argc, char *argv[])
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
					return (FLEX_USAGE_ERROR);
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

		_coco_print_other_filesystems(p, COCO_FS_FLEX);
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

/********************************************************************
 * cflexgen.c - Make a FLEX disk bootable on the Color Computer
 ********************************************************************/
#include <string.h>
#include <toolshed.h>
#include <util.h>
#include <flexpath.h>

#include "cflex.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: gen {[<opts>]} <disk> {<file>} {[<opts>]}\n",
	"Usage:  Make a disk boot FLEX on the Color Computer, with RUN \"FLEX\" in\n",
	"        Disk BASIC, as PUTLDR and LINK do.  A file from the host, such as\n",
	"        FLEX.SYS, is copied to the disk first, replacing one there.\n",
	"Options:\n",
	"     -n<name> = the file on the disk to load (default FLEX.SYS)\n",
	NULL
};


static error_code do_gen(const char *command, char *disk, char *file, const char *name)
{
	static char buffer[32768];
	char pathlist[1024];
	char target[1024];
	int length;
	error_code ec = cflex_disk_pathlist(disk, pathlist, sizeof(pathlist));

	if (ec != 0)
	{
		return ec;
	}
	length = snprintf(target, sizeof(target), "%s%s", pathlist, name);
	if (length < 0 || (size_t) length >= sizeof(target))
	{
		return EOS_BPNAM;
	}

	ec = _flex_putldr(pathlist);
	if (ec == EOS_IA)
	{
		fprintf(stderr, "%s: '%s' can't boot FLEX: it must be double density, with at least 19 tracks. "
			"Make one with dskinit -d.\n", command, disk);
		return CFLEX_USAGE_ERROR;
	}
	if (ec == EOS_FNA)
	{
		fprintf(stderr, "%s: '%s' has files in the sectors a boot disk needs, track 16 sectors 1 to 9 "
			"and track 17 sectors 2 and 3. Use a new disk.\n", command, disk);
		return CFLEX_USAGE_ERROR;
	}
	if (ec != 0)
	{
		return ec;
	}

	if (file != NULL)
	{
		ec = TSCopyFile(file, target, 0, 1, 0, 0, buffer, sizeof(buffer));
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d copying '%s'\n", command, ec, file);
			return ec;
		}
	}

	ec = _flex_link(target);
	if (ec == EOS_IA)
	{
		fprintf(stderr, "%s: the loader can't read all of '%s': it reads only side 0 of tracks past 0. "
			"Put it on a new single sided disk first.\n", command, target);
		return CFLEX_USAGE_ERROR;
	}
	return ec;
}


int cflexgen(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	char *disk = NULL, *file = NULL;
	const char *name = "FLEX.SYS";
	int i;

	/* walk command line for options and pathnames */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'n':
					name = p + 1;
					p += strlen(p) - 1;
					break;

				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (CFLEX_USAGE_ERROR);
				}
			}
		}
		else if (disk == NULL)
		{
			disk = argv[i];
		}
		else if (file == NULL)
		{
			file = argv[i];
		}
		else
		{
			fprintf(stderr, "%s: too many arguments\n", argv[0]);
			return (CFLEX_USAGE_ERROR);
		}
	}

	if (disk == NULL)
	{
		show_help(helpMessage);
		return (0);
	}
	if (name[0] == '\0')
	{
		fprintf(stderr, "%s: -n needs a filename\n", argv[0]);
		return (CFLEX_USAGE_ERROR);
	}

	ec = do_gen(argv[0], disk, file, name);
	if (ec != 0 && ec != CFLEX_USAGE_ERROR)
	{
		fprintf(stderr, "%s: error %d with '%s'\n", argv[0], ec, disk);
	}

	return (ec);
}

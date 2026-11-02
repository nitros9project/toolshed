/********************************************************************
 * dgndosdskinit.c - Dragon DOS Format Utility
 ********************************************************************/
#include <string.h>
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dskinit {[<opts>]} <disk> {[<...>]} {[<opts>]}\n",
	"Usage:  Create a Dragon DOS image.\n",
	"Options:\n",
	"     -t<num>  = tracks, 21 to 80 (default 40)\n",
	"     -s<num>  = sides, 1 or 2 (default 1)\n",
	"     -f<name> = image format: raw, jvc, vdk, dmk or sdf\n",
	"                (default from the disk's extension, otherwise raw)\n",
	NULL
};


static diskimage_format format_from_extension(const char *disk)
{
	diskimage_format format = _diskimage_format_of_extension(disk);

	return format == DISKIMAGE_UNKNOWN ? DISKIMAGE_RAW : format;
}


int dgndosdskinit(int argc, char **argv)
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int tracks = 40, sides = 1;
	diskimage_format format = DISKIMAGE_UNKNOWN;

	/* 1. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 't':
					tracks = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 's':
					sides = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'f':
					format = _diskimage_format_named(p + 1);
					if (format == DISKIMAGE_UNKNOWN)
					{
						fprintf(stderr, "%s: unknown format '%s'\n", argv[0], p + 1);
						return (DGNDOS_USAGE_ERROR);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

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

	/* 2. Walk command line for pathnames. */

	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = _dragondos_dskinit(p, format == DISKIMAGE_UNKNOWN ? format_from_extension(p) : format,
					(u_int) tracks, (u_int) sides);
		if (ec == EOS_IA)
		{
			fprintf(stderr, "%s: '%s' can't have %d tracks and %d sides in this format\n", argv[0], p, tracks, sides);
			return (DGNDOS_USAGE_ERROR);
		}
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d creating '%s'\n", argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

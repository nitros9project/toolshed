/********************************************************************
 * flexdskinit.c - FLEX Format Utility
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <flexpath.h>

#include "flex.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dskinit {[<opts>]} <disk> {[<...>]} {[<opts>]}\n",
	"Usage:  Create a FLEX image, as NEWDISK does.\n",
	"Options:\n",
	"     -t<num>  = tracks, 2 to 256 (default 35)\n",
	"     -s<num>  = sides, 1 or 2 (default 1)\n",
	"     -d       = double density, 18 sectors a side instead of 10.\n",
	"                Track 0 is single density, as the Color Computer needs.\n",
	"     -p       = pad track 0 of a raw image to as many sectors as the others\n",
	"     -n<name> = volume name, as a filename: up to 8 characters, and an extension\n",
	"     -v<num>  = volume number (default 1)\n",
	"     -f<name> = image format: raw, jvc, vdk, dmk or sdf\n",
	"                (default from the disk's extension, otherwise raw)\n",
	"     A DMK or SDF image records track 0 as single density.  Other\n",
	"     images can't, and JVC and VDK images are always padded.\n",
	NULL
};


static diskimage_format format_from_extension(const char *disk)
{
	diskimage_format format = _diskimage_format_of_extension(disk);

	return format == DISKIMAGE_UNKNOWN ? DISKIMAGE_RAW : format;
}


/* The rest of an option, which ends the argument. */
static char *option_value(char **p)
{
	char *value = *p + 1;

	*p += strlen(*p) - 1;
	return value;
}


int flexdskinit(int argc, char **argv)
{
	error_code ec = 0;
	char *p = NULL;
	char *volume_name = NULL;
	int i;
	int tracks = 35, sides = 1, double_density = 0, padded = 0;
	long volume_number = 1;
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
					tracks = atoi(option_value(&p));
					break;

				case 's':
					sides = atoi(option_value(&p));
					break;

				case 'd':
					double_density = 1;
					break;

				case 'p':
					padded = 1;
					break;

				case 'n':
					volume_name = option_value(&p);
					break;

				case 'v':
					volume_number = strtol(option_value(&p), NULL, 0);
					break;

				case 'f':
				{
					char *name = option_value(&p);

					format = _diskimage_format_named(name);
					if (format == DISKIMAGE_UNKNOWN)
					{
						fprintf(stderr, "%s: unknown format '%s'\n", argv[0], name);
						return (FLEX_USAGE_ERROR);
					}
					break;
				}

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

	if (volume_number < 0 || volume_number > 0xFFFF)
	{
		fprintf(stderr, "%s: the volume number must be 0 to 65535\n", argv[0]);
		return (FLEX_USAGE_ERROR);
	}
	if (tracks < 2 || tracks > 256 || (sides != 1 && sides != 2))
	{
		fprintf(stderr, "%s: a disk has 2 to 256 tracks and 1 or 2 sides\n", argv[0]);
		return (FLEX_USAGE_ERROR);
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
		ec = _flex_dskinit(p, format == DISKIMAGE_UNKNOWN ? format_from_extension(p) : format,
				   (u_int) tracks, (u_int) sides, double_density, padded, volume_name,
				   (u_int) volume_number);
		if (ec == EOS_BPNAM)
		{
			fprintf(stderr, "%s: '%s' is not a volume name\n", argv[0], volume_name);
			return (FLEX_USAGE_ERROR);
		}
		if (ec == EOS_IA)
		{
			fprintf(stderr, "%s: '%s' can't have %d tracks, %d sides and %s density in this format\n",
				argv[0], p, tracks, sides, double_density ? "double" : "single");
			return (FLEX_USAGE_ERROR);
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

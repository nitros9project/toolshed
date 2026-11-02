/********************************************************************
 * decbdskini.c - Disk BASIC Format Utility
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <decbpath.h>
#include <nativepath.h>
#include <cocotypes.h>


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dskini {[<opts>]} <disk> {[<...>]} {[<opts>]}\n",
	"Usage:  Create a Disk BASIC image.\n",
	"Options:\n",
	"     -3       = 35 track disk (default)\n",
	"     -4       = 40 track disk\n",
	"     -8       = 80 track disk\n",
	"     -f<name> = image format: raw, jvc, vdk, dmk or sdf\n",
	"                (default from the disk's extension, otherwise raw)\n",
	"     -h<num>  = create <num> HDB-DOS drives\n",
	"     -n<name> = HDB-DOS disk name\n",
	"     -s       = create a \"skitzo\" disk\n",
	NULL
};


int decbdskini(int argc, char **argv)
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int tracks = 35;
	char *diskName = NULL;
	int hdbdrives = 1;
	int skitzo = 0;
	diskimage_format format = DISKIMAGE_UNKNOWN;

	/* 1. If no arguments, show help and return. */

	if (argv[1] == NULL)
	{
		show_help(helpMessage);

		return (0);
	}


	/* 2. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '3':	/* 35 tracks */
					tracks = 35;
					break;

				case '4':	/* 40 tracks */
					tracks = 40;
					break;

				case '8':	/* 80 tracks */
					tracks = 80;
					break;

				case 'f':	/* image format */
					format = _diskimage_format_named(p + 1);
					if (format == DISKIMAGE_UNKNOWN)
					{
						fprintf(stderr, "%s: unknown format '%s'\n", argv[0], p + 1);
						return (1);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'h':	/* HDB-DOS drives */
					hdbdrives = atoi(p + 1);
					tracks = 35;
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'n':	/* disk name */
					diskName = p + 1;
					while (*(p + 1) != '\0')
						p++;
					break;

				case 's':	/* skitzo disk */
					skitzo = 1;
					break;

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


	/* 3. Walk command line for pathnames. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			diskimage_format disk_format = format == DISKIMAGE_UNKNOWN ?
				_diskimage_format_of_extension(argv[i]) : format;

			ec = _decb_dskini_format(argv[i], disk_format, tracks, diskName,
						 hdbdrives, 256, skitzo);

			if (ec == EOS_FAE)
			{
				fprintf(stderr, "%s: '%s' exists and is not that format, so it is left alone\n",
					argv[0], argv[i]);
			}
			else if (ec == EOS_BPNAM && hdbdrives > 1)
			{
				fprintf(stderr, "%s: only a raw image can hold many drives, so '%s' can't\n",
					argv[0], argv[i]);
			}
			else if (ec == EOS_IA)
			{
				fprintf(stderr, "%s: '%s' can't have %d tracks in this format\n", argv[0], argv[i], tracks);
			}

			if (ec != 0)
			{
				return (ec);
			}
		}
	}

	return (ec);
}

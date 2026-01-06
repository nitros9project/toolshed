/********************************************************************
 * os9format.c - OS-9 disk format utility
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>

#include "cocotypes.h"
#include "cocosys.h"

#include "os9path.h"
#include "nativepath.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: format {[<opts>]} <disk> {[<...>]} {[<opts>]}\n",
	"Usage:  Create a disk image of a given size.\n",
	"Options:\n",
	"     -bsX = bytes per sector (default = 256)\n",
	"     -cX  = cluster size\n",
	"     -e   = format entire disk (make full sized image)\n",
	"     -k   = make OS-9/68K LSN0\n",
	"     -nX  = disk name\n",
	"     -q   = quiet; do not report format summary\n",
	" Floppy Options:\n",
	"     -4   = 48 tpi (default)\n",
	"     -9   = 96 tpi\n",
	"     -sa  = sector allocation size (SAS)\n",
	"     -sd  = single density\n",
	"     -dd  = double density (default)\n",
	"     -ss  = single sided (default)\n",
	"     -ds  = double sided\n",
	"     -tX  = tracks (default = 35)\n",
	"     -stX = sectors per track (default = 18)\n",
	"     -szX = sectors for track 0 (default = 18)\n",
	"     -iX  = interleave (default = 3)\n",
	"     -dr  = format a Dragon disk\n",
	" Hard Drive Options:\n",
	"     -lX  = number of logical sectors (floppy options ignored)\n",
	NULL
};

#define DEF_SECTORS_TRACK	18
#define DEF_INTERLEAVE		 3

int os9format(int argc, char **argv)
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int tracks = 35;
	int heads = 1;
	int sectorsPerTrack = DEF_SECTORS_TRACK;
	int sectorsTrack0 = 0;   /* mark as unset */
	int bytesPerSector = 256;
	int tpi = 48;		/* 48 tpi */
	int density = 1;	/* double density */
	int logicalSectors = 0;
	char *diskName = "CoCo Disk";
	int quiet = 0;		/* assume chatter */
	int clusterSize = 0;	/* assume no cluster size */
	int sectorAllocationSize = 8;	/* default */
	int interleave = DEF_INTERLEAVE;
	int os968k = 0;		/* assume OS-9/6809 LSN0 */
	int formatEntire = 0;	/* format entire disk image */
	int isDragon = 0;	/* format disk as Dragon, with reserved boot sectors at begining */
	int isHDD = 0;		/* Is this image for a hard drive */

	/* if no arguments, show help and return */
	if (argv[1] == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '4':	/* 48 tpi */
					tpi = 48;
					break;

				case '9':	/* 96 tpi */
					tpi = 96;
					break;

				case 'c':
					clusterSize = atoi(p + 1);
					if (clusterSize == 0
					    || (clusterSize &
						(clusterSize - 1)))
					{
						fprintf(stderr,
							"%s: cluster size must be a power of two\n",
							argv[0]);
						return 0;
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'e':
					formatEntire = 1;
					break;

				case 'k':
					os968k = 1;
					break;

				case 'q':
					quiet = 1;
					break;

				case 'b':	/* bytes/sector */
					switch (*(p + 1))
					{
					case 's':
						bytesPerSector = atoi(p + 2);
						if (bytesPerSector % 256 != 0)
						{
							fprintf(stderr,
								"%s: bytes per sector must be a multiple of 256\n",
								argv[0]);
							return (0);
						}
						break;

					default:
						fprintf(stderr,
							"%s: unknown option '%c'\n",
							argv[0], *p);
						return (0);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'd':	/* double density or sides */
					switch (*(p + 1))
					{
					case 'd':
						density = 1;
						break;

					case 's':
						heads = 2;
						break;

					case 'r':
						isDragon = 1;
						break;

					default:
						fprintf(stderr,
							"%s: unknown option '%c'\n",
							argv[0], *p);
						return (0);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 's':	/* single density or sides */
					switch (*(p + 1))
					{
					case 'a':
						sectorAllocationSize =
							atoi(p + 2);
						while (*(p + 1) != '\0')
							p++;
						break;

					case 'd':
						density = 0;
						break;

					case 's':
						heads = 1;
						break;

					case 't':
						sectorsPerTrack = atoi(p + 2);
						break;

					case 'z':
						sectorsTrack0 = atoi(p + 2);
						break;

					default:
						fprintf(stderr,
							"%s: unknown option '%c'\n",
							argv[0], *p);
						return (0);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'i':	/* interleave */
					interleave = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 't':	/* tracks */
					tracks = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'n':	/* disk name */
					diskName = p + 1;
					while (*(p + 1) != '\0')
						p++;
					// FIX: chop off disk name if too long
					if (strlen(diskName) > 32)
					{
						diskName[32] = '\0';
					}
					break;

				case 'l':	/* logical sectors */
					logicalSectors = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					isHDD = 1;
					break;

				case '?':
				case 'h':
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

	/* match sectors on track 0 only if it's unset. */
	if (sectorsTrack0 == 0)
	{
		sectorsTrack0 = sectorsPerTrack;
	}

	/* if logicalSectors != 0, then format as a hard drive image */
	if (logicalSectors != 0)
	{
		density = 0;	/* density is irrelavent */
		tpi = 0;	/* tracks per inch is irrelavent */
		tracks = 1;
		heads = 1;
		sectorsPerTrack = 1;

		/* compute c/h/s based on sectors desired */
		for (i = 512; i < 65536; i++)
		{
			if (logicalSectors % i == 0)
			{
				tracks = logicalSectors / i;
				logicalSectors = i;
				i = 65536;
			}
		}

		for (i = 255; i > 0; i--)
		{
			if (logicalSectors % i == 0)
			{
				sectorsPerTrack = logicalSectors / i;
				sectorsTrack0 = sectorsPerTrack;
				heads = i;
				i = 0;
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
			unsigned int totalSectors, totalBytes;

			error_code ec =
				_os9_format(argv[i], os968k, tracks,
					    sectorsPerTrack, sectorsTrack0,
					    heads, bytesPerSector, &clusterSize,
					    diskName, sectorAllocationSize,
					    tpi, density, formatEntire,
					    isDragon, isHDD, interleave,
					    &totalSectors, &totalBytes);

			if (ec == 0)
			{
				/* Print summary */
				if (!quiet)
				{
					printf("Format Summary\n");
					printf("--------------\n");
					printf("Geometry Data:\n");
					printf("      Cylinders: %d\n",
					       tracks);
					printf("          Heads: %d\n",
					       heads);
					printf("  Sectors/track: %d\n",
					       sectorsPerTrack);
					if (sectorsPerTrack != sectorsTrack0)
					{
						printf(" Sectors/track0: %d\n",
								sectorsTrack0);
					}
					printf("    Sector size: %d\n",
					       bytesPerSector);
					printf("\nLogical Data:\n");
					printf("  Total sectors: %u\n",
					       totalSectors);
					printf("  Size in bytes: %u\n",
					       totalBytes);
					printf("   Cluster size: %d\n",
					       clusterSize);
				}
			}

		}
	}

	return (ec);
}

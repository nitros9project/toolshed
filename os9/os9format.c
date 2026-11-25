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
	"     -fX  = image format: raw, jvc, vdk, dmk or sdf\n",
	"            (default from the disk's extension, otherwise raw)\n",
	"     -k   = make OS-9/68K LSN0\n",
	"     -nX  = disk name\n",
	"     -q   = quiet; do not report format summary\n",
	" Floppy Options:\n",
	"     -4   = 48 tpi (default)\n",
	"     -9   = 96 tpi\n",
	"     -saX = sector allocation size (SAS, default = 8)\n",
	"     -sd  = single density\n",
	"     -sd0 = single density on track 0 only, the rest double (Dragon Beta)\n",
	"     -dd  = double density (default)\n",
	"     -ss  = single sided (default)\n",
	"     -ds  = double sided\n",
	"     -tX  = tracks (default = 35)\n",
	"     -stX = sectors per track (default = 18)\n",
	"     -szX = sectors for track 0 (default = 18)\n",
	"     -iX  = interleave (default = 3)\n",
	"     -In[,m] = a track's first sector number, or one for each side of a\n",
	"               double sided disk (default 1, or 0 for -k and a short track 0)\n",
	"     -dr  = format a Dragon disk\n",
	" Hard Drive Options:\n",
	"     -lX  = number of logical sectors (floppy options ignored)\n",
	NULL
};

#define DEF_SECTORS_TRACK	18
#define DEF_INTERLEAVE		 3

/* Read a sector number from text, up to end (or the end of the string). */
static int read_sector_number(const char *text, const char *end, int *value)
{
	char *tail;
	long number = strtol(text, &tail, 10);

	if (tail == text || number < 0 || number > 255)
	{
		return 0;
	}

	if (end != NULL ? tail != end : *tail != '\0')
	{
		return 0;
	}

	*value = (int)number;

	return 1;
}

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
	int firstSector[2] = { -1, -1 };	/* -1 = not given, see the defaults below */
	int perSideNumber = 0;		/* -I was given a number for each side */
	u_int sideFirstSector[2];
	int side;
	int os968k = 0;		/* assume OS-9/6809 LSN0 */
	int track0SingleDensity = 0;	/* track 0 side 0 is FM */
	int formatEntire = 0;	/* format entire disk image */
	int isDragon = 0;	/* format disk as Dragon, with reserved boot sectors at begining */
	int isHDD = 0;		/* Is this image for a hard drive */
	diskimage_format format = DISKIMAGE_UNKNOWN;

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
						return (EOS_IA);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'e':
					formatEntire = 1;
					break;

				case 'f':	/* image format */
					format = _diskimage_format_named(p + 1);
					if (format == DISKIMAGE_UNKNOWN)
					{
						fprintf(stderr,
							"%s: unknown format '%s'\n",
							argv[0], p + 1);
						return (EOS_IA);
					}
					while (*(p + 1) != '\0')
						p++;
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
							return (EOS_IA);
						}
						break;

					default:
						fprintf(stderr,
							"%s: unknown option '%.2s'\n",
							argv[0], p);
						return (EOS_IA);
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
							"%s: unknown option '%.2s'\n",
							argv[0], p);
						return (EOS_IA);
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
						if (*(p + 2) == '0')
						{
							/* Track 0 alone is single density, as a Dragon Beta
							   disk has it; the disk itself stays double. */
							track0SingleDensity = 1;
						}
						else
						{
							density = 0;
						}
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
							"%s: unknown option '%.2s'\n",
							argv[0], p);
						return (EOS_IA);
					}
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'i':	/* interleave */
					interleave = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'I':	/* first sector numbers, one per side */
				{
					char *comma = strchr(p + 1, ',');

					if (!read_sector_number(p + 1, comma, &firstSector[0]) ||
					    (comma != NULL &&
					     !read_sector_number(comma + 1, NULL, &firstSector[1])))
					{
						fprintf(stderr,
							"%s: -I wants a sector number from 0 to 255, or two separated by a comma\n",
							argv[0]);

						return (EOS_SECT);
					}
					if (comma == NULL)
					{
						firstSector[1] = firstSector[0];
					}
					else
					{
						perSideNumber = 1;
					}
					while (*(p + 1) != '\0')
						p++;
					break;
				}

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
					return (EOS_IA);
				}
			}
		}
	}

	/* Native OS-9/68K machines number a track's sectors from 0, as do machines whose
	 * track 0 is short (MM/1, GIMIX, Dragon Beta); the CoCo, Dragon 64 and Dragon
	 * Alpha number from 1.  -I overrides this for 68K machines that differ (BBC
	 * second processor, Atari, Amiga, Macintosh).  Only some formats record the
	 * number, so the default is worked out per image, below. */

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
			diskimage_format disk_format = format;

			if (disk_format == DISKIMAGE_UNKNOWN)
			{
				disk_format = _diskimage_format_of_extension(argv[i]);
			}
			if (disk_format == DISKIMAGE_UNKNOWN)
			{
				disk_format = DISKIMAGE_RAW;
			}

			/* Only DMK, SDF and JVC record a track's first sector number.  The
			 * others number their sectors themselves, so a number they cannot
			 * hold is dropped rather than refused. */
			{
				int records_number = disk_format == DISKIMAGE_DMK ||
						     disk_format == DISKIMAGE_SDF ||
						     disk_format == DISKIMAGE_JVC;
				int defaultFirstSector =
					(os968k || (sectorsTrack0 != 0 && sectorsTrack0 != sectorsPerTrack)) ? 0 : 1;

				/* A number for each side is a double sided disk's parameter: with
				 * one side the second number is for a side the disk does not have,
				 * so the disk asked for cannot be made whatever holds it. */
				if (perSideNumber && heads == 1)
				{
					fprintf(stderr,
						"%s: Invalid initial sector values requested for single sided disk\n",
						argv[0]);

					return (EOS_SECT);
				}

				for (side = 0; side < 2; side++)
				{
					int asked = firstSector[side] >= 0 ? firstSector[side] :
									   defaultFirstSector;

					/* Track 0 is the only short one, so a track's worth of
					 * numbers is sectors per track, on either side. */
					if ((u_int)asked + (u_int)sectorsPerTrack - 1 > 255)
					{
						fprintf(stderr,
							"%s: '%s' would number a track's sectors past 255\n",
							argv[0], argv[i]);

						return (EOS_SECT);
					}

					sideFirstSector[side] = records_number ? (u_int)asked : 1;
				}

				/* A container that means nothing by a sector number leaves it to
				 * whatever reads the image, which is not ours to predict, so the
				 * disk is still made and the request is reported.  A container that
				 * does record one, but has room for only one while two were asked
				 * for, would write a disk that is not the disk asked for, so that is
				 * refused. */
				if (firstSector[0] >= 0 || firstSector[1] >= 0)
				{
					int apart = firstSector[0] >= 0 && firstSector[1] >= 0 &&
						    firstSector[0] != firstSector[1];

					if (apart && disk_format == DISKIMAGE_JVC)
					{
						fprintf(stderr,
							"%s: '%s' records one initial sector number and cannot record %d and %d\n",
							argv[0], argv[i], firstSector[0], firstSector[1]);

						return (EOS_SECT);
					}

					if (!records_number)
					{
						fprintf(stderr,
							"%s: Warning: '%s' does not include a mechanism to express physical sector numbers\n",
							argv[0], argv[i]);
					}
				}
			}

			ec =
				_os9_format_image(argv[i], disk_format, os968k, tracks,
						  sectorsPerTrack, sectorsTrack0,
						  heads, bytesPerSector, &clusterSize,
						  diskName, sectorAllocationSize,
						  tpi, density, formatEntire,
						  isDragon, isHDD, interleave,
						  sideFirstSector, track0SingleDensity,
						  &totalSectors, &totalBytes);

			if (ec == EOS_FAE)
			{
				fprintf(stderr, "%s: '%s' exists, and only a raw image replaces one\n",
					argv[0], argv[i]);
			}
			else if (ec == EOS_IA)
			{
				fprintf(stderr, "%s: '%s' can't have this layout in this format\n",
					argv[0], argv[i]);
			}

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
			else
			{
				fprintf(stderr, "%s: error %d formatting '%s'\n",
					argv[0], ec, argv[i]);

				return (ec);
			}

		}
	}

	return (ec);
}

/********************************************************************
 * diskinfo.c - Report the format and track layout of disk images
 *
 * Tracks with the same layout are reported together.  On a two sided
 * image a run of tracks has one line if both sides match, and a line
 * for each side if not.
 *
 * It can also set or clear the write protect flag of an image.
 ********************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <toolshed.h>
#include <diskimage.h>
#include <util.h>

/* A track holds at most 256 sectors, as a sector number is one byte, and the
   longest sentence about any one of them is under 80 bytes. */
#define TRACK_TEXT_SIZE	32768
#define MAX_TRACK_SECTORS	256

static char const *const helpMessage[] = {
	"diskinfo from Toolshed " TOOLSHED_VERSION "\n",
	"Report the format and track layout of disk images\n",
	"Syntax: diskinfo {[<opts>]} <file> {<file> ...}\n",
	"Options:\n",
	"     -p  = set the write protect flag of a DMK, SDF or VDK image\n",
	"     -u  = clear the write protect flag of a DMK, SDF or VDK image\n",
	"The exit status is 0 only if every file is a disk image.\n",
	NULL
};


static void append(char *text, const char *format, ...)
{
	size_t used = strlen(text);
	va_list args;

	va_start(args, format);
	vsnprintf(text + used, TRACK_TEXT_SIZE - used, format, args);
	va_end(args);
}


/* Add the numbers of the sectors of one size, as in 1-3, 5, 7-9. */
static void append_numbers(char *text, const diskimage_sector_info *sectors, u_int count, u_int size)
{
	int first = 1;
	u_int i = 0;

	while (i < count)
	{
		u_int last = i;
		u_int next;

		if (sectors[i].size != size)
		{
			i++;
			continue;
		}

		for (next = i + 1; next < count; next++)
		{
			if (sectors[next].size != size)
			{
				continue;
			}
			if (sectors[next].sector != sectors[last].sector + 1)
			{
				break;
			}
			last = next;
		}

		append(text, first ? "%u" : ", %u", sectors[i].sector);
		if (last != i)
		{
			append(text, "-%u", sectors[last].sector);
		}
		first = 0;
		i = next;
	}
}


/* Describe the sectors of each size, in the order the sizes first appear. */
static void append_sizes(char *text, const diskimage_sector_info *sectors, u_int count)
{
	u_int sizes[MAX_TRACK_SECTORS];
	u_int size_count = 0;
	u_int i, j;

	for (i = 0; i < count; i++)
	{
		for (j = 0; j < size_count && sizes[j] != sectors[i].size; j++)
		{
		}
		if (j == size_count)
		{
			sizes[size_count++] = sectors[i].size;
		}
	}

	for (i = 0; i < size_count; i++)
	{
		u_int matching = 0;

		for (j = 0; j < count; j++)
		{
			matching += sectors[j].size == sizes[i];
		}

		if (i > 0)
		{
			append(text, i == size_count - 1 ? ", and " : ", ");
		}
		if (matching == 1)
		{
			append(text, "1 %u byte sector (", sizes[i]);
			append_numbers(text, sectors, count, sizes[i]);
			append(text, ")");
		}
		else
		{
			append(text, "%u sectors (", matching);
			append_numbers(text, sectors, count, sizes[i]);
			append(text, "), %u bytes each", sizes[i]);
		}
	}
	append(text, ".");
}


static void append_conditions(char *text, const diskimage_sector_info *sectors, u_int count)
{
	u_int i;

	for (i = 0; i < count; i++)
	{
		int deleted = (sectors[i].flags & DISKIMAGE_SECTOR_DELETED) != 0;
		int crc_error = (sectors[i].flags & DISKIMAGE_SECTOR_CRC_ERROR) != 0;

		if (!deleted && !crc_error)
		{
			continue;
		}

		append(text, " Sector %u has ", sectors[i].sector);
		if (deleted)
		{
			append(text, "a deleted data mark");
		}
		if (deleted && crc_error)
		{
			append(text, " and ");
		}
		if (crc_error)
		{
			append(text, "a CRC error in its data segment");
		}
		append(text, ".");
	}
}


/* Describe a track, moving *index past its sectors.  The text is empty if the
   track has none. */
static error_code describe_track(diskimage_id disk, u_int total, u_int *index, u_int cylinder, u_int side, char *text)
{
	static diskimage_sector_info sectors[MAX_TRACK_SECTORS];
	u_int count = 0;

	text[0] = '\0';

	while (*index < total && count < MAX_TRACK_SECTORS)
	{
		diskimage_sector_info info;
		error_code ec = _diskimage_query_sector(disk, *index, &info);

		if (ec != 0)
		{
			return ec;
		}
		if (info.cylinder != cylinder || info.side != side)
		{
			break;
		}
		sectors[count++] = info;
		(*index)++;
	}

	if (count > 0)
	{
		append_sizes(text, sectors, count);
		append_conditions(text, sectors, count);
	}

	return 0;
}


static void print_tracks(u_int first, u_int last, const char *where, const char *text)
{
	int several = first != last;

	if (several)
	{
		printf("Tracks %u to %u%s ", first, last, where);
	}
	else
	{
		printf("Track %u%s ", first, where);
	}

	if (text[0] == '\0')
	{
		printf("%s no sectors.\n", several ? "have" : "has");
	}
	else
	{
		printf("%s %s\n", several ? "are" : "is", text);
	}
}


static void print_run(u_int first, u_int last, u_int sides, char text[2][TRACK_TEXT_SIZE])
{
	if (sides == 1)
	{
		print_tracks(first, last, "", text[0]);
	}
	else if (strcmp(text[0], text[1]) == 0)
	{
		print_tracks(first, last, ", both sides,", text[0]);
	}
	else
	{
		print_tracks(first, last, ", side 0,", text[0]);
		print_tracks(first, last, ", side 1,", text[1]);
	}
}


static error_code report_tracks(diskimage_id disk, const diskimage_geometry *geometry)
{
	static char run[2][TRACK_TEXT_SIZE];
	static char current[2][TRACK_TEXT_SIZE];
	u_int index = 0;
	u_int run_start = 0;
	u_int cylinder;

	if (geometry->cylinders == 0)
	{
		printf("It has no tracks.\n");
		return 0;
	}

	for (cylinder = 0; cylinder < geometry->cylinders; cylinder++)
	{
		u_int side;

		current[1][0] = '\0';
		for (side = 0; side < geometry->sides; side++)
		{
			error_code ec = describe_track(disk, geometry->total_sectors, &index, cylinder, side, current[side]);

			if (ec != 0)
			{
				return ec;
			}
		}

		if (cylinder > 0 && (strcmp(current[0], run[0]) != 0 || strcmp(current[1], run[1]) != 0))
		{
			print_run(run_start, cylinder - 1, geometry->sides, run);
			run_start = cylinder;
		}
		if (cylinder == run_start)
		{
			memcpy(run, current, sizeof(run));
		}
	}
	print_run(run_start, geometry->cylinders - 1, geometry->sides, run);

	return 0;
}


static const char *format_name(diskimage_format format)
{
	switch (format)
	{
	case DISKIMAGE_RAW:
		return "a raw";
	case DISKIMAGE_JVC:
		return "a JVC";
	case DISKIMAGE_DMK:
		return "a DMK";
	case DISKIMAGE_SDF:
		return "an SDF";
	case DISKIMAGE_VDK:
		return "a VDK";
	default:
		return "an unknown";
	}
}


static int report_file(const char *pathname)
{
	diskimage_geometry geometry;
	diskimage_id disk;
	u_int filesystems;
	error_code ec = _diskimage_open(&disk, pathname, FAM_READ);

	if (ec == EOS_WT)
	{
		printf("%s is not a DMK, SDF, JVC, VDK, nor raw disk image.\n", pathname);
		return 1;
	}
	if (ec != 0)
	{
		fprintf(stderr, "%s: Error: %d - %s\n", pathname, ec, TSReportError(ec));
		return 1;
	}

	ec = _diskimage_query_geometry(disk, &geometry);
	if (ec == 0)
	{
		printf("%s is %s disk image.\n", pathname, format_name(geometry.format));
		if (geometry.write_protected)
		{
			printf("It is write protected.\n");
		}
		if (_coco_detect_filesystems(pathname, &filesystems) == 0 && filesystems != 0)
		{
			char names[64];

			_coco_filesystem_names(filesystems, names, sizeof(names));
			printf("Possible filesystems: %s\n", names);
		}
		ec = report_tracks(disk, &geometry);
	}
	_diskimage_close(disk);

	if (ec != 0)
	{
		fprintf(stderr, "%s: Error: %d - %s\n", pathname, ec, TSReportError(ec));
		return 1;
	}

	return 0;
}


static int set_write_protect(const char *pathname, int write_protect)
{
	error_code ec = _diskimage_set_write_protect(pathname, write_protect);

	if (ec == EOS_IA)
	{
		fprintf(stderr, "%s: this kind of image has no write protect flag.\n", pathname);
		return 1;
	}
	if (ec == EOS_WT)
	{
		printf("%s is not a DMK, SDF, JVC, VDK, nor raw disk image.\n", pathname);
		return 1;
	}
	if (ec != 0)
	{
		fprintf(stderr, "%s: Error: %d - %s\n", pathname, ec, TSReportError(ec));
		return 1;
	}

	printf("%s is %s.\n", pathname, write_protect ? "now write protected" : "no longer write protected");

	return 0;
}


int main(int argc, char *argv[])
{
	int status = 0;
	int write_protect = -1;
	int printed = 0;
	int i;

	if (argc < 2)
	{
		show_help(helpMessage);
		return 1;
	}

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0)
		{
			show_help(helpMessage);
			return 0;
		}
		if (strcmp(argv[i], "-p") == 0)
		{
			write_protect = 1;
		}
		else if (strcmp(argv[i], "-u") == 0)
		{
			write_protect = 0;
		}
		else if (argv[i][0] == '-')
		{
			fprintf(stderr, "diskinfo: unknown option '%s'\n", argv[i]);
			return 1;
		}
	}

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		if (write_protect >= 0)
		{
			status |= set_write_protect(argv[i], write_protect);
			continue;
		}
		if (printed)
		{
			printf("\n");
		}
		printed = 1;
		status |= report_file(argv[i]);
	}

	return status;
}

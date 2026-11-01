/* Tests for the Dragon DOS library. */

#include <string.h>

#include "tinytest.h"
#include "dragondospath.h"
#include "cocopath.h"
#include "toolshed.h"
#include "../libdragondos/libdragondosint.h"

#define TEST_IMAGE "dragondos_test.dsk"
#define TEST_FILE(name) TEST_IMAGE "," name
#define RW (FAM_READ | FAM_WRITE)

/* A disk saved to by Dragon DOS 1.0 running in MAME.  It holds HW1.BAS to
   HW5.BAS, of 41 bytes each.  The tests run in build/unix/unittest. */
#define MAME_IMAGE "../../../unittest/testdata/dragondos mame.vdk"
#define VDK_HEADER 12

#define SS_DIRECTORY 360
#define DS_DIRECTORY 720

/* ASSERT_EQUALS evaluates its second argument twice, so calls with side
   effects keep their result here first. */
static error_code call_result;
#define ASSERT_CALL(expected, call) \
	{ \
		call_result = (call); \
		ASSERT_EQUALS(expected, call_result); \
	}

static u_char image[1024 * 1024];
static u_char other[1024 * 1024];
static u_char data[256 * 1024];
static u_char back[256 * 1024];

static long read_file(const char *pathname, u_char *buffer, long size)
{
	FILE *fp = fopen(pathname, "rb");
	long got;

	if (fp == NULL)
	{
		return -1;
	}
	got = (long)fread(buffer, 1, size, fp);
	fclose(fp);
	return got;
}

static void write_file(const char *pathname, const u_char *buffer, long size)
{
	FILE *fp = fopen(pathname, "wb");

	fwrite(buffer, 1, size, fp);
	fclose(fp);
}

static void fill_pattern(u_int size, u_char seed)
{
	u_int i;

	for (i = 0; i < size; i++)
	{
		data[i] = (u_char)(i * 7 + (i >> 8) + seed);
	}
}

/* Make a file of the given size holding the pattern. */
static error_code save(const char *pathlist, u_int size, u_char seed)
{
	dragondos_path_id path;
	u_int written = size;
	error_code ec = _dragondos_create(&path, pathlist, RW);

	if (ec != 0)
	{
		return ec;
	}
	fill_pattern(size, seed);
	ec = _dragondos_write(path, data, &written);
	if (ec == 0 && written != size)
	{
		ec = EOS_WRITE;
	}
	{
		error_code close_ec = _dragondos_close(path);

		return ec != 0 ? ec : close_ec;
	}
}

/* Check a file has the given size and holds the pattern. */
static int file_matches(const char *pathlist, u_int size, u_char seed)
{
	dragondos_path_id path;
	u_int got = size + 1;
	u_int file_size = 0;
	int good;

	if (_dragondos_open(&path, pathlist, FAM_READ) != 0)
	{
		return 0;
	}
	fill_pattern(size, seed);
	_dragondos_gs_size(path, &file_size);
	good = file_size == size;
	if (size == 0)
	{
		good = good && _dragondos_read(path, back, &got) == EOS_EOF;
	}
	else
	{
		good = good && _dragondos_read(path, back, &got) == 0 && got == size &&
			memcmp(back, data, size) == 0;
	}
	_dragondos_close(path);
	return good;
}

#define ENTRY_AT_END 0x100

static int entry_flags(u_int index)
{
	dragondos_path_id path;
	dragondos_dir_entry entry;
	int flags = -1;

	if (_dragondos_open(&path, TEST_IMAGE ",", FAM_READ) == 0)
	{
		_dragondos_seekdir(path, index);
		if (_dragondos_readdir(path, &entry) == 0)
		{
			flags = entry.flags;
		}
		else
		{
			flags = ENTRY_AT_END;
		}
		_dragondos_close(path);
	}
	return flags;
}

static u_int free_sectors(void)
{
	dragondos_path_id path;
	u_int sectors = 0;

	if (_dragondos_open(&path, TEST_IMAGE ",", FAM_READ) == 0)
	{
		_dragondos_gs_free(path, &sectors);
		_dragondos_close(path);
	}
	return sectors;
}

/* Find which LSN a file sector is on. */
static u_int lsn_of(const char *pathlist, u_int file_sector)
{
	dragondos_path_id path;
	u_int lsn = 0xFFFFFFFF;

	if (_dragondos_open(&path, pathlist, FAM_READ) == 0)
	{
		_dd_sector_lsn(path, file_sector, &lsn);
		_dragondos_close(path);
	}
	return lsn;
}

/* Allocator tests.  Every result here is what the routine at $CD31 of the
   Dragon DOS 1.0 ROM gives for this bitmap. */
typedef struct
{
	const char *name;
	u_int sectors_per_track;
	u_int tracks;
	u_int hint;
	u_int free_runs[8][2];
	u_int lsn;		/* 0xFFFF means the disk is full */
	u_int count;
} allocation_case;

static const allocation_case allocation_cases[] =
{
	{ "two free tracks below the directory", 18, 40, DD_NO_HINT, { { 0, 288 }, { 306, 54 }, { 378, 342 } }, 324, 36 },
	{ "a used track splits the pairs", 18, 40, DD_NO_HINT, { { 342, 1 }, { 306, 1 }, { 270, 1 }, { 252, 1 } }, 252, 1 },
	{ "a free hint is used", 18, 40, 100, { { 100, 5 }, { 324, 36 } }, 100, 5 },
	{ "a used hint is searched from", 18, 40, 100, { { 324, 36 } }, 324, 36 },
	{ "one free track", 18, 40, DD_NO_HINT, { { 126, 3 } }, 126, 3 },
	{ "the lowest of unpaired tracks", 18, 40, DD_NO_HINT, { { 342, 1 }, { 126, 1 } }, 126, 1 },
	{ "above the directory", 18, 40, DD_NO_HINT, { { 378, 3 } }, 378, 3 },
	{ "only the first sector of a track counts", 18, 40, DD_NO_HINT, { { 379, 3 }, { 396, 1 } }, 396, 1 },
	{ "a used high hint looks up from 1440", 36, 80, 1500, { { 1458, 4 }, { 324, 2 } }, 1458, 4 },
	{ "a free high hint", 36, 80, 1500, { { 1500, 4 } }, 1500, 4 },
	{ "a used high hint falls back to a low track", 36, 80, 1500, { { 324, 18 } }, 324, 18 },
	{ "second bitmap sector's run beats the first's", 36, 80, DD_NO_HINT, { { 1, 17 }, { 1441, 16 } }, 1441, 16 },
	{ "first bitmap sector when the second has one byte", 36, 80, DD_NO_HINT, { { 1, 17 }, { 1445, 2 } }, 1, 17 },
	{ "second bitmap sector wins between single bytes", 36, 80, DD_NO_HINT, { { 1, 3 }, { 1445, 2 } }, 1445, 2 },
	{ "first bitmap sector when the second is empty", 36, 80, DD_NO_HINT, { { 1, 3 } }, 1, 3 },
	{ "a single byte gives its highest bits", 18, 40, DD_NO_HINT, { { 1, 1 }, { 3, 2 } }, 3, 2 },
	{ "a run starts with the bits reaching its next byte", 18, 40, DD_NO_HINT, { { 5, 13 }, { 100, 1 } }, 5, 13 },
	{ "the longest run of bytes wins", 18, 40, DD_NO_HINT, { { 1, 17 }, { 55, 17 } }, 55, 17 },
	{ "the first of equal runs wins", 18, 40, DD_NO_HINT, { { 1, 17 }, { 37, 17 } }, 1, 17 },
	{ "a full disk", 18, 40, DD_NO_HINT, { { 0, 0 } }, 0xFFFF, 0 },
	{ "at most 255 sectors", 18, 40, 0, { { 0, 288 }, { 378, 342 } }, 0, 255 },
	{ "a run ends with its bitmap sector", 36, 80, 1439, { { 1439, 1 }, { 1440, 10 } }, 1439, 1 },
	{ "a run up to the end of a bitmap sector", 36, 80, 1430, { { 1430, 10 }, { 1440, 10 } }, 1430, 10 }
};

static struct _dragondos_path_id allocation_path;

static void set_up_allocation(const allocation_case *test)
{
	dragondos_path_id path = &allocation_path;
	int i;
	u_int lsn;

	memset(path, 0, sizeof(*path));
	path->sectors_per_track = test->sectors_per_track;
	path->tracks = test->tracks;
	path->total_sectors = test->tracks * test->sectors_per_track;
	path->directory_lsn = 20 * test->sectors_per_track;
	path->track_loaded[0] = path->track_loaded[1] = 1;

	for (i = 0; i < 8; i++)
	{
		for (lsn = test->free_runs[i][0]; lsn < test->free_runs[i][0] + test->free_runs[i][1]; lsn++)
		{
			path->track[lsn / DD_BITMAP_SECTOR_LSNS][(lsn % DD_BITMAP_SECTOR_LSNS) / 8] |= (u_char)(1 << (lsn % 8));
		}
	}
}

static void test_allocator_matches_rom(void)
{
	size_t i;

	for (i = 0; i < sizeof(allocation_cases) / sizeof(allocation_cases[0]); i++)
	{
		const allocation_case *test = &allocation_cases[i];
		u_int lsn = 0, count = 0, n;

		set_up_allocation(test);
		call_result = _dd_allocate(&allocation_path, test->hint, &lsn, &count);
		ASSERT(test->name, call_result == (test->lsn == 0xFFFF ? EOS_DF : 0));
		if (call_result == 0)
		{
			ASSERT(test->name, lsn == test->lsn);
			ASSERT(test->name, count == test->count);
			for (n = 0; n < count; n++)
			{
				int is_free = 1;

				_dd_sector_free(&allocation_path, lsn + n, &is_free);
				ASSERT(test->name, !is_free);
			}
		}
	}
}

/* Bits past the end of the disk are not free, whatever the bitmap says. */
static void test_allocator_stays_on_disk(void)
{
	static const allocation_case test = { "", 18, 40, DD_NO_HINT, { { 720, 100 } }, 0, 0 };
	u_int lsn, count;

	set_up_allocation(&test);
	ASSERT_CALL(EOS_DF, _dd_allocate(&allocation_path, DD_NO_HINT, &lsn, &count));
}

/* A single sided disk doesn't need the bitmap's second sector, so running out
   of space must not try to read it. */
static void test_allocator_ignores_second_sector_when_single_sided(void)
{
	static const allocation_case test = { "", 18, 40, DD_NO_HINT, { { 1, 3 } }, 0, 0 };
	u_int lsn, count;

	set_up_allocation(&test);
	allocation_path.track_loaded[1] = 0;
	/* With no image to read, reaching for the sector would crash. */
	ASSERT_CALL(0, _dd_allocate(&allocation_path, DD_NO_HINT, &lsn, &count));
	ASSERT_EQUALS(1, lsn);
	ASSERT_EQUALS(3, count);
}

static void test_parse_names(void)
{
	u_char name[11];

	ASSERT_CALL(0, _dd_parse_name("HW1.BAS", name));
	ASSERT_MEM_EQUALS("HW1\0\0\0\0\0BAS", name, 11);
	ASSERT_CALL(0, _dd_parse_name("a-9/b", name));
	ASSERT_MEM_EQUALS("A-9\0\0\0\0\0B\0\0", name, 11);
	ASSERT_CALL(0, _dd_parse_name("ABCDEFGH.XYZ", name));
	ASSERT_MEM_EQUALS("ABCDEFGHXYZ", name, 11);
	ASSERT_CALL(0, _dd_parse_name("NOEXT.", name));
	ASSERT_MEM_EQUALS("NOEXT\0\0\0\0\0\0", name, 11);

	/* A long name is cut short, and what follows it is ignored. */
	ASSERT_CALL(0, _dd_parse_name("ABCDEFGHIJKL.XYZ", name));
	ASSERT_MEM_EQUALS("ABCDEFGH\0\0\0", name, 11);

	ASSERT_CALL(EOS_BPNAM, _dd_parse_name("", name));
	ASSERT_CALL(EOS_BPNAM, _dd_parse_name(".BAS", name));
	ASSERT_CALL(EOS_BPNAM, _dd_parse_name("A.B.C", name));
	ASSERT_CALL(EOS_BPNAM, _dd_parse_name("A.BCDE", name));
	ASSERT_CALL(EOS_BPNAM, _dd_parse_name("A B", name));
	ASSERT_CALL(EOS_BPNAM, _dd_parse_name("A_B", name));
}

static void test_read_mame_disk(void)
{
	dragondos_path_id path;
	dragondos_dir_entry entry;
	char name[13];
	char expected[13];
	u_int size;
	u_int free_count;
	u_char sector[256];
	int i;

	ASSERT_CALL(0, _dragondos_open(&path, MAME_IMAGE ",", FAM_READ));
	ASSERT_EQUALS(40, path->tracks);
	ASSERT_EQUALS(18, path->sectors_per_track);
	for (i = 1; i <= 5; i++)
	{
		ASSERT_CALL(0, _dragondos_readdir(path, &entry));
		ASSERT_CALL(0, _dragondos_ncpy_name(&entry, name, sizeof(name)));
		sprintf(expected, "HW%d.BAS", i);
		ASSERT_STRING_EQUALS(expected, name);
		ASSERT_EQUALS(41, entry.last_or_next);
	}
	ASSERT_CALL(EOS_EOF, _dragondos_readdir(path, &entry));
	ASSERT_CALL(0, _dragondos_gs_free(path, &free_count));
	ASSERT_EQUALS(720 - 36 - 5, free_count);
	ASSERT_CALL(0, _dragondos_gs_sector(path, 324, sector));
	ASSERT_EQUALS(0x55, sector[0]);
	ASSERT_CALL(EOS_BMODE, _dragondos_gs_size(path, &size));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, _dragondos_open(&path, MAME_IMAGE ",hw3.bas", FAM_READ));
	ASSERT_CALL(0, _dragondos_gs_size(path, &size));
	ASSERT_EQUALS(41, size);
	size = sizeof(sector);
	ASSERT_CALL(0, _dragondos_read(path, sector, &size));
	ASSERT_EQUALS(41, size);
	ASSERT_EQUALS(0x55, sector[0]);
	ASSERT_EQUALS(0x30, sector[37]);
	ASSERT_CALL(EOS_EOF, _dragondos_gs_eof(path));
	size = 1;
	ASSERT_CALL(EOS_EOF, _dragondos_read(path, sector, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(EOS_PNNF, _dragondos_open(&path, MAME_IMAGE ",NOPE.BAS", FAM_READ));
	ASSERT_CALL(EOS_BPNAM, _dragondos_open(&path, MAME_IMAGE, FAM_READ));
}

/* DSKINIT and saving the same files must make the same disk Dragon DOS made. */
static void test_reproduce_mame_disk(void)
{
	static u_char programs[5][41];
	dragondos_path_id path;
	long expected_size = read_file(MAME_IMAGE, other, sizeof(other));
	long i;
	long actual_size;

	ASSERT("test image must be readable", expected_size == VDK_HEADER + 40 * 18 * 256);

	for (i = 0; i < 5; i++)
	{
		char pathlist[64];
		u_int size = 41;

		sprintf(pathlist, MAME_IMAGE ",HW%ld.BAS", i + 1);
		ASSERT_CALL(0, _dragondos_open(&path, pathlist, FAM_READ));
		ASSERT_CALL(0, _dragondos_read(path, programs[i], &size));
		ASSERT_CALL(0, _dragondos_close(path));
	}

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_VDK, 40, 1));
	for (i = 0; i < 5; i++)
	{
		char pathlist[64];
		u_int size = 41;

		sprintf(pathlist, TEST_IMAGE ",HW%ld.BAS", i + 1);
		ASSERT_CALL(0, _dragondos_create(&path, pathlist, RW));
		ASSERT_CALL(0, _dragondos_write(path, programs[i], &size));
		ASSERT_CALL(0, _dragondos_close(path));
	}

	actual_size = read_file(TEST_IMAGE, image, sizeof(image));
	ASSERT_EQUALS(expected_size, actual_size);
	for (i = VDK_HEADER; i < expected_size; i++)
	{
		ASSERT_EQUALS(other[i], image[i]);
	}
	remove(TEST_IMAGE);
}

static void check_bitmap(diskimage_format format, u_int tracks, u_int sides, u_int free_count, long directory_offset)
{
	u_int spt = 18 * sides;
	u_int dir = 20 * spt;
	u_int i;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, format, tracks, sides));
	ASSERT_EQUALS(free_count, free_sectors());
	ASSERT_EQUALS(ENTRY_AT_END, entry_flags(0));

	ASSERT_EQUALS(tracks * 18 * sides * 256, read_file(TEST_IMAGE, image, sizeof(image)));
	ASSERT_EQUALS(tracks, image[directory_offset + 0xFC]);
	ASSERT_EQUALS(spt, image[directory_offset + 0xFD]);
	ASSERT_EQUALS((u_char)~tracks, image[directory_offset + 0xFE]);
	ASSERT_EQUALS((u_char)~spt, image[directory_offset + 0xFF]);

	/* The directory track and its copy are used, the tracks around are free. */
	for (i = 0; i < 18; i++)
	{
		u_char *bitmap = image + directory_offset;

		ASSERT_EQUALS(0, (bitmap[(dir + i) / 8] >> ((dir + i) % 8)) & 1);
		ASSERT_EQUALS(0, (bitmap[(dir - 4 * spt + i) / 8] >> ((dir - 4 * spt + i) % 8)) & 1);
	}
	ASSERT_EQUALS(1, (image[directory_offset + (dir - 1) / 8] >> ((dir - 1) % 8)) & 1);
	ASSERT_EQUALS(1, (image[directory_offset + (dir + 18) / 8] >> ((dir + 18) % 8)) & 1);

	/* The copy matches. */
	ASSERT_MEM_EQUALS(image + directory_offset, image + directory_offset - 4L * spt * 256, 18 * 256);

	remove(TEST_IMAGE);
}

static void test_dskinit_bitmaps(void)
{
	check_bitmap(DISKIMAGE_RAW, 40, 1, 720 - 36, SS_DIRECTORY * 256L);
	check_bitmap(DISKIMAGE_RAW, 80, 1, 1440 - 36, SS_DIRECTORY * 256L);
	check_bitmap(DISKIMAGE_RAW, 40, 2, 1440 - 36, DS_DIRECTORY * 256L);
	check_bitmap(DISKIMAGE_RAW, 80, 2, 2880 - 36, DS_DIRECTORY * 256L);
	check_bitmap(DISKIMAGE_RAW, 35, 1, 630 - 36, SS_DIRECTORY * 256L);
}

static void test_dskinit_arguments(void)
{
	remove(TEST_IMAGE);
	ASSERT_CALL(EOS_IA, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 20, 1));
	ASSERT_CALL(EOS_IA, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 81, 2));
	ASSERT_CALL(EOS_IA, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 3));
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(EOS_FAE, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	remove(TEST_IMAGE);
}

/* The same work must succeed in every image format, either side count. */
static void test_all_formats(void)
{
	static const diskimage_format formats[] =
	{
		DISKIMAGE_RAW, DISKIMAGE_JVC, DISKIMAGE_VDK, DISKIMAGE_DMK, DISKIMAGE_SDF
	};
	size_t i;
	u_int sides;

	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
	{
		for (sides = 1; sides <= 2; sides++)
		{
			u_int blank = 40 * 18 * sides - 36;

			remove(TEST_IMAGE);
			ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, formats[i], 40, sides));
			ASSERT_EQUALS(blank, free_sectors());
			ASSERT_CALL(0, save(TEST_FILE("LONG.BIN"), 20000, 3));
			ASSERT_CALL(0, save(TEST_FILE("SHORT.DAT"), 5, 9));
			ASSERT("long file reads back", file_matches(TEST_FILE("LONG.BIN"), 20000, 3));
			ASSERT("short file reads back", file_matches(TEST_FILE("SHORT.DAT"), 5, 9));
			ASSERT_EQUALS(blank - (20000 + 255) / 256 - 1, free_sectors());
			ASSERT_CALL(0, _dragondos_kill(TEST_FILE("LONG.BIN")));
			ASSERT_CALL(0, _dragondos_kill(TEST_FILE("SHORT.DAT")));
			ASSERT_EQUALS(blank, free_sectors());
			remove(TEST_IMAGE);
		}
	}
}

/* Sides are found from the LSN: the sector's own place on the disk. */
static void test_double_sided_placement(void)
{
	u_char sector[256];
	diskimage_id disk;
	u_int size = sizeof(sector);

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_VDK, 40, 2));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 256 * 19, 1));

	/* Two free tracks below LSN 720 are 702 and 684.  The file goes on from
	   684: cylinder 19, side 0 then side 1. */
	ASSERT_EQUALS(684, lsn_of(TEST_FILE("FILE"), 0));
	ASSERT_EQUALS(702, lsn_of(TEST_FILE("FILE"), 18));

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 19, 0, 1, sector, &size));
	ASSERT_MEM_EQUALS(data, sector, 256);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 19, 1, 1, sector, &size));
	ASSERT_MEM_EQUALS(data + 18 * 256, sector, 256);
	ASSERT_CALL(0, _diskimage_close(disk));
	remove(TEST_IMAGE);
}

/* A raw image has no sides, so its double sided disk is found further in. */
static void test_raw_double_sided_placement(void)
{
	dragondos_path_id path;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 2));
	ASSERT_CALL(0, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));
	ASSERT_EQUALS(1, path->linear);
	ASSERT_EQUALS(DS_DIRECTORY, path->directory_lsn);
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 1));
	ASSERT_EQUALS(684, lsn_of(TEST_FILE("FILE"), 0));
	ASSERT_EQUALS(1440 * 256, read_file(TEST_IMAGE, image, sizeof(image)));
	fill_pattern(300, 1);
	ASSERT_MEM_EQUALS(data, image + 684 * 256L, 300);
	remove(TEST_IMAGE);
}

static void test_sizes_at_sector_boundaries(void)
{
	static const u_int sizes[] = { 0, 1, 255, 256, 257, 511, 512, 513, 65535, 65536, 65537 };
	size_t i;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 2));
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		u_int blank = free_sectors();

		ASSERT_CALL(0, save(TEST_FILE("SIZE"), sizes[i], (u_char)i));
		ASSERT("file reads back", file_matches(TEST_FILE("SIZE"), sizes[i], (u_char)i));
		ASSERT_EQUALS(blank - (sizes[i] + 255) / 256, free_sectors());
		ASSERT_CALL(0, _dragondos_kill(TEST_FILE("SIZE")));
		ASSERT_EQUALS(blank, free_sectors());
	}
	remove(TEST_IMAGE);
}

/* Writes in small pieces land exactly as one large write does. */
static void test_small_writes_match_one_write(void)
{
	dragondos_path_id path;
	u_int position = 0;
	long size;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 30000, 5));
	size = read_file(TEST_IMAGE, other, sizeof(other));
	remove(TEST_IMAGE);

	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("FILE"), RW));
	fill_pattern(30000, 5);
	while (position < 30000)
	{
		u_int chunk = 37 + position % 311;

		if (chunk > 30000 - position)
		{
			chunk = 30000 - position;
		}
		ASSERT_CALL(0, _dragondos_write(path, data + position, &chunk));
		position += chunk;
	}
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_EQUALS(size, read_file(TEST_IMAGE, image, sizeof(image)));
	ASSERT_MEM_EQUALS(other, image, size);
	remove(TEST_IMAGE);
}

/* Leave many small gaps on a disk, then make a file fill them.  File A takes
   entry 0 until it is deleted, so that entry 0 is free for a continuation
   entry if the search doesn't avoid it. */
static void test_continuation_entries(void)
{
	dragondos_path_id path;
	u_int blank;
	u_int size = 40000;
	int i;
	dragondos_dir_entry *entry;
	dragondos_extent *extent;
	int count, more, entries = 1;
	u_int next, index;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 80, 2));
	blank = free_sectors();
	ASSERT_CALL(0, save(TEST_FILE("A"), 10, 0));
	ASSERT_CALL(0, save(TEST_FILE("BIG"), 0, 0));
	for (i = 0; i < 40; i++)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d"), i);
		ASSERT_CALL(0, save(pathlist, 256, (u_char)i));
	}
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("A")));
	for (i = 0; i < 40; i += 2)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d"), i);
		ASSERT_CALL(0, _dragondos_kill(pathlist));
	}

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("BIG"), RW));
	fill_pattern(size, 77);
	ASSERT_CALL(0, _dragondos_write(path, data, &size));
	ASSERT_EQUALS(40000, size);
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT("the file reads back", file_matches(TEST_FILE("BIG"), 40000, 77));

	/* Entry 0 stays free and every entry but the last says where to go. */
	ASSERT_EQUALS(DD_KILLED_FLAGS, entry_flags(0));
	ASSERT_EQUALS(DRAGONDOS_FLAG_CONTINUED, entry_flags(1));
	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("BIG"), FAM_READ));
	index = path->entry_index;
	ASSERT_EQUALS(1, index);
	for (;;)
	{
		ASSERT_CALL(0, _dd_entry(path, index, &entry));
		_dd_extents(entry, &extent, &count);
		ASSERT_CALL(0, _dd_next_entry(entry, &next, &more));
		if (!more)
		{
			break;
		}
		ASSERT("a continuation is never entry 0", next != 0);
		index = next;
		entries++;
		ASSERT_CALL(0, _dd_entry(path, index, &entry));
		ASSERT_EQUALS(DRAGONDOS_FLAG_CONTINUATION, entry->flags & (DRAGONDOS_FLAG_CONTINUATION | DRAGONDOS_FLAG_DELETED));
	}
	ASSERT("the file needs continuation entries", entries > 1);
	ASSERT_EQUALS((40000 & 0xFF), entry->last_or_next);
	ASSERT_CALL(0, _dragondos_close(path));

	/* The directory track copy keeps up. */
	ASSERT_EQUALS(2880 * 256, read_file(TEST_IMAGE, image, sizeof(image)));
	ASSERT_MEM_EQUALS(image + DS_DIRECTORY * 256L, image + (DS_DIRECTORY - 144) * 256L, 18 * 256);

	/* Deleting returns the sectors and every entry. */
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("BIG")));
	for (i = 1; i < 40; i += 2)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d"), i);
		ASSERT_CALL(0, _dragondos_kill(pathlist));
	}
	ASSERT_EQUALS(blank, free_sectors());
	for (i = 0; i < 42; i++)
	{
		ASSERT_EQUALS(DD_KILLED_FLAGS, entry_flags(i));
	}
	ASSERT_EQUALS(ENTRY_AT_END, entry_flags(42));
	remove(TEST_IMAGE);
}

static void test_disk_full(void)
{
	dragondos_path_id path;
	u_int size = 720 * 256;
	u_int blank;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	blank = free_sectors();
	ASSERT_CALL(0, save(TEST_FILE("KEEP"), 1000, 2));

	/* A write that can't fit changes nothing. */
	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("LARGE"), RW));
	fill_pattern(size, 1);
	ASSERT_CALL(EOS_DF, _dragondos_write(path, data, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_CALL(0, _dragondos_gs_size(path, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT_EQUALS(blank - 4, free_sectors());

	/* What does fit, fits, right to the end of the disk. */
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("LARGE")));
	ASSERT_CALL(0, save(TEST_FILE("LARGE"), (blank - 4) * 256, 8));
	ASSERT_EQUALS(0, free_sectors());
	ASSERT("the file reads back", file_matches(TEST_FILE("LARGE"), (blank - 4) * 256, 8));
	ASSERT("the other file is intact", file_matches(TEST_FILE("KEEP"), 1000, 2));
	remove(TEST_IMAGE);
}

static void test_directory_full(void)
{
	int i;
	dragondos_path_id path;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	for (i = 0; i < DRAGONDOS_MAX_DIR_ENTRIES; i++)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d"), i);
		ASSERT_CALL(0, save(pathlist, 1, 1));
	}
	ASSERT_CALL(EOS_DF, _dragondos_create(&path, TEST_FILE("MORE"), RW));

	/* Reading a full directory runs off its end, not past it. */
	ASSERT_CALL(0, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));
	for (i = 0; i < DRAGONDOS_MAX_DIR_ENTRIES; i++)
	{
		dragondos_dir_entry entry;

		ASSERT_CALL(0, _dragondos_readdir(path, &entry));
	}
	{
		dragondos_dir_entry entry;

		ASSERT_CALL(EOS_EOF, _dragondos_readdir(path, &entry));
	}
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT_CALL(EOS_PNNF, _dragondos_open(&path, TEST_FILE("NOPE"), FAM_READ));
	remove(TEST_IMAGE);
}

static void test_create_existing(void)
{
	dragondos_path_id path;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 1000, 2));
	ASSERT_CALL(EOS_FAE, _dragondos_create(&path, TEST_FILE("FILE"), RW | FAM_NOCREATE));
	ASSERT("the file is intact", file_matches(TEST_FILE("FILE"), 1000, 2));
	ASSERT_CALL(0, save(TEST_FILE("file"), 300, 3));
	ASSERT("the file is replaced", file_matches(TEST_FILE("FILE"), 300, 3));
	ASSERT_EQUALS(720 - 36 - 2, free_sectors());
	ASSERT_CALL(EOS_BPNAM, _dragondos_create(&path, TEST_IMAGE ",", RW));
	ASSERT_CALL(EOS_BPNAM, _dragondos_create(&path, TEST_FILE("B@D"), RW));
	remove(TEST_IMAGE);
}

static void test_protection(void)
{
	dragondos_path_id path;
	dragondos_file_stat stat;
	u_int size = 1;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 100, 2));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), RW));
	ASSERT_CALL(0, _dragondos_gs_fd(path, &stat));
	ASSERT_EQUALS(0, stat.protected_file);
	ASSERT_EQUALS(100, stat.file_size);
	stat.protected_file = 1;
	ASSERT_CALL(0, _dragondos_ss_fd(path, &stat));
	ASSERT_CALL(EOS_FNA, _dragondos_write(path, data, &size));
	ASSERT_CALL(EOS_FNA, _dragondos_ss_size(path, 0));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_EQUALS(DRAGONDOS_FLAG_PROTECTED, entry_flags(0));
	ASSERT_CALL(EOS_FNA, _dragondos_kill(TEST_FILE("FILE")));
	ASSERT_CALL(EOS_FNA, _dragondos_rename(TEST_FILE("FILE"), "NEW"));
	ASSERT_CALL(EOS_FNA, save(TEST_FILE("FILE"), 1, 1));
	ASSERT("the file is intact", file_matches(TEST_FILE("FILE"), 100, 2));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), RW));
	stat.protected_file = 0;
	ASSERT_CALL(0, _dragondos_ss_fd(path, &stat));
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("FILE")));
	remove(TEST_IMAGE);
}

static void test_rename(void)
{
	dragondos_path_id path;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("ONE.BAS"), 100, 1));
	ASSERT_CALL(0, save(TEST_FILE("TWO.BAS"), 200, 2));
	ASSERT_CALL(EOS_FAE, _dragondos_rename(TEST_FILE("ONE.BAS"), "two.bas"));
	ASSERT_CALL(EOS_BPNAM, _dragondos_rename(TEST_FILE("ONE.BAS"), "B@D"));
	ASSERT_CALL(EOS_PNNF, _dragondos_rename(TEST_FILE("NOPE.BAS"), "NEW.BAS"));
	ASSERT_CALL(0, _dragondos_rename(TEST_FILE("ONE.BAS"), "new.dat"));
	ASSERT_CALL(EOS_PNNF, _dragondos_open(&path, TEST_FILE("ONE.BAS"), FAM_READ));
	ASSERT("the renamed file reads back", file_matches(TEST_FILE("NEW.DAT"), 100, 1));
	remove(TEST_IMAGE);
}

static void test_seek_and_overwrite(void)
{
	dragondos_path_id path;
	u_int size = 4;
	u_int position;
	u_char buffer[8];

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 1000, 2));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), RW));
	ASSERT_CALL(0, _dragondos_seek(path, 254, SEEK_SET));
	ASSERT_CALL(0, _dragondos_write(path, "WXYZ", &size));
	ASSERT_CALL(0, _dragondos_gs_pos(path, &position));
	ASSERT_EQUALS(258, position);
	ASSERT_CALL(EOS_SE, _dragondos_seek(path, -1, SEEK_SET));
	ASSERT_CALL(0, _dragondos_seek(path, -6, SEEK_CUR));
	ASSERT_CALL(0, _dragondos_gs_pos(path, &position));
	ASSERT_EQUALS(252, position);
	size = 8;
	ASSERT_CALL(0, _dragondos_read(path, buffer, &size));
	ASSERT_EQUALS(8, size);
	fill_pattern(1000, 2);
	ASSERT_MEM_EQUALS(data + 252, buffer, 2);
	ASSERT_MEM_EQUALS("WXYZ", buffer + 2, 4);
	ASSERT_MEM_EQUALS(data + 258, buffer + 6, 2);
	ASSERT_CALL(0, _dragondos_seek(path, -1, SEEK_END));
	ASSERT_CALL(0, _dragondos_gs_pos(path, &position));
	ASSERT_EQUALS(999, position);
	ASSERT_CALL(0, _dragondos_gs_size(path, &size));
	ASSERT_EQUALS(1000, size);
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

/* Growing by a seek past the end must allocate, without writing what lies
   between, so a sector missing there doesn't matter. */
static void test_write_past_end(void)
{
	dragondos_path_id path;
	u_int size = 3;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("FILE"), RW));
	ASSERT_CALL(0, _dragondos_seek(path, 1000, SEEK_SET));
	ASSERT_CALL(0, _dragondos_write(path, "abc", &size));
	ASSERT_CALL(0, _dragondos_gs_size(path, &size));
	ASSERT_EQUALS(1003, size);
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT_EQUALS(720 - 36 - 4, free_sectors());

	ASSERT_CALL(0, read_file(TEST_IMAGE, image, sizeof(image)) > 0 ? 0 : 1);
	ASSERT_MEM_EQUALS("abc", image + (324 + 3) * 256L + 1000 % 256, 3);
	remove(TEST_IMAGE);
}

static void test_set_size(void)
{
	dragondos_path_id path;
	u_int blank;
	u_int size;
	int i;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 80, 2));
	blank = free_sectors();

	/* A fragmented file, so that shrinking crosses entries. */
	ASSERT_CALL(0, save(TEST_FILE("BIG"), 0, 0));
	for (i = 0; i < 40; i++)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d"), i);
		ASSERT_CALL(0, save(pathlist, 256, (u_char)i));
	}
	for (i = 0; i < 40; i += 2)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d"), i);
		ASSERT_CALL(0, _dragondos_kill(pathlist));
	}
	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("BIG"), RW));
	size = 40000;
	fill_pattern(size, 4);
	ASSERT_CALL(0, _dragondos_write(path, data, &size));
	ASSERT_CALL(0, _dragondos_close(path));

	/* Shrink to one sector and a bit. */
	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("BIG"), RW));
	ASSERT_CALL(0, _dragondos_ss_size(path, 300));
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT("the start of the file remains", file_matches(TEST_FILE("BIG"), 300, 4));
	ASSERT_EQUALS(blank - 2 - 20, free_sectors());

	/* Grow it again, leaving its new sectors as they were. */
	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("BIG"), RW));
	ASSERT_CALL(0, _dragondos_ss_size(path, 1024));
	ASSERT_CALL(0, _dragondos_gs_size(path, &size));
	ASSERT_EQUALS(1024, size);
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT_EQUALS(blank - 4 - 20, free_sectors());

	/* Cut it to nothing. */
	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("BIG"), RW));
	ASSERT_CALL(0, _dragondos_ss_size(path, 0));
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT("nothing remains", file_matches(TEST_FILE("BIG"), 0, 0));
	ASSERT_EQUALS(blank - 20, free_sectors());
	remove(TEST_IMAGE);
}

static void test_readln(void)
{
	dragondos_path_id path;
	char line[32];
	u_int size = 10;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("TEXT"), RW));
	ASSERT_CALL(0, _dragondos_write(path, "ONE\rTWO\rTH", &size));
	ASSERT_CALL(0, _dragondos_seek(path, 0, SEEK_SET));

	size = sizeof(line);
	ASSERT_CALL(0, _dragondos_readln(path, line, &size));
	ASSERT_EQUALS(4, size);
	ASSERT_MEM_EQUALS("ONE\r", line, 4);

	size = 2;
	ASSERT_CALL(0, _dragondos_readln(path, line, &size));
	ASSERT_EQUALS(2, size);
	ASSERT_MEM_EQUALS("TW", line, 2);

	size = sizeof(line);
	ASSERT_CALL(0, _dragondos_readln(path, line, &size));
	ASSERT_EQUALS(2, size);
	size = sizeof(line);
	ASSERT_CALL(0, _dragondos_readln(path, line, &size));
	ASSERT_EQUALS(2, size);
	ASSERT_MEM_EQUALS("TH", line, 2);
	size = sizeof(line);
	ASSERT_CALL(EOS_EOF, _dragondos_readln(path, line, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

static void test_access_modes(void)
{
	dragondos_path_id path;
	u_char sector[256];
	u_int size = 1;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 100, 2));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), FAM_READ));
	ASSERT_CALL(EOS_BMODE, _dragondos_write(path, data, &size));
	ASSERT_CALL(EOS_BMODE, _dragondos_ss_size(path, 0));
	ASSERT_CALL(EOS_BMODE, _dragondos_ss_sector(path, 0, sector));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), FAM_WRITE));
	size = 1;
	ASSERT_CALL(EOS_BMODE, _dragondos_read(path, sector, &size));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_IMAGE ",", RW));
	size = 1;
	ASSERT_CALL(EOS_BMODE, _dragondos_read(path, sector, &size));
	size = 1;
	ASSERT_CALL(EOS_BMODE, _dragondos_write(path, data, &size));
	ASSERT_CALL(EOS_BMODE, _dragondos_seek(path, 0, SEEK_SET));
	memset(sector, 0xA5, sizeof(sector));
	ASSERT_CALL(0, _dragondos_ss_sector(path, 5, sector));
	memset(sector, 0, sizeof(sector));
	ASSERT_CALL(0, _dragondos_gs_sector(path, 5, sector));
	ASSERT_EQUALS(0xA5, sector[100]);
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

static void test_write_protected_image(void)
{
	dragondos_path_id path;
	long size;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_VDK, 40, 1));
	size = read_file(TEST_IMAGE, image, sizeof(image));
	image[10] |= 1;
	write_file(TEST_IMAGE, image, size);

	ASSERT_CALL(EOS_WP, _dragondos_create(&path, TEST_FILE("FILE"), RW));
	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("FILE"), RW | DISKIMAGE_IGNORE_WRITE_PROTECT));
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

static void test_not_dragondos(void)
{
	dragondos_path_id path;

	/* A blank image, and then a bitmap whose check bytes are wrong. */
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 40, 256, 18, 0, 0, 0xFF));
	ASSERT_CALL(EOS_WT, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));
	remove(TEST_IMAGE);

	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_EQUALS(720 * 256, read_file(TEST_IMAGE, image, sizeof(image)));
	image[SS_DIRECTORY * 256L + 0xFE] ^= 1;
	image[(SS_DIRECTORY - 72) * 256L + 0xFE] ^= 1;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_CALL(EOS_WT, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));

	/* Only a sectors per track of 18 or 36 is a Dragon DOS disk. */
	image[SS_DIRECTORY * 256L + 0xFE] ^= 1;
	image[(SS_DIRECTORY - 72) * 256L + 0xFE] ^= 1;
	image[SS_DIRECTORY * 256L + 0xFD] = 17;
	image[SS_DIRECTORY * 256L + 0xFF] = (u_char)~17;
	image[(SS_DIRECTORY - 72) * 256L + 0xFD] = 17;
	image[(SS_DIRECTORY - 72) * 256L + 0xFF] = (u_char)~17;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_CALL(EOS_WT, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));
	remove(TEST_IMAGE);

	ASSERT_CALL(EOS_PNNF, _dragondos_open(&path, "dragondos_missing.dsk,", FAM_READ));
}

/* A directory entry chain that loops must not hang. */
static void test_corrupt_chain(void)
{
	dragondos_path_id path;
	long offset = (SS_DIRECTORY + 2) * 256L;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("LOOP"), 10, 1));
	ASSERT_EQUALS(720 * 256, read_file(TEST_IMAGE, image, sizeof(image)));
	image[offset] |= DRAGONDOS_FLAG_CONTINUED;
	image[offset + 24] = 0;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_CALL(EOS_WT, _dragondos_open(&path, TEST_FILE("LOOP"), FAM_READ));

	image[offset + 24] = 200;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_CALL(EOS_WT, _dragondos_open(&path, TEST_FILE("LOOP"), FAM_READ));
	remove(TEST_IMAGE);
}

/* Remove a sector from a DMK image by deleting its ID pointer. */
static long dmk_size;

static void dmk_load(void)
{
	dmk_size = read_file(TEST_IMAGE, image, sizeof(image));
}

static void dmk_remove_sector(u_int cylinder, u_int sector)
{
	long track_length = image[2] | (image[3] << 8);
	u_char *track = image + 16 + cylinder * track_length;
	int i;

	for (i = 0; i < 64; i++)
	{
		long pointer = (track[i * 2] | (track[i * 2 + 1] << 8)) & 0x3FFF;

		if (pointer != 0 && track[pointer + 3] == sector)
		{
			memmove(track + i * 2, track + (i + 1) * 2, (size_t)(63 - i) * 2);
			track[126] = track[127] = 0;
			return;
		}
	}
}

static void dmk_save(void)
{
	write_file(TEST_IMAGE, image, dmk_size);
}

static void test_missing_sectors_unneeded_for_read(void)
{
	dragondos_path_id path;
	dragondos_dir_entry entry;
	u_int cylinder, sector;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_DMK, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 2));

	/* Leave only the file's sectors, and what's needed to find them. */
	dmk_load();
	for (cylinder = 0; cylinder < 40; cylinder++)
	{
		for (sector = 1; sector <= 18; sector++)
		{
			u_int lsn = cylinder * 18 + sector - 1;

			if (lsn == 324 || lsn == 325 || lsn == SS_DIRECTORY || lsn == SS_DIRECTORY + 2)
			{
				continue;
			}
			dmk_remove_sector(cylinder, sector);
		}
	}
	dmk_save();

	ASSERT("the file reads back", file_matches(TEST_FILE("FILE"), 300, 2));

	/* The rest of the image is out of reach, not a problem until touched. */
	ASSERT_CALL(0, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));
	ASSERT_CALL(0, _dragondos_readdir(path, &entry));
	ASSERT_CALL(EOS_EOF, _dragondos_readdir(path, &entry));
	ASSERT_CALL(EOS_SECT, _dragondos_gs_sector(path, 0, data));
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

static void test_missing_sector_stops_a_read(void)
{
	dragondos_path_id path;
	u_int size = 300;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_DMK, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 2));
	dmk_load();
	dmk_remove_sector(18, 2);	/* LSN 325, the file's second sector */
	dmk_save();

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), FAM_READ));
	ASSERT_CALL(EOS_SECT, _dragondos_read(path, back, &size));
	ASSERT_EQUALS(256, size);
	fill_pattern(300, 2);
	ASSERT_MEM_EQUALS(data, back, 256);
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

/* The directory track's copy stands in for it. */
static void test_directory_falls_back_to_copy(void)
{
	dragondos_path_id path;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_DMK, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 2));
	dmk_load();
	dmk_remove_sector(20, 1);
	dmk_remove_sector(20, 3);
	dmk_save();
	ASSERT("the file reads back", file_matches(TEST_FILE("FILE"), 300, 2));

	/* Writes reach the copy that's there. */
	ASSERT_CALL(0, save(TEST_FILE("TWO"), 100, 3));
	ASSERT("the new file reads back", file_matches(TEST_FILE("TWO"), 100, 3));
	ASSERT("the old file reads back", file_matches(TEST_FILE("FILE"), 300, 2));

	/* Without either copy of the first bitmap sector there's no geometry. */
	dmk_load();
	dmk_remove_sector(16, 1);
	dmk_save();
	ASSERT_CALL(EOS_SECT, _dragondos_open(&path, TEST_FILE("FILE"), FAM_READ));
	remove(TEST_IMAGE);
}

static void test_write_without_copy_track(void)
{
	u_int sector;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_DMK, 40, 1));
	dmk_load();
	for (sector = 1; sector <= 18; sector++)
	{
		dmk_remove_sector(16, sector);
	}
	dmk_save();
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 2));
	ASSERT("the file reads back", file_matches(TEST_FILE("FILE"), 300, 2));
	remove(TEST_IMAGE);
}

/* Single sided, the bitmap's second sector and directory sectors past the end
   of the listing don't matter, even when allocating. */
static void test_write_needs_only_what_it_touches(void)
{
	u_int cylinder, sector;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_DMK, 40, 1));
	dmk_load();
	for (cylinder = 0; cylinder < 40; cylinder++)
	{
		for (sector = 1; sector <= 18; sector++)
		{
			u_int lsn = cylinder * 18 + sector - 1;

			if (lsn == SS_DIRECTORY || lsn == SS_DIRECTORY + 2 || lsn == 324 || lsn == 325)
			{
				continue;
			}
			dmk_remove_sector(cylinder, sector);
		}
	}
	dmk_save();

	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 2));
	ASSERT("the file reads back", file_matches(TEST_FILE("FILE"), 300, 2));
	remove(TEST_IMAGE);
}

/* An allocated sector the image lacks fails the write, and undoes its growth. */
static void test_write_to_missing_sector(void)
{
	dragondos_path_id path;
	u_int size = 600;
	u_int blank;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_DMK, 40, 1));
	blank = free_sectors();
	dmk_load();
	dmk_remove_sector(18, 2);	/* LSN 325 */
	dmk_save();

	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("FILE"), RW));
	fill_pattern(size, 1);
	ASSERT_CALL(EOS_SECT, _dragondos_write(path, data, &size));
	ASSERT_EQUALS(256, size);
	ASSERT_CALL(0, _dragondos_gs_size(path, &size));
	ASSERT_EQUALS(256, size);
	ASSERT_CALL(0, _dragondos_close(path));
	ASSERT_EQUALS(blank - 1, free_sectors());
	ASSERT("what was written reads back", file_matches(TEST_FILE("FILE"), 256, 1));
	remove(TEST_IMAGE);
}

/* A raw image cut short still works for what's left of it. */
static void test_short_image(void)
{
	long size;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 300, 2));
	size = read_file(TEST_IMAGE, image, sizeof(image));
	ASSERT_EQUALS(720 * 256, size);

	/* Keep up to the first directory sector.  A 40 track bitmap, 21 tracks. */
	write_file(TEST_IMAGE, image, (SS_DIRECTORY + 3) * 256L);
	ASSERT("the file reads back", file_matches(TEST_FILE("FILE"), 300, 2));
	remove(TEST_IMAGE);
}

/* Whole file layouts.  Each of these is the directory the Dragon DOS 1.0 ROM
   makes of the same writes, with its write, close and kill routines run in an
   emulator. */
typedef struct
{
	int flags;
	int last_or_next;
	u_int extents[7][2];
} expected_entry;

static void check_directory(const expected_entry *expected, int count, u_int free_count)
{
	dragondos_path_id path;
	dragondos_dir_entry entry;
	u_int sectors;
	int i, e;

	ASSERT_CALL(0, _dragondos_open(&path, TEST_IMAGE ",", FAM_READ));
	for (i = 0; i < count; i++)
	{
		ASSERT_CALL(0, _dragondos_readdir(path, &entry));
		ASSERT_EQUALS(expected[i].flags, entry.flags);
		if (entry.flags & DRAGONDOS_FLAG_DELETED)
		{
			continue;
		}
		ASSERT_EQUALS(expected[i].last_or_next, entry.last_or_next);
		for (e = 0; e < ((entry.flags & DRAGONDOS_FLAG_CONTINUATION) ? 7 : 4); e++)
		{
			const dragondos_extent *extent = (entry.flags & DRAGONDOS_FLAG_CONTINUATION) ? entry.u.continuation.extent : entry.u.header.extent;

			ASSERT_EQUALS(expected[i].extents[e][1], extent[e].count);
			if (extent[e].count != 0)
			{
				ASSERT_EQUALS(expected[i].extents[e][0], (u_int)((extent[e].lsn[0] << 8) | extent[e].lsn[1]));
			}
		}
	}
	ASSERT_CALL(EOS_EOF, _dragondos_readdir(path, &entry));
	ASSERT_CALL(0, _dragondos_gs_free(path, &sectors));
	ASSERT_EQUALS(free_count, sectors);
	ASSERT_CALL(0, _dragondos_close(path));
}

/* Write to a file in pieces of what a Dragon can hold at once. */
static error_code write_in_pieces(const char *pathlist, int create, u_int position, u_int pieces, u_int piece)
{
	dragondos_path_id path;
	error_code ec = create ? _dragondos_create(&path, pathlist, RW) : _dragondos_open(&path, pathlist, RW);
	u_int i;

	if (ec != 0)
	{
		return ec;
	}
	ec = _dragondos_seek(path, (long)position, SEEK_SET);
	for (i = 0; ec == 0 && i < pieces; i++)
	{
		u_int size = piece;

		fill_pattern(size, (u_char)i);
		ec = _dragondos_write(path, data, &size);
	}
	{
		error_code close_ec = _dragondos_close(path);

		return ec != 0 ? ec : close_ec;
	}
}

static void test_layout_of_a_fragmented_disk_matches_rom(void)
{
	static const expected_entry expected[] =
	{
		{ 0x00, 0, { { 324, 3 } } },
		{ 0x20, 3, { { 252, 36 }, { 180, 36 }, { 108, 36 }, { 0, 36 } } },
		{ 0x81, 0, { { 0 } } },
		{ 0x01, 0, { { 378, 144 } } },
		{ 0x00, 0, { { 144, 3 } } },
		{ 0x81, 0, { { 0 } } },
		{ 0x00, 0, { { 72, 3 } } },
		{ 0x00, 10, { { 36, 4 } } }
	};
	int i;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	for (i = 0; i < 8; i++)
	{
		char pathlist[64];

		sprintf(pathlist, TEST_FILE("F%d.BIN"), i);
		ASSERT_CALL(0, write_in_pieces(pathlist, 1, 0, 1, 256 * 3));
	}
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("F1.BIN")));
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("F3.BIN")));
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("F5.BIN")));
	ASSERT_CALL(0, write_in_pieces(TEST_FILE("BIG.BIN"), 1, 0, 3, 0x6000));
	ASSERT_CALL(0, write_in_pieces(TEST_FILE("F7.BIN"), 0, 768, 1, 10));
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("F2.BIN")));

	check_directory(expected, 8, 383);
	remove(TEST_IMAGE);
}

/* An extent stops at 255 sectors, and sectors above LSN 1440 are freed. */
static void test_layout_of_a_large_disk_matches_rom(void)
{
	static const expected_entry expected[] =
	{
		{ 0x00, 112, { { 684, 36 }, { 648, 36 }, { 612, 10 } } },
		{ 0x81, 0, { { 0 } } },
		{ 0x81, 0, { { 0 } } },
		{ 0x81, 0, { { 0 } } },
		{ 0x00, 232, { { 1044, 4 } } }
	};
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 80, 2));
	ASSERT_CALL(0, write_in_pieces(TEST_FILE("HUGE.BIN"), 1, 0, 10, 0x6000));
	ASSERT_CALL(0, write_in_pieces(TEST_FILE("MORE.BIN"), 1, 0, 1, 1000));
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("HUGE.BIN")));
	ASSERT_CALL(0, write_in_pieces(TEST_FILE("LAST.BIN"), 1, 0, 1, 70000 % 0x6000));

	check_directory(expected, 5, 2758);
	remove(TEST_IMAGE);
}

/* The bitmap's sector above LSN 1440 is the one changed, not the sector before
   the directory as in the ROM. */
static void test_freeing_high_sectors_changes_only_the_bitmap(void)
{
	long size;
	long before = (DS_DIRECTORY - 1) * 256L;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 80, 2));
	ASSERT_CALL(0, write_in_pieces(TEST_FILE("HUGE.BIN"), 1, 0, 10, 0x6000));
	size = read_file(TEST_IMAGE, other, sizeof(other));
	ASSERT_EQUALS(2880 * 256, size);
	ASSERT_CALL(0, _dragondos_kill(TEST_FILE("HUGE.BIN")));
	ASSERT_EQUALS(2880 - 36, free_sectors());
	ASSERT_EQUALS(size, read_file(TEST_IMAGE, image, sizeof(image)));
	ASSERT_MEM_EQUALS(other + before, image + before, 256);
	remove(TEST_IMAGE);
}

/* The last write wins, however the ROM's buffers would have dealt with it. */
static void test_whole_sector_write_over_a_partial_one(void)
{
	dragondos_path_id path;
	u_int size;
	int i;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, _dragondos_create(&path, TEST_FILE("FILE"), RW));
	size = 300;
	memset(data, 0x11, size);
	ASSERT_CALL(0, _dragondos_write(path, data, &size));
	ASSERT_CALL(0, _dragondos_seek(path, 256, SEEK_SET));
	size = 256;
	memset(data, 0x22, size);
	ASSERT_CALL(0, _dragondos_write(path, data, &size));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, _dragondos_open(&path, TEST_FILE("FILE"), FAM_READ));
	size = 512;
	ASSERT_CALL(0, _dragondos_read(path, back, &size));
	ASSERT_EQUALS(512, size);
	for (i = 0; i < 512; i++)
	{
		ASSERT_EQUALS(i < 256 ? 0x11 : 0x22, back[i]);
	}
	ASSERT_CALL(0, _dragondos_close(path));
	remove(TEST_IMAGE);
}

/* Every result here is what the routine at $8F67 of the Dragon BASIC ROM gives,
   with Dragon DOS's keywords added. */
static const struct
{
	const char *text;
	const char *tokens;
} entoken_cases[] =
{
	{ "PRINT\"HI\"", "\x87\x22\x48\x49\x22" },
	{ "?\"HI\"", "\x87\x22\x48\x49\x22" },
	{ "GOTO 10", "\x81\xbc\x20\x31\x30" },
	{ "GOSUB 10", "\x81\xbd\x20\x31\x30" },
	{ "A=1:'NOTE PRINT", "\x41\xcb\x31\x3a\x3a\x83\x4e\x4f\x54\x45\x20\x50\x52\x49\x4e\x54" },
	{ "IF A THEN B ELSE C", "\x85\x20\x41\x20\xbf\x20\x42\x20\x3a\x84\x20\x43" },
	{ "A:ELSE", "\x41\x3a\x3a\x84" },
	{ "BTHEN", "\x42\x54\x48\x45\x4e" },
	{ "B THEN", "\x42\x20\xbf" },
	{ "A1THEN", "\x41\x31\x54\x48\x45\x4e" },
	{ "aTHEN", "\x61\xbf" },
	{ "print", "\x70\x72\x69\x6e\x74" },
	{ "DATA A,PRINT:PRINT", "\x86\x20\x41\x2c\x50\x52\x49\x4e\x54\x3a\x87" },
	{ "DATA \"A:B\",PRINT", "\x86\x20\x22\x41\x3a\x42\x22\x2c\x50\x52\x49\x4e\x54" },
	{ "REM PRINT:PRINT", "\x82\x20\x50\x52\x49\x4e\x54\x3a\x50\x52\x49\x4e\x54" },
	{ "PRINT\"UNCLOSED", "\x87\x22\x55\x4e\x43\x4c\x4f\x53\x45\x44" },
	{ "A$=CHR$(65)", "\x41\x24\xcb\xff\x91\x28\x36\x35\x29" },
	{ "FOR I=1 TO 2 STEP 3:NEXT", "\x80\x20\x49\xcb\x31\x20\xbc\x20\x32\x20\xc1\x20\x33\x3a\x8b" },
	{ "SWAP A,B", "\xe7\x20\x41\x2c\x42" },
	{ "USING", "\xcd" },
	{ "AUTO", "\xce" },
	{ "DSKINIT", "\xd7" },
	{ "ERROR", "\xda" },
	{ "ERR", "\xff\xa5" },
	{ "ERL", "\xff\xa4" },
	{ "FREE", "\xff\xa3" },
	{ "FRE$", "\xff\xa8" },
	{ "FRE", "\x46\x52\x45" },
	{ "LOF(1)", "\xff\xa2\x28\x31\x29" },
	{ "HIMEM", "\xff\xa6" },
	{ "USR0", "\xff\xa1\x30" },
	{ "TAB(1)", "\xbb\x31\x29" },
	{ "A>=B<C", "\x41\xca\xcb\x42\xcc\x43" },
	{ "1+2-3*4/5^6", "\x31\xc3\x32\xc4\x33\xc5\x34\xc6\x35\xc7\x36" },
	{ "PRINT  A ; B", "\x87\x20\x20\x41\x20\x3b\x20\x42" },
	{ "TOT", "\xbc\x54" },
	{ "ON X GOTO 1", "\x88\x20\x58\x20\x81\xbc\x20\x31" },
	{ "SGN(1)", "\xff\x80\x28\x31\x29" },
	{ "FN A(1)", "\xbe\x20\x41\x28\x31\x29" },
};

static void test_entoken_matches_rom(void)
{
	size_t i;

	for (i = 0; i < sizeof(entoken_cases) / sizeof(entoken_cases[0]); i++)
	{
		char line[128];
		u_char *out;
		u_int size;
		size_t expected = strlen(entoken_cases[i].tokens);

		sprintf(line, "10 %s\n", entoken_cases[i].text);
		call_result = _dragondos_entoken((u_char *)line, (u_int)strlen(line), &out, &size, DRAGONDOS_BASIC_LOAD_ADDRESS);
		ASSERT(entoken_cases[i].text, call_result == 0);
		ASSERT(entoken_cases[i].text, size == 9 + 4 + expected + 1 + 2);
		ASSERT(entoken_cases[i].text, memcmp(out + 9 + 4, entoken_cases[i].tokens, expected) == 0);
		free(out);
	}
}

/* The same as BASIC saved to the MAME disk. */
static void test_entoken_makes_a_saved_program(void)
{
	static const char text[] = "10 PRINT \"HELLO WORLD\"\n20 GOTO 10\n";
	u_char saved[41];
	u_char *out;
	u_int size = sizeof(saved);
	dragondos_path_id path;

	ASSERT_CALL(0, _dragondos_open(&path, MAME_IMAGE ",HW1.BAS", FAM_READ));
	ASSERT_CALL(0, _dragondos_read(path, saved, &size));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, _dragondos_entoken((const u_char *)text, sizeof(text) - 1, &out, &size, DRAGONDOS_BASIC_LOAD_ADDRESS));
	ASSERT_EQUALS(41, size);
	ASSERT_MEM_EQUALS(saved, out, 41);
	ASSERT_CALL(0, _dragondos_detect_tokenized(out, size));
	free(out);
}

static void test_detoken_lists_a_saved_program(void)
{
	u_char saved[41];
	char *text;
	u_int size = sizeof(saved);
	dragondos_path_id path;
	static const char expected[] = "10 PRINT \"HELLO WORLD\"\n20 GOTO 10\n";

	ASSERT_CALL(0, _dragondos_open(&path, MAME_IMAGE ",HW1.BAS", FAM_READ));
	ASSERT_CALL(0, _dragondos_read(path, saved, &size));
	ASSERT_CALL(0, _dragondos_close(path));

	ASSERT_CALL(0, _dragondos_detect_tokenized(saved, size));
	ASSERT_CALL(0, _dragondos_detoken(saved, size, &text, &size));
	ASSERT_EQUALS(sizeof(expected) - 1, size);
	ASSERT_MEM_EQUALS(expected, text, size);
	free(text);

	/* A program as BASIC holds it, without the header. */
	size = sizeof(saved) - 9;
	ASSERT_CALL(0, _dragondos_detoken(saved + 9, size, &text, &size));
	ASSERT_EQUALS(sizeof(expected) - 1, size);
	ASSERT_MEM_EQUALS(expected, text, size);
	free(text);
}

static void test_detoken_special_tokens(void)
{
	/* Colons that ELSE and an apostrophe carry aren't shown, and ones of the
	   program's own are.  Tokens past each table's end show as "!", in BASIC too. */
	static const u_char program[] =
	{
		0x24, 0x10, 0x00, 0x0A,
		0x41, 0x3A, 0x84, 0x42, 0x3A, 0x83, 0x43, 0x3A, 0x41, 0x00,
		0x24, 0x18, 0x00, 0x14,
		0xE7, 0xE8, 0xFE, 0xFF, 0xA8, 0xFF, 0xA9, 0x00,
		0x00, 0x00
	};
	static const char expected[] = "10 AELSEB'C:A\n20 SWAP!!FRE$!\n";
	char *text;
	u_int size;

	ASSERT_CALL(0, _dragondos_detoken(program, sizeof(program), &text, &size));
	ASSERT_EQUALS(sizeof(expected) - 1, size);
	ASSERT_MEM_EQUALS(expected, text, size);
	free(text);
}

static void test_detoken_refuses_what_is_not_basic(void)
{
	static const u_char machine_code[] = { 0x55, 0x02, 0x30, 0x00, 0x00, 0x01, 0x30, 0x00, 0xAA, 0x39 };
	static const u_char too_long[] = { 0x55, 0x01, 0x24, 0x01, 0x00, 0x20, 0x8B, 0x8D, 0xAA, 0x00, 0x00 };
	static const u_char no_end_of_line[] = { 0x24, 0x10, 0x00, 0x0A, 0x41 };
	static const u_char no_end_of_program[] = { 0x24, 0x10, 0x00, 0x0A, 0x41, 0x00, 0x24 };
	static const u_char cut_in_a_function[] = { 0x24, 0x10, 0x00, 0x0A, 0xFF };
	char *text;
	u_int size;

	ASSERT_CALL(EOS_WT, _dragondos_detoken(machine_code, sizeof(machine_code), &text, &size));
	ASSERT_CALL(EOS_SN, _dragondos_detect_tokenized(machine_code, sizeof(machine_code)));
	ASSERT_CALL(EOS_SN, _dragondos_detect_tokenized(too_long, sizeof(too_long)));
	ASSERT_CALL(EOS_SN, _dragondos_detoken(too_long, sizeof(too_long), &text, &size));
	ASSERT_CALL(EOS_SN, _dragondos_detoken(no_end_of_line, sizeof(no_end_of_line), &text, &size));
	ASSERT_CALL(EOS_SN, _dragondos_detoken(no_end_of_program, sizeof(no_end_of_program), &text, &size));
	ASSERT_CALL(EOS_SN, _dragondos_detoken(cut_in_a_function, sizeof(cut_in_a_function), &text, &size));
	ASSERT_CALL(EOS_SN, _dragondos_detoken(no_end_of_line, 0, &text, &size));
}

static void test_entoken_lines(void)
{
	static const struct
	{
		const char *text;
		error_code ec;
		const char *listed;
	} cases[] =
	{
		{ "10 A\r\n20 B\r\n", 0, "10 A\n20 B\n" },
		{ "10 A\r20 B", 0, "10 A\n20 B\n" },
		{ "\n\n  10   A  \n\n20 B\n", 0, "10 A  \n20 B\n" },
		{ "10 A\n15\n15 \n20 B\n", 0, "10 A\n20 B\n" },
		{ "63999 A\n", 0, "63999 A\n" },
		{ "0 A\n", 0, "0 A\n" },
		{ "10 \tA\n", 0, "10 A\n" },
		{ "64000 A\n", EOS_SN, NULL },
		{ "A=1\n", EOS_SN, NULL },
		{ "20 A\n10 B\n", EOS_SN, NULL },
		{ "10 A\n10 B\n", EOS_SN, NULL },
		{ "", EOS_SN, NULL },
		{ "\n \n", EOS_SN, NULL },
		{ "10\n", EOS_SN, NULL }
	};
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		u_char *out;
		char *text;
		u_int size, text_size;

		call_result = _dragondos_entoken((const u_char *)cases[i].text, (u_int)strlen(cases[i].text), &out, &size, DRAGONDOS_BASIC_LOAD_ADDRESS);
		ASSERT(cases[i].text, call_result == cases[i].ec);
		if (cases[i].ec == 0)
		{
			call_result = _dragondos_detoken(out, size, &text, &text_size);
			ASSERT(cases[i].text, call_result == 0);
			ASSERT(cases[i].text, text_size == strlen(cases[i].listed));
			ASSERT(cases[i].text, memcmp(text, cases[i].listed, text_size) == 0);
			free(text);
			free(out);
		}
	}
}

/* Links follow the load address, and the worst line for growth fits. */
static void test_entoken_load_address_and_growth(void)
{
	static char text[8192];
	u_char *out;
	u_int size;
	int i, position;

	ASSERT_CALL(0, _dragondos_entoken((const u_char *)"10 A\n", 5, &out, &size, 0x1E01));
	ASSERT_EQUALS(0x1E, out[2]);
	ASSERT_EQUALS(0x01, out[3]);
	ASSERT_EQUALS(0x1E, out[9]);
	ASSERT_EQUALS(0x07, out[10]);
	free(out);

	position = sprintf(text, "1 ");
	for (i = 0; i < 4000; i++)
	{
		text[position++] = '\'';
	}
	/* Only the first is a token: the rest are its comment. */
	ASSERT_CALL(0, _dragondos_entoken((const u_char *)text, (u_int)position, &out, &size, DRAGONDOS_BASIC_LOAD_ADDRESS));
	ASSERT_EQUALS(9 + 4 + 2 + 3999 + 1 + 2, size);
	free(out);

	position = 0;
	for (i = 1; i <= 1000; i++)
	{
		position += sprintf(text + position, "%d '\n", i);
	}
	ASSERT_CALL(0, _dragondos_entoken((const u_char *)text, (u_int)position, &out, &size, DRAGONDOS_BASIC_LOAD_ADDRESS));
	free(out);
}

static const char *const blank_formats[] = { "raw", "vdk", "dmk", "sdf", "jvc" };

static _path_type identify(char *pathlist)
{
	_path_type type = NATIVE;

	call_result = _coco_identify_image(pathlist, &type);
	return type;
}

static void test_coco_identifies_dragondos(void)
{
	static const diskimage_format formats[] =
	{
		DISKIMAGE_RAW, DISKIMAGE_VDK, DISKIMAGE_DMK, DISKIMAGE_SDF, DISKIMAGE_JVC
	};
	size_t i;
	u_int sides;

	ASSERT_EQUALS(DRAGONDOS, identify(MAME_IMAGE ","));
	ASSERT_EQUALS(DRAGONDOS, identify(MAME_IMAGE ",HW1.BAS"));

	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
	{
		for (sides = 1; sides <= 2; sides++)
		{
			remove(TEST_IMAGE);
			ASSERT(blank_formats[i], _dragondos_dskinit(TEST_IMAGE, formats[i], 40, sides) == 0);
			ASSERT(blank_formats[i], identify(TEST_IMAGE ",") == DRAGONDOS);
			remove(TEST_IMAGE);
		}
	}
}

static void test_coco_still_identifies_others(void)
{
	long offset = SS_DIRECTORY * 256L;

	ASSERT_EQUALS(NATIVE, identify("no_comma_here.dsk"));

	/* A drive number means Disk BASIC, as does an empty disk. */
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_EQUALS(DECB, identify(TEST_IMAGE ",FILE:1"));
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _decb_dskini(TEST_IMAGE, 35, "", 1, 256, 0));
	ASSERT_EQUALS(DECB, identify(TEST_IMAGE ","));
	remove(TEST_IMAGE);

	/* Check bytes alone don't make a Dragon DOS disk: the bitmap must have
	   the directory track in use, in either copy it is read from. */
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_EQUALS(720 * 256, read_file(TEST_IMAGE, image, sizeof(image)));
	image[offset + SS_DIRECTORY / 8 + 1] = 0x01;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_EQUALS(DECB, identify(TEST_IMAGE ","));

	/* Too few tracks to hold the directory. */
	image[offset + SS_DIRECTORY / 8 + 1] = 0;
	image[offset + 0xFC] = 20;
	image[offset + 0xFE] = (u_char)~20;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_EQUALS(DECB, identify(TEST_IMAGE ","));
	image[offset + 0xFC] = 40;
	image[offset + 0xFE] = (u_char)~40;

	/* OS-9's marker is looked for first. */
	memset(image, 0, 256);
	image[10] = 1;
	image[2 * 256] = 0x2E;
	image[2 * 256 + 1] = 0xAE;
	image[2 * 256 + 32] = 0xAE;
	write_file(TEST_IMAGE, image, 720 * 256);
	ASSERT_EQUALS(OS9, identify(TEST_IMAGE ","));
	remove(TEST_IMAGE);
}

static void test_coco_file_operations(void)
{
	coco_path_id path;
	coco_file_stat stat;
	coco_dir_entry entry;
	u_char name[32];
	u_int size = 1000;
	u_int position;
	int attributes;
	char line[32];

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_VDK, 40, 1));
	memset(&stat, 0, sizeof(stat));

	ASSERT_CALL(0, _coco_create(&path, TEST_FILE("FILE.DAT"), RW, &stat));
	ASSERT_EQUALS(DRAGONDOS, path->type);
	fill_pattern(size, 6);
	ASSERT_CALL(0, _coco_write(path, data, &size));
	ASSERT_EQUALS(1000, size);
	ASSERT_CALL(0, _coco_gs_pos(path, &position));
	ASSERT_EQUALS(1000, position);
	ASSERT_CALL(0, _coco_close(path));
	ASSERT("the file reads back", file_matches(TEST_FILE("FILE.DAT"), 1000, 6));

	ASSERT_CALL(0, _coco_gs_size_pathlist(TEST_FILE("FILE.DAT"), &size));
	ASSERT_EQUALS(1000, size);

	ASSERT_CALL(0, _coco_open(&path, TEST_FILE("FILE.DAT"), RW));
	ASSERT_CALL(0, _coco_seek(path, 100, SEEK_SET));
	size = 50;
	ASSERT_CALL(0, _coco_read(path, back, &size));
	ASSERT_MEM_EQUALS(data + 100, back, 50);
	ASSERT_EQUALS(0, _coco_gs_eof(path));
	ASSERT_CALL(0, _coco_ss_size(path, 10));
	ASSERT_CALL(0, _coco_gs_size(path, &size));
	ASSERT_EQUALS(10, size);
	ASSERT_CALL(0, _coco_seek(path, 0, SEEK_END));
	ASSERT_CALL(EOS_EOF, _coco_gs_eof(path));
	size = 4;
	ASSERT_CALL(0, _coco_write(path, "A\rB\r", &size));
	ASSERT_CALL(0, _coco_seek(path, 10, SEEK_SET));
	size = sizeof(line);
	ASSERT_CALL(0, _coco_readln(path, line, &size));
	ASSERT_EQUALS(2, size);
	ASSERT_CALL(0, _coco_close(path));

	/* Renaming hands back the entry. */
	ASSERT_CALL(0, _coco_rename_ex(TEST_FILE("FILE.DAT"), "NEW.BIN", &entry));
	ASSERT_MEM_EQUALS("NEW\0\0\0\0\0BIN", entry.dentry.dragondos.u.header.filename, 11);
	ASSERT_CALL(0, _coco_rename(TEST_FILE("NEW.BIN"), "LAST.BIN"));

	ASSERT_CALL(EOS_FNA, _coco_makdir(TEST_FILE("DIR")));
	ASSERT_CALL(EOS_BPNAM, _coco_delete_directory(TEST_FILE("LAST.BIN")));

	/* Listing a directory reports each file, and not what surrounds them. */
	ASSERT_CALL(0, save(TEST_FILE("GONE"), 10, 1));
	ASSERT_CALL(0, save(TEST_FILE("KEEP"), 20, 2));
	ASSERT_CALL(0, _coco_delete(TEST_FILE("GONE")));
	ASSERT_CALL(0, _coco_open(&path, TEST_IMAGE ",", FAM_READ));
	ASSERT_CALL(0, _coco_readdir(path, &entry));
	ASSERT_EQUALS(DRAGONDOS, entry.type);
	ASSERT_CALL(0, _coco_ncpy_name(&entry, name, sizeof(name)));
	ASSERT_STRING_EQUALS("LAST.BIN", (char *)name);
	ASSERT_CALL(0, _coco_readdir(path, &entry));
	ASSERT_CALL(0, _coco_ncpy_name(&entry, name, sizeof(name)));
	ASSERT_STRING_EQUALS("KEEP", (char *)name);
	ASSERT_CALL(EOS_EOF, _coco_readdir(path, &entry));

	/* A raw path is the root directory. */
	ASSERT_CALL(0, _coco_gs_fd(path, &stat));
	ASSERT_EQUALS(FAP_DIR, stat.attributes & FAP_DIR);
	ASSERT_CALL(0, _coco_gs_attr(path, &attributes));
	ASSERT_EQUALS(FAP_DIR, attributes & FAP_DIR);
	ASSERT_CALL(0, _coco_close(path));
	ASSERT_CALL(0, _coco_open(&path, TEST_IMAGE ",@", FAM_READ));
	ASSERT_EQUALS(1, path->path.dragondos->israw);
	ASSERT_CALL(0, _coco_close(path));
	remove(TEST_IMAGE);
}

/* Protecting a file is the one permission Dragon DOS has. */
static void test_coco_protection(void)
{
	coco_path_id path;
	coco_file_stat stat;
	int attributes;
	u_int size = 1;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_CALL(0, save(TEST_FILE("FILE"), 100, 2));

	ASSERT_CALL(0, _coco_open(&path, TEST_FILE("FILE"), RW));
	ASSERT_CALL(0, _coco_gs_attr(path, &attributes));
	ASSERT_EQUALS(FAP_READ | FAP_WRITE | FAP_PREAD, attributes);
	ASSERT_CALL(0, _coco_ss_attr(path, FAP_READ | FAP_PREAD));
	ASSERT_CALL(0, _coco_gs_fd(path, &stat));
	ASSERT_EQUALS(FAP_READ | FAP_PREAD, stat.attributes);
	ASSERT_CALL(EOS_FNA, _coco_write(path, data, &size));
	stat.attributes |= FAP_WRITE;
	ASSERT_CALL(0, _coco_ss_fd(path, &stat));
	size = 1;
	ASSERT_CALL(0, _coco_write(path, data, &size));
	ASSERT_CALL(0, _coco_close(path));
	remove(TEST_IMAGE);
}

/* The generic copy moves a file on and off a disk by way of libcoco. */
static void test_coco_copy_through_toolshed(void)
{
	static char buffer[512];
	const char *native = "dragondos_native.bin";
	const char *copied = "dragondos_copied.bin";
	long size;

	remove(TEST_IMAGE);
	remove(native);
	remove(copied);
	ASSERT_CALL(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_VDK, 40, 1));
	fill_pattern(5000, 8);
	write_file(native, data, 5000);

	ASSERT_CALL(0, TSCopyFile((char *)native, TEST_FILE("COPY.BIN"), 0, 1, 0, 0, buffer, sizeof(buffer)));
	ASSERT("the copy reads back", file_matches(TEST_FILE("COPY.BIN"), 5000, 8));
	ASSERT_CALL(0, TSCopyFile(TEST_FILE("COPY.BIN"), (char *)copied, 0, 1, 0, 0, buffer, sizeof(buffer)));
	size = read_file(copied, back, sizeof(back));
	ASSERT_EQUALS(5000, size);
	fill_pattern(5000, 8);
	ASSERT_MEM_EQUALS(data, back, 5000);

	remove(native);
	remove(copied);
	remove(TEST_IMAGE);
}

int main(void)
{
	RUN(test_allocator_matches_rom);
	RUN(test_allocator_stays_on_disk);
	RUN(test_allocator_ignores_second_sector_when_single_sided);
	RUN(test_parse_names);
	RUN(test_read_mame_disk);
	RUN(test_reproduce_mame_disk);
	RUN(test_dskinit_bitmaps);
	RUN(test_dskinit_arguments);
	RUN(test_all_formats);
	RUN(test_double_sided_placement);
	RUN(test_raw_double_sided_placement);
	RUN(test_sizes_at_sector_boundaries);
	RUN(test_small_writes_match_one_write);
	RUN(test_continuation_entries);
	RUN(test_disk_full);
	RUN(test_directory_full);
	RUN(test_create_existing);
	RUN(test_protection);
	RUN(test_rename);
	RUN(test_seek_and_overwrite);
	RUN(test_write_past_end);
	RUN(test_set_size);
	RUN(test_readln);
	RUN(test_access_modes);
	RUN(test_write_protected_image);
	RUN(test_not_dragondos);
	RUN(test_corrupt_chain);
	RUN(test_missing_sectors_unneeded_for_read);
	RUN(test_missing_sector_stops_a_read);
	RUN(test_directory_falls_back_to_copy);
	RUN(test_write_without_copy_track);
	RUN(test_write_needs_only_what_it_touches);
	RUN(test_write_to_missing_sector);
	RUN(test_short_image);
	RUN(test_layout_of_a_fragmented_disk_matches_rom);
	RUN(test_layout_of_a_large_disk_matches_rom);
	RUN(test_freeing_high_sectors_changes_only_the_bitmap);
	RUN(test_whole_sector_write_over_a_partial_one);
	RUN(test_entoken_matches_rom);
	RUN(test_entoken_makes_a_saved_program);
	RUN(test_detoken_lists_a_saved_program);
	RUN(test_detoken_special_tokens);
	RUN(test_detoken_refuses_what_is_not_basic);
	RUN(test_entoken_lines);
	RUN(test_entoken_load_address_and_growth);
	RUN(test_coco_identifies_dragondos);
	RUN(test_coco_still_identifies_others);
	RUN(test_coco_file_operations);
	RUN(test_coco_protection);
	RUN(test_coco_copy_through_toolshed);
	return TEST_REPORT();
}

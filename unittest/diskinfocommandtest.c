/* Tests for the diskinfo command. */

#include <string.h>

#include "tinytest.h"
#include "cocopath.h"
#include "diskimage.h"

#define TEST_IMAGE "diskinfo_test.dsk"
#define TEST_OTHER "diskinfo_other.dsk"
#define OUTPUT_FILE "diskinfo_output.txt"
#define ERRORS_FILE "diskinfo_errors.txt"

#define DMK_TRACK_LENGTH 0x1900
#define DMK_FLAG_SINGLE_SIDED 0x10
#define DMK_FIRST_SECTOR 128

#define MARK_NORMAL 0xFB
#define MARK_DELETED 0xF8

static u_char image[1024 * 1024];
static char output[16384];
static char errors[1024];

static int dmk_tracks;
static int dmk_sides;
static u_char *dmk_track;
static int dmk_pos;
static int dmk_slot;

/* ASSERT_EQUALS evaluates its second argument twice, so calls with side
   effects keep their result here first. */
static int call_result;
#define ASSERT_CALL(expected, call) \
	{ \
		call_result = (call); \
		ASSERT_EQUALS(expected, call_result); \
	}

static unsigned short crc16(unsigned short crc, const u_char *data, int size)
{
	int i, bit;

	for (i = 0; i < size; i++)
	{
		crc ^= (unsigned short)(data[i] << 8);
		for (bit = 0; bit < 8; bit++)
		{
			crc = (crc & 0x8000) ? (unsigned short)((crc << 1) ^ 0x1021) : (unsigned short)(crc << 1);
		}
	}
	return crc;
}

static void read_file(const char *pathname, char *buffer, size_t size)
{
	FILE *fp = fopen(pathname, "rb");
	size_t got = 0;

	if (fp != NULL)
	{
		got = fread(buffer, 1, size - 1, fp);
		fclose(fp);
	}
	buffer[got] = '\0';
}

/* Run diskinfo, keeping what it prints, and return its result. */
static int run_diskinfo(const char *arguments)
{
	char command[512];
	int result;

	snprintf(command, sizeof(command), "../diskinfo/diskinfo %s > %s 2> %s", arguments, OUTPUT_FILE, ERRORS_FILE);
	result = system(command);
	read_file(OUTPUT_FILE, output, sizeof(output));
	read_file(ERRORS_FILE, errors, sizeof(errors));
	remove(OUTPUT_FILE);
	remove(ERRORS_FILE);

	return result;
}

static void dmk_begin(int tracks, int sides)
{
	long size = 16 + (long)tracks * sides * DMK_TRACK_LENGTH;

	memset(image, 0, size);
	image[1] = (u_char)tracks;
	image[2] = DMK_TRACK_LENGTH & 0xFF;
	image[3] = DMK_TRACK_LENGTH >> 8;
	image[4] = sides == 1 ? DMK_FLAG_SINGLE_SIDED : 0;
	dmk_tracks = tracks;
	dmk_sides = sides;
}

static void dmk_select_track(int cylinder, int side)
{
	dmk_track = &image[16 + (long)(cylinder * dmk_sides + side) * DMK_TRACK_LENGTH];
	dmk_pos = DMK_FIRST_SECTOR;
	dmk_slot = 0;
}

/* Record a double density sector on the selected track. */
static void dmk_add_sector(int cylinder, int side, int id, int size_code, u_char mark, int bad_data_crc)
{
	int size = 128 << size_code;
	unsigned int pointer;
	unsigned short crc;
	u_char *field;

	memset(&dmk_track[dmk_pos], 0xA1, 3);
	dmk_pos += 3;
	pointer = dmk_pos | 0x8000;
	dmk_track[dmk_slot * 2] = pointer & 0xFF;
	dmk_track[dmk_slot * 2 + 1] = (u_char)(pointer >> 8);
	dmk_slot++;

	field = &dmk_track[dmk_pos];
	field[0] = 0xFE;
	field[1] = (u_char)cylinder;
	field[2] = (u_char)side;
	field[3] = (u_char)id;
	field[4] = (u_char)size_code;
	crc = crc16(0xCDB4, field, 5);
	field[5] = (u_char)(crc >> 8);
	field[6] = (u_char)crc;
	dmk_pos += 7;

	memset(&dmk_track[dmk_pos], 0x4E, 22);
	dmk_pos += 22 + 12;
	memset(&dmk_track[dmk_pos], 0xA1, 3);
	dmk_pos += 3;

	field = &dmk_track[dmk_pos];
	field[0] = mark;
	memset(&field[1], 0xE5, size);
	crc = crc16(0xCDB4, field, 1 + size);
	if (bad_data_crc)
	{
		crc ^= 0xFFFF;
	}
	field[1 + size] = (u_char)(crc >> 8);
	field[2 + size] = (u_char)crc;
	dmk_pos += 3 + size;

	memset(&dmk_track[dmk_pos], 0x4E, 24);
	dmk_pos += 24;
}

static void dmk_add_full_track(int cylinder, int side)
{
	int id;

	dmk_select_track(cylinder, side);
	for (id = 1; id <= 18; id++)
	{
		dmk_add_sector(cylinder, side, id, 1, MARK_NORMAL, 0);
	}
}

static void dmk_write(void)
{
	FILE *fp = fopen(TEST_IMAGE, "wb");

	fwrite(image, 1, 16 + (long)dmk_tracks * dmk_sides * DMK_TRACK_LENGTH, fp);
	fclose(fp);
}

static void test_dmk_report(void)
{
	int cylinder, id;

	dmk_begin(35, 1);
	for (cylinder = 0; cylinder < 35; cylinder++)
	{
		if (cylinder != 17 && cylinder != 18)
		{
			dmk_add_full_track(cylinder, 0);
		}
	}
	dmk_select_track(17, 0);
	dmk_add_sector(17, 0, 4, 2, MARK_NORMAL, 0);
	dmk_select_track(18, 0);
	for (id = 1; id <= 18; id++)
	{
		dmk_add_sector(18, 0, id, 1, id == 3 ? MARK_DELETED : MARK_NORMAL, id == 17);
	}
	dmk_write();

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);

	ASSERT_STRING_EQUALS(
		TEST_IMAGE " is a DMK disk image.\n"
		"Tracks 0 to 16 are 18 sectors (1-18), 256 bytes each.\n"
		"Track 17 is 1 512 byte sector (4).\n"
		"Track 18 is 18 sectors (1-18), 256 bytes each. Sector 3 has a deleted data mark. "
		"Sector 17 has a CRC error in its data segment.\n"
		"Tracks 19 to 34 are 18 sectors (1-18), 256 bytes each.\n",
		output);
}

static void test_not_a_disk_image(void)
{
	FILE *fp = fopen("diskinfo_file.txt", "wb");

	fputs("This is not a disk image.\n", fp);
	fclose(fp);

	ASSERT_NEQUALS(0, run_diskinfo("diskinfo_file.txt"));
	remove("diskinfo_file.txt");

	ASSERT_STRING_EQUALS("diskinfo_file.txt is not a DMK, SDF, JVC, VDK, nor raw disk image.\n", output);
}

static void check_created(diskimage_format format, const char *name, u_int sides, const char *tracks)
{
	char expected[256];

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, sides, 35, 256, 18, 0, 0, 0xFF));
	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);

	snprintf(expected, sizeof(expected), "%s is %s disk image.\n%s\n", TEST_IMAGE, name, tracks);
	ASSERT_STRING_EQUALS(expected, output);
}

static void test_each_format(void)
{
	const char *one_side = "Tracks 0 to 34 are 18 sectors (1-18), 256 bytes each.";
	const char *two_sides = "Tracks 0 to 34, both sides, are 18 sectors (1-18), 256 bytes each.";

	check_created(DISKIMAGE_RAW, "a raw", 1, one_side);
	check_created(DISKIMAGE_JVC, "a JVC", 2, two_sides);
	check_created(DISKIMAGE_DMK, "a DMK", 2, two_sides);
	check_created(DISKIMAGE_SDF, "an SDF", 1, one_side);
	check_created(DISKIMAGE_VDK, "a VDK", 1, one_side);
}

static void test_short_track0(void)
{
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 10, 0, 0xFF));
	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);
	ASSERT_STRING_EQUALS(
		TEST_IMAGE " is a raw disk image.\n"
		"Track 0 is 10 sectors (1-10), 256 bytes each.\n"
		"Tracks 1 to 34 are 18 sectors (1-18), 256 bytes each.\n",
		output);

	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 2, 3, 256, 18, 10, 0, 0xFF));
	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);
	ASSERT_STRING_EQUALS(
		TEST_IMAGE " is a DMK disk image.\n"
		"Track 0, side 0, is 10 sectors (1-10), 256 bytes each.\n"
		"Track 0, side 1, is 18 sectors (1-18), 256 bytes each.\n"
		"Tracks 1 to 2, both sides, are 18 sectors (1-18), 256 bytes each.\n",
		output);
}

static void test_track_layouts(void)
{
	dmk_begin(8, 1);
	dmk_add_full_track(0, 0);

	/* Tracks 1 and 2 have no sectors, nor does track 7. */
	dmk_select_track(3, 0);
	dmk_add_sector(3, 0, 1, 1, MARK_NORMAL, 0);
	dmk_add_sector(3, 0, 2, 1, MARK_NORMAL, 0);
	dmk_add_sector(3, 0, 4, 2, MARK_NORMAL, 0);

	dmk_select_track(4, 0);
	dmk_add_sector(4, 0, 1, 1, MARK_NORMAL, 0);
	dmk_add_sector(4, 0, 3, 1, MARK_NORMAL, 0);
	dmk_add_sector(4, 0, 4, 1, MARK_NORMAL, 0);
	dmk_add_sector(4, 0, 5, 1, MARK_NORMAL, 0);
	dmk_add_sector(4, 0, 7, 1, MARK_NORMAL, 0);

	dmk_select_track(5, 0);
	dmk_add_sector(5, 0, 1, 0, MARK_NORMAL, 0);
	dmk_add_sector(5, 0, 2, 1, MARK_NORMAL, 0);
	dmk_add_sector(5, 0, 3, 2, MARK_NORMAL, 0);

	/* Recorded out of order: sector numbers are reported in ascending order. */
	dmk_select_track(6, 0);
	dmk_add_sector(6, 0, 2, 1, MARK_DELETED, 1);
	dmk_add_sector(6, 0, 1, 1, MARK_NORMAL, 0);
	dmk_write();

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);

	ASSERT_STRING_EQUALS(
		TEST_IMAGE " is a DMK disk image.\n"
		"Track 0 is 18 sectors (1-18), 256 bytes each.\n"
		"Tracks 1 to 2 have no sectors.\n"
		"Track 3 is 2 sectors (1-2), 256 bytes each, and 1 512 byte sector (4).\n"
		"Track 4 is 5 sectors (1, 3-5, 7), 256 bytes each.\n"
		"Track 5 is 1 128 byte sector (1), 1 256 byte sector (2), and 1 512 byte sector (3).\n"
		"Track 6 is 2 sectors (1-2), 256 bytes each. "
		"Sector 2 has a deleted data mark and a CRC error in its data segment.\n"
		"Track 7 has no sectors.\n",
		output);
}

static void test_tracks_differ_only_in_condition(void)
{
	int id;

	dmk_begin(2, 1);
	dmk_add_full_track(0, 0);
	dmk_select_track(1, 0);
	for (id = 1; id <= 18; id++)
	{
		dmk_add_sector(1, 0, id, 1, MARK_NORMAL, id == 9);
	}
	dmk_write();

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);

	ASSERT_STRING_EQUALS(
		TEST_IMAGE " is a DMK disk image.\n"
		"Track 0 is 18 sectors (1-18), 256 bytes each.\n"
		"Track 1 is 18 sectors (1-18), 256 bytes each. Sector 9 has a CRC error in its data segment.\n",
		output);
}

static void test_empty_raw_image(void)
{
	FILE *fp = fopen(TEST_IMAGE, "wb");

	fclose(fp);

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	remove(TEST_IMAGE);

	ASSERT_STRING_EQUALS(TEST_IMAGE " is a raw disk image.\nIt has no tracks.\n", output);
}

static void test_missing_file(void)
{
	ASSERT_NEQUALS(0, run_diskinfo("diskinfo_missing.dsk"));

	ASSERT_STRING_EQUALS("", output);
	ASSERT("names the file", strstr(errors, "diskinfo_missing.dsk") != NULL);
}

static void test_several_files(void)
{
	remove(TEST_IMAGE);
	remove(TEST_OTHER);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 0, 0xFF));
	ASSERT_CALL(0, _diskimage_create(TEST_OTHER, DISKIMAGE_VDK, 1, 40, 256, 18, 0, 0, 0xFF));

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE " " TEST_OTHER));
	remove(TEST_IMAGE);
	remove(TEST_OTHER);

	ASSERT_STRING_EQUALS(
		TEST_IMAGE " is a raw disk image.\n"
		"Tracks 0 to 34 are 18 sectors (1-18), 256 bytes each.\n"
		"\n"
		TEST_OTHER " is a VDK disk image.\n"
		"Tracks 0 to 39 are 18 sectors (1-18), 256 bytes each.\n",
		output);
}

static void test_one_bad_file_sets_status(void)
{
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 1, 256, 18, 0, 0, 0xFF));

	ASSERT_NEQUALS(0, run_diskinfo("diskinfo_missing.dsk " TEST_IMAGE));
	remove(TEST_IMAGE);

	ASSERT("still reports the good file", strstr(output, "Track 0 is 18 sectors") != NULL);
}

static void test_usage(void)
{
	ASSERT_NEQUALS(0, run_diskinfo(""));
	ASSERT("usage on stderr", strstr(errors, "Syntax: diskinfo") != NULL);

	ASSERT_CALL(0, run_diskinfo("-h"));
	ASSERT("usage on request", strstr(errors, "Syntax: diskinfo") != NULL);

	ASSERT_NEQUALS(0, run_diskinfo("-x"));
	ASSERT("unknown option named", strstr(errors, "-x") != NULL);
}

static void test_set_write_protect(void)
{
	diskimage_geometry geometry;
	diskimage_id disk;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 2, 256, 18, 0, 0, 0xFF));

	ASSERT_CALL(0, run_diskinfo("-p " TEST_IMAGE));
	ASSERT("reported as protected", strstr(output, "is now write protected.") != NULL);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &geometry));
	ASSERT_EQUALS(1, geometry.write_protected);
	_diskimage_close(disk);

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE));
	ASSERT("the report says so", strstr(output, "It is write protected.\n") != NULL);

	ASSERT_CALL(0, run_diskinfo(TEST_IMAGE " -u"));
	ASSERT("reported as not protected", strstr(output, "no longer write protected.") != NULL);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &geometry));
	ASSERT_EQUALS(0, geometry.write_protected);
	_diskimage_close(disk);
	remove(TEST_IMAGE);

	/* A raw image has no flag to set. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 2, 256, 18, 0, 0, 0xFF));
	ASSERT_NEQUALS(0, run_diskinfo("-p " TEST_IMAGE));
	ASSERT("no flag reported", strstr(errors, "no write protect flag") != NULL);
	remove(TEST_IMAGE);
}

static void test_possible_filesystems(void)
{
	ASSERT_CALL(0, run_diskinfo("\"../../../unittest/testdata/os9 mame.dmk\""));
	ASSERT("OS-9 is named", strstr(output, "is a DMK disk image.\nPossible filesystems: OS-9\nTracks") != NULL);

	ASSERT_CALL(0, run_diskinfo("\"../../../unittest/testdata/dragondos mame.vdk\""));
	ASSERT("Dragon DOS is named", strstr(output, "Possible filesystems: Dragon DOS\n") != NULL);
}

int main(void)
{
	RUN(test_dmk_report);
	RUN(test_not_a_disk_image);
	RUN(test_each_format);
	RUN(test_short_track0);
	RUN(test_track_layouts);
	RUN(test_tracks_differ_only_in_condition);
	RUN(test_empty_raw_image);
	RUN(test_missing_file);
	RUN(test_several_files);
	RUN(test_one_bad_file_sets_status);
	RUN(test_usage);
	RUN(test_set_write_protect);

	RUN(test_possible_filesystems);
	return TEST_REPORT();
}

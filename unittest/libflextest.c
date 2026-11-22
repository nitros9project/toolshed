/* Tests for the FLEX library. */

#include <string.h>

#include "tinytest.h"
#include "diskimage.h"
#include "imagedamage.h"
#include "flexpath.h"
#include "cocopath.h"
#include "toolshed.h"
#include "../libflex/libflexint.h"

#define TEST_IMAGE "flex_test.dsk"
#define TEST_DMK "flex_test.dmk"
#define TEST_FILE(name) TEST_IMAGE "," name
#define RW (FAM_READ | FAM_WRITE)
#define MAX_TRACKS 256
#define MAX_SECTORS 256

static u_char data[512 * 1024];
static u_char back[512 * 1024];
static u_char sector[FLEX_SECTOR_SIZE];

/* What each sector of a disk is used for, from a check of the whole disk. */
static u_char usage[MAX_TRACKS][MAX_SECTORS];

/* GCC's -Wformat-truncation needs the result of snprintf checked. */
#define FORMAT(buffer, ...) \
	{ \
		int format_length = snprintf(buffer, sizeof(buffer), __VA_ARGS__); \
		ASSERT("the text fits", format_length > 0 && (size_t)format_length < sizeof(buffer)); \
	}

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
		data[i] = (u_char)(i * 7 + seed + (i >> 8));
	}
}

static error_code write_whole(const char *pathlist, const u_char *buffer, u_int size)
{
	flex_path_id path;
	error_code ec = _flex_create(&path, pathlist, FAM_WRITE);
	u_int count = size;

	if (ec != 0)
	{
		return ec;
	}
	ec = _flex_write(path, buffer, &count);
	if (ec == 0 && count != size)
	{
		ec = EOS_WRITE;
	}
	{
		error_code close_ec = _flex_close(path);

		return ec != 0 ? ec : close_ec;
	}
}

static error_code read_whole(const char *pathlist, u_char *buffer, u_int *size)
{
	flex_path_id path;
	error_code ec = _flex_open(&path, pathlist, FAM_READ);
	u_int count = *size;

	*size = 0;
	if (ec != 0)
	{
		return ec;
	}
	ec = _flex_read(path, buffer, &count);
	*size = count;
	_flex_close(path);
	return ec == EOS_EOF ? 0 : ec;
}

/* The data of a file holding a pattern reads back, padded with nuls to a
   whole sector. */
static int file_matches(const char *pathlist, u_int size, u_char seed)
{
	u_int sectors = (size + FLEX_DATA_SIZE - 1) / FLEX_DATA_SIZE;
	u_int got = sizeof(back);
	u_int i;

	if (sectors == 0)
	{
		sectors = 1;
	}
	if (read_whole(pathlist, back, &got) != 0 || got != sectors * FLEX_DATA_SIZE)
	{
		return 0;
	}
	fill_pattern(size, seed);
	if (memcmp(data, back, size) != 0)
	{
		return 0;
	}
	for (i = size; i < got; i++)
	{
		if (back[i] != 0)
		{
			return 0;
		}
	}
	return 1;
}

/* Check a whole disk: no sector is in more than one of the directory, a
   file or the free chain, each file's chain ends at its last sector and has
   as many sectors as its entry says, with record numbers from 1, and the free
   count is right.  On a disk made by libflex, every sector past track 0 is
   in one of them, and check_disk sees that too; check_chains doesn't, for
   disks made by hand.  Returns the number of files, or -1. */
#define USE_DIRECTORY 1
#define USE_FILE 2
#define USE_FREE 3

static int check_any_disk(const char *image, int every_sector)
{
	char pathlist[256];
	flex_path_id path;
	flex_dir_entry entry;
	flex_sir sir;
	u_int track, number, i, count;
	int files = 0;
	int ok = 1;

	count = (u_int)snprintf(pathlist, sizeof(pathlist), "%s,", image);
	if (count >= sizeof(pathlist) || _flex_open(&path, pathlist, FAM_READ) != 0)
	{
		return -1;
	}
	_flex_gs_sir(path, &sir);
	memset(usage, 0, sizeof(usage));

	/* The directory */
	track = 0;
	number = FLEX_DIR_SECTOR;
	for (count = 0; ok && count < 1000; count++)
	{
		if (_flex_gs_sector(path, track, number, sector) != 0 || usage[track][number] != 0)
		{
			ok = 0;
			break;
		}
		usage[track][number] = USE_DIRECTORY;
		if (sector[0] == 0 && sector[1] == 0)
		{
			break;
		}
		track = sector[0];
		number = sector[1];
	}

	/* The files */
	while (ok && _flex_readdir(path, &entry) == 0)
	{
		u_int sectors;

		if (!_flex_entry_in_use(&entry))
		{
			continue;
		}
		files++;
		sectors = _flex_entry_sectors(&entry);
		track = entry.start_track;
		number = entry.start_sector;
		for (i = 0; ok && i < sectors; i++)
		{
			if (_flex_gs_sector(path, track, number, sector) != 0 || usage[track][number] != 0)
			{
				ok = 0;
				break;
			}
			usage[track][number] = USE_FILE;
			if (!entry.random && ((sector[2] << 8) | sector[3]) != i + 1)
			{
				ok = 0;
			}
			if (i + 1 == sectors)
			{
				if (track != entry.end_track || number != entry.end_sector || sector[0] != 0 || sector[1] != 0)
				{
					ok = 0;
				}
			}
			track = sector[0];
			number = sector[1];
		}
	}

	/* The free chain */
	track = sir.free_track;
	number = sir.free_sector;
	count = 0;
	while (ok && !(track == 0 && number == 0))
	{
		if (_flex_gs_sector(path, track, number, sector) != 0 || usage[track][number] != 0)
		{
			ok = 0;
			break;
		}
		usage[track][number] = USE_FREE;
		count++;
		if (track == sir.last_free_track && number == sir.last_free_sector)
		{
			break;
		}
		track = sector[0];
		number = sector[1];
	}
	if (count != sir.free_sectors)
	{
		ok = 0;
	}

	for (track = 1; ok && every_sector && track <= sir.max_track; track++)
	{
		for (number = 1; number <= sir.max_sector; number++)
		{
			if (usage[track][number] == 0)
			{
				ok = 0;
			}
		}
	}

	_flex_close(path);
	return ok ? files : -1;
}

static int check_disk(const char *image)
{
	return check_any_disk(image, 1);
}

static int check_chains(const char *image)
{
	return check_any_disk(image, 0);
}

static void fresh_disk(const char *image, diskimage_format format, u_int tracks, u_int sides, int double_density)
{
	remove(image);
	_flex_dskinit(image, format, tracks, sides, double_density, 0, "TEST", 1);
}

static void test_parse_names(void)
{
	u_char name[11];

	ASSERT_EC(0, _fx_parse_name("FLEX.SYS", name));
	ASSERT_MEM_EQUALS("FLEX\0\0\0\0SYS", name, 11);
	ASSERT_EC(0, _fx_parse_name("a_b-9.t", name));
	ASSERT_MEM_EQUALS("A_B-9\0\0\0T\0\0", name, 11);
	ASSERT_EC(0, _fx_parse_name("1.NEWDISKA.CMD", name));
	ASSERT_MEM_EQUALS("NEWDISKACMD", name, 11);
	ASSERT_EC(0, _fx_parse_name("NOEXT", name));
	ASSERT_MEM_EQUALS("NOEXT\0\0\0\0\0\0", name, 11);
	ASSERT_EC(EOS_BPNAM, _fx_parse_name("", name));
	ASSERT_EC(EOS_BPNAM, _fx_parse_name("9START", name));
	ASSERT_EC(0, _fx_parse_name("toolongname.txt", name));
	ASSERT_MEM_EQUALS("TOOLONGNTXT", name, 11);
	ASSERT_EC(EOS_BPNAM, _fx_parse_name("NAME.LONG", name));
	ASSERT_EC(EOS_BPNAM, _fx_parse_name("BAD NAME", name));
	ASSERT_EC(EOS_BPNAM, _fx_parse_name("NAME.EXT.MORE", name));
}

/* A blank disk is laid out as NEWDISK lays it out. */
static void test_dskinit_layout(void)
{
	long size;
	u_int i;

	remove(TEST_IMAGE);
	ASSERT_EC(0, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0, 0, "WORK.DSK", 0x1234));
	size = read_file(TEST_IMAGE, data, sizeof(data));
	ASSERT_EQUALS(35 * 10 * 256, size);

	/* The SIR */
	ASSERT_MEM_EQUALS("WORK\0\0\0\0DSK", &data[2 * 256 + 0x10], 11);
	ASSERT_EQUALS(0x12, data[2 * 256 + 0x1B]);
	ASSERT_EQUALS(0x34, data[2 * 256 + 0x1C]);
	ASSERT_EQUALS(1, data[2 * 256 + 0x1D]);
	ASSERT_EQUALS(1, data[2 * 256 + 0x1E]);
	ASSERT_EQUALS(34, data[2 * 256 + 0x1F]);
	ASSERT_EQUALS(10, data[2 * 256 + 0x20]);
	ASSERT_EQUALS(340 >> 8, data[2 * 256 + 0x21]);
	ASSERT_EQUALS(340 & 0xFF, data[2 * 256 + 0x22]);
	ASSERT_EQUALS(34, data[2 * 256 + 0x26]);
	ASSERT_EQUALS(10, data[2 * 256 + 0x27]);

	/* The directory is sectors 5 to 10 of track 0. */
	for (i = 5; i < 10; i++)
	{
		ASSERT_EQUALS(0, data[(i - 1) * 256]);
		ASSERT_EQUALS(i + 1, data[(i - 1) * 256 + 1]);
	}
	ASSERT_EQUALS(0, data[9 * 256]);
	ASSERT_EQUALS(0, data[9 * 256 + 1]);

	/* Every other sector is free, in order. */
	ASSERT_EQUALS(1, data[10 * 256]);
	ASSERT_EQUALS(2, data[10 * 256 + 1]);
	ASSERT_EQUALS(2, data[19 * 256]);
	ASSERT_EQUALS(1, data[19 * 256 + 1]);
	ASSERT_EQUALS(0, data[349 * 256]);
	ASSERT_EQUALS(0, data[349 * 256 + 1]);
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));

	remove(TEST_IMAGE);
}

static void test_dskinit_sizes(void)
{
	long size;

	/* Raw images keep only the sectors track 0 has, unless padded. */
	remove(TEST_IMAGE);
	ASSERT_EC(0, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1, 1, 0, NULL, 1));
	size = read_file(TEST_IMAGE, data, sizeof(data));
	ASSERT_EQUALS((10 + 39 * 18) * 256, size);
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));

	remove(TEST_IMAGE);
	ASSERT_EC(0, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1, 1, 1, NULL, 1));
	size = read_file(TEST_IMAGE, data, sizeof(data));
	ASSERT_EQUALS(40 * 18 * 256, size);
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));

	remove(TEST_IMAGE);
	ASSERT_EC(0, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 2, 1, 0, NULL, 1));
	size = read_file(TEST_IMAGE, data, sizeof(data));
	ASSERT_EQUALS((20 + 39 * 36) * 256, size);
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));

	remove(TEST_IMAGE);
	ASSERT_EC(0, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 2, 1, 1, NULL, 1));
	size = read_file(TEST_IMAGE, data, sizeof(data));
	ASSERT_EQUALS(40 * 36 * 256, size);
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));

	/* A double sided directory fills both sides of track 0. */
	{
		flex_path_id path;
		u_char buffer[256];

		ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
		ASSERT_EC(0, _flex_gs_sector(path, 0, 19, buffer));
		ASSERT_EQUALS(0, buffer[0]);
		ASSERT_EQUALS(20, buffer[1]);
		ASSERT_EC(0, _flex_gs_sector(path, 0, 20, buffer));
		ASSERT_EQUALS(0, buffer[0]);
		ASSERT_EQUALS(0, buffer[1]);
		_flex_close(path);
	}

	remove(TEST_IMAGE);
	ASSERT_EC(EOS_IA, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 1, 1, 0, 0, NULL, 1));
	ASSERT_EC(EOS_IA, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 257, 1, 0, 0, NULL, 1));
	ASSERT_EC(EOS_IA, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 35, 3, 0, 0, NULL, 1));
	ASSERT_EC(EOS_BPNAM, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0, 0, "BAD NAME", 1));
	ASSERT_EC(EOS_IA, _flex_dskinit(TEST_IMAGE, DISKIMAGE_VDK, 35, 1, 0, 0, NULL, 1));
	ASSERT_EC(EOS_IA, _flex_dskinit(TEST_DMK, DISKIMAGE_SDF, 81, 1, 1, 0, NULL, 1));
	ASSERT_EQUALS(-1, read_file(TEST_IMAGE, data, sizeof(data)));
	ASSERT_EQUALS(-1, read_file(TEST_DMK, data, sizeof(data)));

	/* An existing file is not replaced. */
	write_file(TEST_IMAGE, (const u_char *)"keep", 4);
	ASSERT_EC(EOS_FAE, _flex_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0, 0, NULL, 1));
	ASSERT_EQUALS(4, read_file(TEST_IMAGE, data, sizeof(data)));
	remove(TEST_IMAGE);
}

/* Find what is recorded for a sector of a DMK image: whether its ID is
   double density, and its position in the file. */
static int dmk_sector_density(const char *image, u_int cylinder, u_int side, u_int number, int *found)
{
	u_char header[16];
	u_char pointers[128];
	FILE *fp = fopen(image, "rb");
	u_int sides, track_length, i;
	long track;

	*found = 0;
	if (fp == NULL || fread(header, 1, sizeof(header), fp) != sizeof(header))
	{
		if (fp != NULL)
		{
			fclose(fp);
		}
		return 0;
	}
	sides = (header[4] & 0x10) ? 1 : 2;
	track_length = header[2] | (header[3] << 8);
	track = 16 + (long)(cylinder * sides + side) * track_length;
	fseek(fp, track, SEEK_SET);
	if (fread(pointers, 1, sizeof(pointers), fp) != sizeof(pointers))
	{
		fclose(fp);
		return 0;
	}
	for (i = 0; i < 64; i++)
	{
		u_int pointer = pointers[i * 2] | (pointers[i * 2 + 1] << 8);
		int double_density = (pointer & 0x8000) != 0;
		u_char id[8];

		if (pointer == 0)
		{
			break;
		}
		fseek(fp, track + (pointer & 0x3FFF), SEEK_SET);
		if (fread(id, 1, sizeof(id), fp) != sizeof(id))
		{
			break;
		}
		if ((double_density ? id[3] : id[6]) == number)
		{
			*found = 1;
			fclose(fp);
			return double_density;
		}
	}
	fclose(fp);
	return 0;
}

/* DMK and SDF images have track 0 single density, and sector numbers that
   carry on across the second side. */
static void test_dskinit_records_flex_layout(void)
{
	diskimage_id disk;
	diskimage_geometry geometry;
	diskimage_sector_info info;
	int found, double_density;
	u_int i, index;
	const char *sdf = "flex_test.sdf";
	const char *images[2];

	images[0] = TEST_DMK;
	images[1] = sdf;

	remove(TEST_DMK);
	ASSERT_EC(0, _flex_dskinit(TEST_DMK, DISKIMAGE_DMK, 40, 1, 1, 0, NULL, 1));
	double_density = dmk_sector_density(TEST_DMK, 0, 0, 3, &found);
	ASSERT("track 0 has sector 3", found);
	ASSERT_EQUALS(0, double_density);
	double_density = dmk_sector_density(TEST_DMK, 1, 0, 18, &found);
	ASSERT("track 1 has sector 18", found);
	ASSERT_EQUALS(1, double_density);
	ASSERT_EQUALS(0, check_disk(TEST_DMK));

	/* A single density disk is single density throughout. */
	remove(TEST_DMK);
	ASSERT_EC(0, _flex_dskinit(TEST_DMK, DISKIMAGE_DMK, 35, 1, 0, 0, NULL, 1));
	double_density = dmk_sector_density(TEST_DMK, 34, 0, 10, &found);
	ASSERT("track 34 has sector 10", found);
	ASSERT_EQUALS(0, double_density);
	ASSERT_EQUALS(0, check_disk(TEST_DMK));

	remove(TEST_DMK);
	remove(sdf);
	ASSERT_EC(0, _flex_dskinit(TEST_DMK, DISKIMAGE_DMK, 40, 2, 1, 0, NULL, 1));
	ASSERT_EC(0, _flex_dskinit(sdf, DISKIMAGE_SDF, 40, 2, 1, 0, NULL, 1));
	for (i = 0; i < 2; i++)
	{
		ASSERT_EC(0, _diskimage_open(&disk, images[i], FAM_READ));
		ASSERT_EC(0, _diskimage_query_geometry(disk, &geometry));
		ASSERT_EQUALS(2, geometry.sides);
		ASSERT_EQUALS(10, geometry.sectors_track0);
		ASSERT_EQUALS(20 + 39 * 36, geometry.total_sectors);

		/* Track 0 side 1 is 11 to 20, track 1 side 1 is 19 to 36. */
		ASSERT_EC(0, _diskimage_query_sector(disk, 10, &info));
		ASSERT_EQUALS(0, info.cylinder);
		ASSERT_EQUALS(1, info.side);
		ASSERT_EQUALS(11, info.sector);
		index = 20 + 18;
		ASSERT_EC(0, _diskimage_query_sector(disk, index, &info));
		ASSERT_EQUALS(1, info.cylinder);
		ASSERT_EQUALS(1, info.side);
		ASSERT_EQUALS(19, info.sector);
		_diskimage_close(disk);
		ASSERT_EQUALS(0, check_disk(images[i]));
	}

	remove(TEST_DMK);
	remove(sdf);
}

/* What libdiskimage does for FLEX, other formats aren't asked to do. */
static void test_diskimage_create_flags(void)
{
	ASSERT_EC(EOS_IA, _diskimage_create_ex(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 10, 1, 0,
					       DISKIMAGE_CREATE_FM_TRACK0));
	ASSERT_EC(EOS_IA, _diskimage_create_ex(TEST_IMAGE, DISKIMAGE_JVC, 1, 35, 256, 10, 0, 1, 0,
					       DISKIMAGE_CREATE_SINGLE_DENSITY));
	ASSERT_EC(EOS_IA, _diskimage_create_ex(TEST_DMK, DISKIMAGE_DMK, 1, 35, 256, 18, 0, 1, 0, 0x80));
	/* Single density holds only so much. */
	ASSERT_EC(EOS_IA, _diskimage_create_ex(TEST_DMK, DISKIMAGE_DMK, 1, 35, 256, 18, 0, 1, 0,
					       DISKIMAGE_CREATE_SINGLE_DENSITY));
	ASSERT_EQUALS(-1, read_file(TEST_IMAGE, data, sizeof(data)));
	ASSERT_EQUALS(-1, read_file(TEST_DMK, data, sizeof(data)));

	/* Raw images record no numbers, so spanning sides changes nothing. */
	ASSERT_EC(0, _diskimage_create_ex(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 1, 0,
					  DISKIMAGE_CREATE_SPAN_SIDES));
	remove(TEST_IMAGE);
}

static void round_trip(const char *image, diskimage_format format, u_int tracks, u_int sides, int double_density)
{
	static const u_int sizes[] = { 0, 1, 251, 252, 253, 504, 5000, 20000 };
	char pathlist[256];
	u_int i;

	fresh_disk(image, format, tracks, sides, double_density);
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		FORMAT(pathlist, "%s,F%u.BIN", image, i);
		fill_pattern(sizes[i], (u_char)i);
		ASSERT_EC(0, write_whole(pathlist, data, sizes[i]));
	}
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		FORMAT(pathlist, "%s,F%u.BIN", image, i);
		ASSERT("the file reads back", file_matches(pathlist, sizes[i], (u_char)i));
	}
	ASSERT_EQUALS(8, check_disk(image));
	remove(image);
}

static void test_all_formats(void)
{
	round_trip(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	round_trip(TEST_IMAGE, DISKIMAGE_RAW, 40, 2, 1);
	round_trip("flex_test.jvc", DISKIMAGE_JVC, 40, 2, 1);
	round_trip("flex_test.jvc", DISKIMAGE_JVC, 35, 1, 0);
	round_trip("flex_test.vdk", DISKIMAGE_VDK, 40, 2, 1);
	round_trip(TEST_DMK, DISKIMAGE_DMK, 35, 1, 0);
	round_trip(TEST_DMK, DISKIMAGE_DMK, 40, 2, 1);
	round_trip("flex_test.sdf", DISKIMAGE_SDF, 40, 1, 1);
}

/* A file is written to the start of the free chain, its sectors numbered
   from 1, and with no sectors to spare. */
static void test_write_layout(void)
{
	flex_path_id path;
	flex_file_stat stat;
	u_char buffer[256];

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(600, 1);
	ASSERT_EC(0, write_whole(TEST_FILE("ONE.TXT"), data, 600));
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("ONE.TXT"), FAM_READ));
	ASSERT_EC(0, _flex_gs_fd(path, &stat));
	ASSERT_EQUALS(3, stat.sectors);
	ASSERT_EQUALS(3 * 252, stat.file_size);
	ASSERT_EQUALS(1, stat.start_track);
	ASSERT_EQUALS(1, stat.start_sector);
	ASSERT_EQUALS(1, stat.end_track);
	ASSERT_EQUALS(3, stat.end_sector);
	ASSERT_EQUALS(0, stat.random);
	ASSERT_EC(0, _flex_gs_sector(path, 1, 2, buffer));
	ASSERT_EQUALS(1, buffer[0]);
	ASSERT_EQUALS(3, buffer[1]);
	ASSERT_EQUALS(0, buffer[2]);
	ASSERT_EQUALS(2, buffer[3]);
	ASSERT_MEM_EQUALS(&data[252], &buffer[4], 252);
	_flex_close(path);

	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	{
		flex_sir sir;

		_flex_gs_sir(path, &sir);
		ASSERT_EQUALS(1, sir.free_track);
		ASSERT_EQUALS(4, sir.free_sector);
		ASSERT_EQUALS(337, sir.free_sectors);
	}
	_flex_close(path);
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

/* A file left empty still has a sector, as in FLEX. */
static void test_empty_file(void)
{
	flex_path_id path;
	u_int size;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	ASSERT_EC(0, _flex_create(&path, TEST_FILE("EMPTY.TXT"), FAM_WRITE));
	ASSERT_EC(0, _flex_close(path));
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("EMPTY.TXT"), FAM_READ));
	ASSERT_EC(0, _flex_gs_size(path, &size));
	ASSERT_EQUALS(252, size);
	_flex_close(path);
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

/* Deleting puts $FF at the start of the name, and the sectors at the end of
   the free chain. */
static void test_kill(void)
{
	flex_path_id path;
	flex_dir_entry entry;
	flex_sir sir;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(1000, 2);
	ASSERT_EC(0, write_whole(TEST_FILE("A.BIN"), data, 1000));
	ASSERT_EC(0, write_whole(TEST_FILE("B.BIN"), data, 100));
	ASSERT_EC(0, _flex_kill(TEST_FILE("a.bin")));
	ASSERT_EC(EOS_PNNF, _flex_kill(TEST_FILE("A.BIN")));
	ASSERT_EC(EOS_PNNF, _flex_open(&path, TEST_FILE("A.BIN"), FAM_READ));

	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	ASSERT_EC(0, _flex_readdir(path, &entry));
	ASSERT_EQUALS(0xFF, entry.filename[0]);
	ASSERT_MEM_EQUALS("BIN", entry.file_extension, 3);
	ASSERT_EQUALS(0, _flex_entry_in_use(&entry));
	_flex_gs_sir(path, &sir);
	ASSERT_EQUALS(1, sir.free_track);
	ASSERT_EQUALS(6, sir.free_sector);
	ASSERT_EQUALS(1, sir.last_free_track);
	ASSERT_EQUALS(4, sir.last_free_sector);
	ASSERT_EQUALS(339, sir.free_sectors);
	_flex_close(path);
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));

	/* The deleted entry is used again. */
	ASSERT_EC(0, write_whole(TEST_FILE("C.BIN"), data, 10));
	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	ASSERT_EC(0, _flex_readdir(path, &entry));
	ASSERT_MEM_EQUALS("C\0\0\0\0\0\0\0BIN", entry.filename, 11);
	_flex_close(path);
	ASSERT_EQUALS(2, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

/* Once the 60 entries of track 0 are used, the directory takes a sector from
   the free chain. */
static void test_directory_grows(void)
{
	char pathlist[64];
	flex_path_id path;
	u_char buffer[256];
	int i;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(10, 0);
	for (i = 0; i < 61; i++)
	{
		FORMAT(pathlist, TEST_FILE("F%d.TXT"), i);
		ASSERT_EC(0, write_whole(pathlist, data, 10));
	}
	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	ASSERT_EC(0, _flex_gs_sector(path, 0, 10, buffer));
	/* 60 files took sectors 1,1 to 6,10. */
	ASSERT_EQUALS(7, buffer[0]);
	ASSERT_EQUALS(1, buffer[1]);
	_flex_close(path);
	ASSERT_EQUALS(61, check_disk(TEST_IMAGE));
	for (i = 0; i < 61; i++)
	{
		FORMAT(pathlist, TEST_FILE("F%d.TXT"), i);
		fill_pattern(10, 0);
		ASSERT("each file reads back", file_matches(pathlist, 10, 0));
	}
	remove(TEST_IMAGE);
}

static void test_disk_full(void)
{
	flex_path_id path;
	u_int size = 341 * 252;
	u_int free_sectors;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(size, 3);
	ASSERT_EC(0, _flex_create(&path, TEST_FILE("BIG.BIN"), FAM_WRITE));
	ASSERT_EC(EOS_DF, _flex_write(path, data, &size));
	ASSERT_EQUALS(340 * 252, size);
	ASSERT_EC(0, _flex_gs_free(path, &free_sectors));
	ASSERT_EQUALS(0, free_sectors);
	_flex_close(path);
	ASSERT("what fit reads back", file_matches(TEST_FILE("BIG.BIN"), 340 * 252, 3));
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));

	ASSERT_EC(EOS_DF, write_whole(TEST_FILE("MORE.BIN"), data, 1));
	ASSERT_EC(0, _flex_kill(TEST_FILE("MORE.BIN")));
	ASSERT_EC(0, _flex_kill(TEST_FILE("BIG.BIN")));
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));
	ASSERT_EC(0, write_whole(TEST_FILE("AGAIN.BIN"), data, 340 * 252));
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

static void test_create_existing(void)
{
	flex_path_id path;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(2000, 4);
	ASSERT_EC(0, write_whole(TEST_FILE("SAME.BIN"), data, 2000));
	ASSERT_EC(EOS_FAE, _flex_create(&path, TEST_FILE("same.bin"), FAM_WRITE | FAM_NOCREATE));
	fill_pattern(300, 5);
	ASSERT_EC(0, write_whole(TEST_FILE("same.bin"), data, 300));
	ASSERT("the new file replaced it", file_matches(TEST_FILE("SAME.BIN"), 300, 5));
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

static void test_protection(void)
{
	flex_path_id path;
	flex_file_stat stat;
	u_int size = 10;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(500, 6);
	ASSERT_EC(0, write_whole(TEST_FILE("SAFE.TXT"), data, 500));

	ASSERT_EC(0, _flex_open(&path, TEST_FILE("SAFE.TXT"), RW));
	ASSERT_EC(0, _flex_gs_fd(path, &stat));
	stat.attributes = FLEX_ATTR_DELETE;
	ASSERT_EC(0, _flex_ss_fd(path, &stat));
	_flex_close(path);
	ASSERT_EC(EOS_FNA, _flex_kill(TEST_FILE("SAFE.TXT")));
	ASSERT_EC(EOS_FNA, _flex_rename(TEST_FILE("SAFE.TXT"), "OTHER"));
	ASSERT_EC(EOS_FNA, _flex_create(&path, TEST_FILE("SAFE.TXT"), FAM_WRITE));

	ASSERT_EC(0, _flex_open(&path, TEST_FILE("SAFE.TXT"), RW));
	stat.attributes = FLEX_ATTR_WRITE | 0x01;
	ASSERT_EC(0, _flex_ss_fd(path, &stat));
	ASSERT_EC(EOS_FNA, _flex_write(path, data, &size));
	ASSERT_EC(EOS_FNA, _flex_ss_size(path, 0));
	_flex_close(path);
	ASSERT_EC(EOS_FNA, _flex_kill(TEST_FILE("SAFE.TXT")));

	/* Bits FLEX doesn't define are kept. */
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("SAFE.TXT"), FAM_READ));
	ASSERT_EC(0, _flex_gs_fd(path, &stat));
	ASSERT_EQUALS(FLEX_ATTR_WRITE, stat.attributes);
	_flex_close(path);

	ASSERT_EC(0, _flex_open(&path, TEST_FILE("SAFE.TXT"), RW));
	stat.attributes = 0;
	ASSERT_EC(0, _flex_ss_fd(path, &stat));
	_flex_close(path);
	ASSERT("the file is as it was", file_matches(TEST_FILE("SAFE.TXT"), 500, 6));
	ASSERT_EC(0, _flex_kill(TEST_FILE("SAFE.TXT")));
	ASSERT_EQUALS(0, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

static void test_rename(void)
{
	flex_dir_entry entry;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(300, 7);
	ASSERT_EC(0, write_whole(TEST_FILE("OLD.TXT"), data, 300));
	ASSERT_EC(0, write_whole(TEST_FILE("TAKEN.TXT"), data, 10));

	/* The extension stays unless another is given. */
	ASSERT_EC(0, _flex_rename_ex(TEST_FILE("OLD.TXT"), "new", &entry));
	ASSERT_MEM_EQUALS("NEW\0\0\0\0\0TXT", entry.filename, 11);
	ASSERT("the file has its new name", file_matches(TEST_FILE("NEW.TXT"), 300, 7));
	ASSERT_EC(0, _flex_rename(TEST_FILE("NEW.TXT"), "NEW.BAK"));
	ASSERT("the file has its new extension", file_matches(TEST_FILE("NEW.BAK"), 300, 7));

	ASSERT_EC(EOS_FAE, _flex_rename(TEST_FILE("NEW.BAK"), "TAKEN.TXT"));
	ASSERT_EC(0, _flex_rename(TEST_FILE("NEW.BAK"), "new.bak"));
	ASSERT_EC(EOS_BPNAM, _flex_rename(TEST_FILE("NEW.BAK"), "BAD NAME"));
	ASSERT_EC(EOS_PNNF, _flex_rename(TEST_FILE("GONE.TXT"), "OTHER"));
	ASSERT_EQUALS(2, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

static void test_seek_and_overwrite(void)
{
	flex_path_id path;
	u_char buffer[16];
	u_int size, pos;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(1000, 8);
	ASSERT_EC(0, write_whole(TEST_FILE("S.BIN"), data, 1000));

	ASSERT_EC(0, _flex_open(&path, TEST_FILE("S.BIN"), RW));
	ASSERT_EC(0, _flex_seek(path, 500, SEEK_SET));
	size = 4;
	ASSERT_EC(0, _flex_write(path, "ABCD", &size));
	ASSERT_EC(0, _flex_seek(path, -10, SEEK_END));
	ASSERT_EC(0, _flex_gs_pos(path, &pos));
	ASSERT_EQUALS(4 * 252 - 10, pos);
	ASSERT_EC(EOS_SE, _flex_seek(path, -2000, SEEK_CUR));

	/* Back to an earlier sector, after the chain has moved on. */
	ASSERT_EC(0, _flex_seek(path, 250, SEEK_SET));
	size = 4;
	ASSERT_EC(0, _flex_read(path, buffer, &size));
	ASSERT_EQUALS(4, size);
	ASSERT_MEM_EQUALS(&data[250], buffer, 4);
	_flex_close(path);

	memcpy(&data[500], "ABCD", 4);
	size = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("S.BIN"), back, &size));
	ASSERT_MEM_EQUALS(data, back, 1000);
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

/* Writing past the end fills what is between with zeros. */
static void test_write_past_end(void)
{
	flex_path_id path;
	u_int size = 3, i;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	ASSERT_EC(0, _flex_create(&path, TEST_FILE("GAP.BIN"), FAM_WRITE));
	ASSERT_EC(0, _flex_seek(path, 1000, SEEK_SET));
	ASSERT_EC(0, _flex_write(path, "END", &size));
	_flex_close(path);

	size = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("GAP.BIN"), back, &size));
	ASSERT_EQUALS(4 * 252, size);
	for (i = 0; i < 1000; i++)
	{
		if (back[i] != 0)
		{
			break;
		}
	}
	ASSERT_EQUALS(1000, i);
	ASSERT_MEM_EQUALS("END", &back[1000], 3);
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

static void test_set_size(void)
{
	flex_path_id path;
	u_int size;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	fill_pattern(3000, 9);
	ASSERT_EC(0, write_whole(TEST_FILE("CUT.BIN"), data, 3000));
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("CUT.BIN"), RW));
	ASSERT_EC(0, _flex_ss_size(path, 600));
	ASSERT_EC(0, _flex_gs_size(path, &size));
	ASSERT_EQUALS(3 * 252, size);
	_flex_close(path);
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	size = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("CUT.BIN"), back, &size));
	ASSERT_EQUALS(756, size);
	ASSERT_MEM_EQUALS(data, back, 756);

	ASSERT_EC(0, _flex_open(&path, TEST_FILE("CUT.BIN"), RW));
	ASSERT_EC(0, _flex_ss_size(path, 2000));
	ASSERT_EC(0, _flex_gs_size(path, &size));
	ASSERT_EQUALS(8 * 252, size);
	ASSERT_EC(0, _flex_ss_size(path, 0));
	_flex_close(path);
	/* Closing gives back the one sector a file always has. */
	ASSERT_EQUALS(1, check_disk(TEST_IMAGE));
	remove(TEST_IMAGE);
}

static void test_readln(void)
{
	flex_path_id path;
	u_char line[300];
	u_int size;
	const char *text = "FIRST\rSECOND LINE\r";
	u_int length = (u_int)strlen(text);

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	ASSERT_EC(0, write_whole(TEST_FILE("LINES.TXT"), (const u_char *)text, length));
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("LINES.TXT"), FAM_READ));
	size = sizeof(line);
	ASSERT_EC(0, _flex_readln(path, line, &size));
	ASSERT_EQUALS(6, size);
	ASSERT_MEM_EQUALS("FIRST\r", line, 6);
	size = sizeof(line);
	ASSERT_EC(0, _flex_readln(path, line, &size));
	ASSERT_EQUALS(12, size);
	/* The rest of the sector is padding, with no carriage return. */
	size = sizeof(line);
	ASSERT_EC(0, _flex_readln(path, line, &size));
	ASSERT_EQUALS(252 - 18, size);
	ASSERT_EC(EOS_EOF, _flex_gs_eof(path));
	size = sizeof(line);
	ASSERT_EC(EOS_EOF, _flex_readln(path, line, &size));
	ASSERT_EQUALS(0, size);
	_flex_close(path);
	remove(TEST_IMAGE);
}

static void test_access_modes(void)
{
	flex_path_id path;
	u_int size = 4;

	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	ASSERT_EC(0, write_whole(TEST_FILE("M.BIN"), data, 100));
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("M.BIN"), FAM_READ));
	ASSERT_EC(EOS_BMODE, _flex_write(path, data, &size));
	_flex_close(path);
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("M.BIN"), FAM_WRITE));
	size = 4;
	ASSERT_EC(EOS_BMODE, _flex_read(path, back, &size));
	_flex_close(path);
	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	size = 4;
	ASSERT_EC(EOS_BMODE, _flex_read(path, back, &size));
	ASSERT_EC(EOS_BMODE, _flex_ss_sector(path, 1, 1, data));
	_flex_close(path);
	ASSERT_EC(EOS_BPNAM, _flex_create(&path, TEST_FILE(""), FAM_WRITE));
	ASSERT_EC(EOS_FNA, _flex_open(&path, TEST_FILE("M.BIN"), FAM_DIR | FAM_READ));
	ASSERT_EC(EOS_BPNAM, _flex_open(&path, TEST_IMAGE, FAM_READ));
	ASSERT_EC(EOS_BPNAM, _flex_open(&path, TEST_FILE("BAD NAME"), FAM_READ));
	remove(TEST_IMAGE);
}

static void test_write_protected_image(void)
{
	flex_path_id path;

	fresh_disk(TEST_DMK, DISKIMAGE_DMK, 35, 1, 0);
	ASSERT_EC(0, write_whole(TEST_DMK ",KEEP.TXT", data, 10));
	ASSERT_EC(0, _diskimage_set_write_protect(TEST_DMK, 1));
	ASSERT_EC(EOS_WP, _flex_kill(TEST_DMK ",KEEP.TXT"));
	ASSERT_EC(EOS_WP, write_whole(TEST_DMK ",NEW.TXT", data, 10));
	ASSERT_EC(0, _flex_open(&path, TEST_DMK ",KEEP.TXT", FAM_READ));
	_flex_close(path);
	ASSERT_EC(0, _flex_open(&path, TEST_DMK ",KEEP.TXT", RW | DISKIMAGE_IGNORE_WRITE_PROTECT));
	_flex_close(path);
	ASSERT_EC(0, _diskimage_set_write_protect(TEST_DMK, 0));
	ASSERT_EC(0, _flex_kill(TEST_DMK ",KEEP.TXT"));
	remove(TEST_DMK);
}

/* Put a sector in a raw image of a padded double density disk. */
static void put_sector(u_char *image, u_int track, u_int number, const u_char *contents)
{
	memcpy(&image[(track * 18 + number - 1) * 256], contents, 256);
}

/* Build a padded single sided double density disk by hand, as FLEX on
   another machine would leave it: a random file, and a free chain whose
   last sector links to itself, as a real one does. */
static long make_handmade_disk(u_char *image)
{
	u_char contents[256];
	long size = 35 * 18 * 256;
	u_int i;

	memset(image, 0, size);
	memset(contents, 0, sizeof(contents));
	memcpy(&contents[0x10], "HANDMADE", 8);
	contents[0x1D] = 3;	/* free from 3,1 */
	contents[0x1E] = 1;
	contents[0x1F] = 3;	/* to 3,3 */
	contents[0x20] = 3;
	contents[0x22] = 3;
	contents[0x23] = 6;
	contents[0x24] = 4;
	contents[0x25] = 83;
	contents[0x26] = 34;
	contents[0x27] = 18;
	put_sector(image, 0, 3, contents);

	/* The directory: a random file of 2 map sectors and 3 data sectors, and
	   a sequential file with a lower case name. */
	memset(contents, 0, sizeof(contents));
	memcpy(&contents[16], "RANDOM\0\0DAT", 11);
	contents[16 + 13] = 1;
	contents[16 + 14] = 1;
	contents[16 + 15] = 1;
	contents[16 + 16] = 5;
	contents[16 + 18] = 5;
	contents[16 + 19] = 2;
	memcpy(&contents[40], "lower\0\0\0txt", 11);
	contents[40 + 13] = 2;
	contents[40 + 14] = 1;
	contents[40 + 15] = 2;
	contents[40 + 16] = 1;
	contents[40 + 18] = 1;
	put_sector(image, 0, 5, contents);

	for (i = 1; i <= 5; i++)
	{
		memset(contents, 0, sizeof(contents));
		if (i < 5)
		{
			contents[0] = 1;
			contents[1] = (u_char)(i + 1);
		}
		contents[3] = (u_char)(i > 2 ? i - 2 : 0);
		if (i == 1)
		{
			/* The map: 3 sectors from 1,3 */
			contents[4] = 1;
			contents[5] = 3;
			contents[6] = 3;
		}
		else if (i > 2)
		{
			memset(&contents[4], 'A' + i, 252);
		}
		put_sector(image, 1, i, contents);
	}

	memset(contents, 0, sizeof(contents));
	contents[3] = 1;
	memcpy(&contents[4], "TEXT\r", 5);
	put_sector(image, 2, 1, contents);

	for (i = 1; i <= 3; i++)
	{
		memset(contents, 0, sizeof(contents));
		contents[0] = 3;
		contents[1] = (u_char)(i < 3 ? i + 1 : 3);
		put_sector(image, 3, i, contents);
	}
	return size;
}

static void test_read_handmade_disk(void)
{
	static u_char image[35 * 18 * 256];
	flex_path_id path;
	flex_file_stat stat;
	u_int size, free_sectors, i;
	long image_size = make_handmade_disk(image);
	int found;

	write_file(TEST_IMAGE, image, image_size);
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(1, found);

	/* The map sectors aren't part of a random file's data. */
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("RANDOM.DAT"), FAM_READ));
	ASSERT_EC(0, _flex_gs_fd(path, &stat));
	ASSERT_EQUALS(1, stat.random);
	ASSERT_EQUALS(5, stat.sectors);
	ASSERT_EQUALS(3 * 252, stat.file_size);
	size = sizeof(back);
	ASSERT_EC(0, _flex_read(path, back, &size));
	ASSERT_EQUALS(756, size);
	for (i = 0; i < 3; i++)
	{
		ASSERT_EQUALS('A' + 3 + i, back[i * 252]);
		ASSERT_EQUALS('A' + 3 + i, back[i * 252 + 251]);
	}
	_flex_close(path);

	/* A random file can be changed, but can't grow. */
	ASSERT_EC(0, _flex_open(&path, TEST_FILE("RANDOM.DAT"), RW));
	size = 4;
	ASSERT_EC(0, _flex_write(path, "WXYZ", &size));
	ASSERT_EC(0, _flex_seek(path, 0, SEEK_END));
	size = 1;
	ASSERT_EC(EOS_FNA, _flex_write(path, "!", &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(EOS_FNA, _flex_ss_size(path, 0));
	_flex_close(path);
	ASSERT_EQUALS(2, check_chains(TEST_IMAGE));

	/* Names match whatever their case. */
	size = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("LOWER.TXT"), back, &size));
	ASSERT_MEM_EQUALS("TEXT\r", back, 5);

	/* The free chain ends at the SIR's last sector, not at its link. */
	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	ASSERT_EC(0, _flex_count_free_chain(path, &free_sectors));
	ASSERT_EQUALS(3, free_sectors);
	_flex_close(path);
	fill_pattern(756, 10);
	ASSERT_EC(0, write_whole(TEST_FILE("FILL.BIN"), data, 756));
	ASSERT_EC(EOS_DF, write_whole(TEST_FILE("OVER.BIN"), data, 1));
	ASSERT_EC(0, _flex_kill(TEST_FILE("OVER.BIN")));
	ASSERT_EC(0, _flex_kill(TEST_FILE("RANDOM.DAT")));
	ASSERT_EQUALS(2, check_chains(TEST_IMAGE));
	ASSERT("the last file reads back", file_matches(TEST_FILE("FILL.BIN"), 756, 10));
	remove(TEST_IMAGE);
}

/* Raw images have track 0 short or padded, and some have tracks past the
   last one FLEX uses. */
static void test_raw_track0(void)
{
	static u_char padded[35 * 18 * 256];
	static u_char image[(10 + 35 * 18) * 256];
	long size = make_handmade_disk(padded);
	u_int got;

	/* Short track 0, and a spare track at the end */
	memset(image, 0xE5, sizeof(image));
	memcpy(image, padded, 10 * 256);
	memcpy(&image[10 * 256], &padded[18 * 256], size - 18 * 256);
	write_file(TEST_IMAGE, image, sizeof(image));
	got = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("LOWER.TXT"), back, &got));
	ASSERT_MEM_EQUALS("TEXT\r", back, 5);
	ASSERT_EQUALS(2, check_chains(TEST_IMAGE));

	/* Exactly as many tracks as the SIR says */
	write_file(TEST_IMAGE, image, (10 + 34 * 18) * 256);
	got = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("RANDOM.DAT"), back, &got));
	ASSERT_EQUALS(756, got);
	ASSERT_EQUALS('D', back[0]);
	remove(TEST_IMAGE);
}

static void test_damaged_chains(void)
{
	static u_char image[35 * 18 * 256];
	flex_path_id path;
	u_int size;
	long image_size = make_handmade_disk(image);

	/* A file's chain that loops is only followed for the file's sectors,
	   so it can't go on forever. */
	image[(18 + 2) * 256] = 1;
	image[(18 + 2) * 256 + 1] = 3;
	write_file(TEST_IMAGE, image, image_size);
	size = sizeof(back);
	ASSERT_EC(0, read_whole(TEST_FILE("RANDOM.DAT"), back, &size));
	ASSERT_EQUALS(756, size);
	ASSERT_EQUALS('D', back[252]);

	/* One that leads off the disk */
	image[(18 + 2) * 256] = 200;
	write_file(TEST_IMAGE, image, image_size);
	size = sizeof(back);
	ASSERT_EC(EOS_SECT, read_whole(TEST_FILE("RANDOM.DAT"), back, &size));

	/* A directory that loops ends with an error, not forever. */
	make_handmade_disk(image);
	image[4 * 256 + 0] = 0;
	image[4 * 256 + 1] = 5;
	write_file(TEST_IMAGE, image, image_size);
	ASSERT_EC(0, _flex_open(&path, TEST_FILE(""), FAM_READ));
	{
		flex_dir_entry entry;
		error_code ec;
		int count = 0;

		while ((ec = _flex_readdir(path, &entry)) == 0 && count < 100000)
		{
			count++;
		}
		ASSERT_EQUALS(EOS_SECT, ec);
	}
	_flex_close(path);
	ASSERT_EC(EOS_SECT, _flex_open(&path, TEST_FILE("MISSING.TXT"), FAM_READ));
	remove(TEST_IMAGE);
}

static void test_missing_sectors(void)
{
	flex_path_id path;
	u_int size;

	/* A DMK image missing the sector a read needs */
	fresh_disk(TEST_DMK, DISKIMAGE_DMK, 40, 1, 1);
	fill_pattern(600, 11);
	ASSERT_EC(0, write_whole(TEST_DMK ",PART.BIN", data, 600));
	image_remove_sector(TEST_DMK, 1, 0, 2);
	ASSERT_EC(0, _flex_open(&path, TEST_DMK ",PART.BIN", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(EOS_SECT, _flex_read(path, back, &size));
	ASSERT_EQUALS(252, size);
	_flex_close(path);
	remove(TEST_DMK);
}

/* Other filesystems aren't taken for FLEX, nor FLEX for them. */
static void test_identify(void)
{
	_path_type type;
	int found;
	u_int filesystems;

	remove(TEST_IMAGE);
	ASSERT_EC(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(0, found);
	ASSERT_EC(0, _coco_identify_image(TEST_FILE("X"), &type));
	ASSERT_EQUALS(DECB, type);
	remove(TEST_IMAGE);

	ASSERT_EC(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 1, 0x00));
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(0, found);
	remove(TEST_IMAGE);

	ASSERT_EC(0, _dragondos_dskinit(TEST_IMAGE, DISKIMAGE_RAW, 40, 1));
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(0, found);
	ASSERT_EC(0, _coco_identify_image(TEST_FILE("X"), &type));
	ASSERT_EQUALS(DRAGONDOS, type);
	remove(TEST_IMAGE);

	write_file(TEST_IMAGE, (const u_char *)"short", 5);
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(0, found);
	remove(TEST_IMAGE);
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(0, found);

	fresh_disk(TEST_DMK, DISKIMAGE_DMK, 40, 2, 1);
	ASSERT_EC(0, _flex_identify(TEST_DMK, &found));
	ASSERT_EQUALS(1, found);
	ASSERT_EC(0, _coco_identify_image(TEST_DMK ",X", &type));
	ASSERT_EQUALS(FLEX, type);
	ASSERT_EC(0, _coco_detect_filesystems(TEST_DMK, &filesystems));
	ASSERT("FLEX is found", (filesystems & COCO_FS_FLEX) != 0);
	remove(TEST_DMK);

	/* A SIR with a free chain off the disk isn't believable. */
	fresh_disk(TEST_IMAGE, DISKIMAGE_RAW, 35, 1, 0);
	{
		FILE *fp = fopen(TEST_IMAGE, "r+b");

		fseek(fp, 2 * 256 + 0x1D, SEEK_SET);
		fputc(99, fp);
		fclose(fp);
	}
	ASSERT_EC(0, _flex_identify(TEST_IMAGE, &found));
	ASSERT_EQUALS(0, found);
	remove(TEST_IMAGE);
}

static void test_filesystem_names(void)
{
	char names[64];

	_coco_filesystem_names(COCO_FS_FLEX, names, sizeof(names));
	ASSERT_STRING_EQUALS("FLEX", names);
	_coco_filesystem_names(COCO_FS_OS9 | COCO_FS_DECB | COCO_FS_DRAGONDOS | COCO_FS_FLEX, names, sizeof(names));
	ASSERT_STRING_EQUALS("OS-9, Disk BASIC, Dragon DOS, FLEX", names);
	_coco_filesystem_names(COCO_FS_DECB | COCO_FS_FLEX, names, sizeof(names));
	ASSERT_STRING_EQUALS("Disk BASIC, FLEX", names);
	_coco_filesystem_names(0, names, sizeof(names));
	ASSERT_STRING_EQUALS("", names);
	_coco_filesystem_names(COCO_FS_OS9 | COCO_FS_DECB, names, 8);
	ASSERT_STRING_EQUALS("OS-9", names);
}

static void test_text_conversion(void)
{
	const u_char flex_text[] = { 'A', '\t', 3, 'B', '\r', '\n', 'C', '\r', 0, 0, 0 };
	const char *native = "\tX\r\nY\nZ";
	u_char *out;
	u_int size;

	ASSERT_EC(0, _flex_text_to_native(flex_text, sizeof(flex_text), &out, &size));
	ASSERT_EQUALS(8, size);
	ASSERT_MEM_EQUALS("A   B\nC\n", out, 8);
	free(out);

	ASSERT_EC(0, _flex_text_from_native((const u_char *)native, (u_int)strlen(native), &out, &size));
	ASSERT_EQUALS(7, size);
	ASSERT_MEM_EQUALS("\t\010X\rY\rZ", out, 7);
	free(out);

	ASSERT_EC(0, _flex_text_to_native((const u_char *)"", 0, &out, &size));
	ASSERT_EQUALS(0, size);
	free(out);
}

static void test_binary_info(void)
{
	const u_char binary[] = {
		0x02, 0xC1, 0x00, 0x03, 1, 2, 3,
		0x02, 0x10, 0x00, 0x01, 9,
		0x16, 0xC1, 0x00,
		0, 0, 0
	};
	u_int low, high, transfer, records;
	int has_transfer;

	ASSERT_EC(0, _flex_binary_info(binary, sizeof(binary), &low, &high, &transfer, &has_transfer, &records));
	ASSERT_EQUALS(0x1000, low);
	ASSERT_EQUALS(0xC102, high);
	ASSERT_EQUALS(0xC100, transfer);
	ASSERT_EQUALS(1, has_transfer);
	ASSERT_EQUALS(3, records);

	ASSERT_EC(0, _flex_binary_info(binary, 12, &low, &high, &transfer, &has_transfer, &records));
	ASSERT_EQUALS(0, has_transfer);
	ASSERT_EC(EOS_WT, _flex_binary_info(binary, 5, &low, &high, &transfer, &has_transfer, &records));
	ASSERT_EC(EOS_WT, _flex_binary_info((const u_char *)"text\r", 5, &low, &high, &transfer, &has_transfer,
					    &records));
	ASSERT_EC(EOS_WT, _flex_binary_info(&binary[15], 3, &low, &high, &transfer, &has_transfer, &records));
}

static void test_coco_file_operations(void)
{
	static char buffer[512];
	coco_path_id path;
	coco_dir_entry entry;
	coco_file_stat stat;
	u_char name[16];
	const char *native = "flex_native.bin";
	const char *copied = "flex_copied.bin";
	int attributes, count = 0;
	long size;

	fresh_disk(TEST_DMK, DISKIMAGE_DMK, 40, 1, 1);
	fill_pattern(5000, 12);
	write_file(native, data, 5000);
	ASSERT_EC(0, TSCopyFile((char *)native, TEST_DMK ",COPY.BIN", 0, 1, 0, 0, buffer, sizeof(buffer)));
	ASSERT_EC(0, TSCopyFile(TEST_DMK ",COPY.BIN", (char *)copied, 0, 1, 0, 0, buffer, sizeof(buffer)));
	size = read_file(copied, back, sizeof(back));
	ASSERT_EQUALS(20 * 252, size);
	ASSERT_MEM_EQUALS(data, back, 5000);

	ASSERT_EC(0, _coco_open(&path, TEST_DMK ",", FAM_READ));
	while (_coco_readdir(path, &entry) == 0)
	{
		ASSERT_EQUALS(FLEX, entry.type);
		ASSERT_EC(0, _coco_ncpy_name(&entry, name, sizeof(name)));
		ASSERT_STRING_EQUALS("COPY.BIN", (char *)name);
		count++;
	}
	ASSERT_EQUALS(1, count);
	ASSERT_EC(0, _coco_gs_attr(path, &attributes));
	ASSERT("the disk is a directory", (attributes & FAP_DIR) != 0);
	_coco_close(path);

	ASSERT_EC(0, _coco_open(&path, TEST_DMK ",COPY.BIN", RW));
	ASSERT_EC(0, _coco_ss_attr(path, FAP_READ));
	ASSERT_EC(0, _coco_gs_attr(path, &attributes));
	ASSERT_EQUALS(0, attributes & FAP_WRITE);
	ASSERT_EC(0, _coco_gs_fd(path, &stat));
	_coco_close(path);
	ASSERT_EC(EOS_FNA, _coco_delete(TEST_DMK ",COPY.BIN"));
	ASSERT_EC(0, _coco_open(&path, TEST_DMK ",COPY.BIN", RW));
	ASSERT_EC(0, _coco_ss_attr(path, FAP_READ | FAP_WRITE));
	_coco_close(path);

	ASSERT_EC(0, _coco_rename(TEST_DMK ",COPY.BIN", "MOVED"));
	ASSERT_EC(0, _coco_delete(TEST_DMK ",MOVED.BIN"));
	ASSERT_EC(EOS_FNA, _coco_makdir(TEST_DMK ",DIR"));
	ASSERT_EQUALS(0, check_disk(TEST_DMK));

	remove(native);
	remove(copied);
	remove(TEST_DMK);
}

int main(void)
{
	RUN(test_parse_names);
	RUN(test_dskinit_layout);
	RUN(test_dskinit_sizes);
	RUN(test_dskinit_records_flex_layout);
	RUN(test_diskimage_create_flags);
	RUN(test_all_formats);
	RUN(test_write_layout);
	RUN(test_empty_file);
	RUN(test_kill);
	RUN(test_directory_grows);
	RUN(test_disk_full);
	RUN(test_create_existing);
	RUN(test_protection);
	RUN(test_rename);
	RUN(test_seek_and_overwrite);
	RUN(test_write_past_end);
	RUN(test_set_size);
	RUN(test_readln);
	RUN(test_access_modes);
	RUN(test_write_protected_image);
	RUN(test_read_handmade_disk);
	RUN(test_raw_track0);
	RUN(test_damaged_chains);
	RUN(test_missing_sectors);
	RUN(test_identify);
	RUN(test_filesystem_names);
	RUN(test_text_conversion);
	RUN(test_binary_info);
	RUN(test_coco_file_operations);
	return TEST_REPORT();
}

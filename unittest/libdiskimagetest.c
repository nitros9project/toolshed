/* Tests for the common disk image library. */

#include <string.h>

#include "tinytest.h"
#include "cocopath.h"
#include "diskimage.h"

#define TEST_IMAGE "diskimage_test.dsk"
#define RW (FAM_READ | FAM_WRITE)

#define DMK_TRACK_LENGTH 0x1900
#define DMK_FLAG_SINGLE_SIDED 0x10
#define DMK_FLAG_SD_ONE_BYTE 0x40
#define SDF_RECORD 6656

/* ASSERT_EQUALS evaluates its second argument twice, so calls with side
   effects keep their result here first. */
static error_code call_result;
#define ASSERT_CALL(expected, call) \
	{ \
		call_result = (call); \
		ASSERT_EQUALS(expected, call_result); \
	}

static u_char image[1024 * 1024];

/* Sector descriptions used to build DMK and SDF tracks. */
typedef struct
{
	int cylinder;
	int side;
	int id;
	int size_code;
	int single_density;
	u_char fill;
	u_char mark;		/* 0 means $FB */
	int id_track;		/* -1 means the physical cylinder */
	int bad_id_crc;
	int bad_data_crc;
	int no_data;
} test_sector;

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

static void write_image(long size)
{
	FILE *fp = fopen(TEST_IMAGE, "wb");

	fwrite(image, 1, size, fp);
	fclose(fp);
}

static long read_image(void)
{
	FILE *fp = fopen(TEST_IMAGE, "rb");
	long size = (long)fread(image, 1, sizeof(image), fp);

	fclose(fp);
	return size;
}

/* How many bytes of the image in a range are not the value. */
static long count_other(long start, long end, u_char value)
{
	long count = 0;
	long i;

	for (i = start; i < end; i++)
	{
		count += image[i] != value;
	}

	return count;
}

static void put(u_char *track, int *pos, u_char byte, int step, int count)
{
	int i;

	for (i = 0; i < count * step; i++)
	{
		track[(*pos)++] = byte;
	}
}

/* Record a sector on a track as a controller would, returning the positions
   of its address marks. */
static void put_sector(u_char *track, int *pos, const test_sector *s, int step, int *idam, int *dam)
{
	u_char field[1 + 1024];
	u_char id[5];
	unsigned short crc;
	int dd = !s->single_density;
	int size = 128 << s->size_code;
	unsigned short init = dd ? 0xCDB4 : 0xFFFF;
	int i;

	if (dd)
	{
		put(track, pos, 0xA1, 1, 3);
	}
	id[0] = 0xFE;
	id[1] = (u_char)(s->id_track >= 0 ? s->id_track : s->cylinder);
	id[2] = (u_char)s->side;
	id[3] = (u_char)s->id;
	id[4] = (u_char)s->size_code;
	*idam = *pos;
	for (i = 0; i < 5; i++)
	{
		put(track, pos, id[i], step, 1);
	}
	crc = crc16(init, id, 5);
	if (s->bad_id_crc)
	{
		crc ^= 0xFFFF;
	}
	put(track, pos, (u_char)(crc >> 8), step, 1);
	put(track, pos, (u_char)crc, step, 1);

	*dam = 0;
	if (s->no_data)
	{
		return;
	}

	if (dd)
	{
		put(track, pos, 0x4E, 1, 22);
		put(track, pos, 0x00, 1, 12);
		put(track, pos, 0xA1, 1, 3);
	}
	else
	{
		put(track, pos, 0xFF, step, 11);
		put(track, pos, 0x00, step, 6);
	}

	field[0] = s->mark ? s->mark : 0xFB;
	memset(&field[1], s->fill, size);
	*dam = *pos;
	put(track, pos, field[0], step, 1);
	for (i = 0; i < size; i++)
	{
		put(track, pos, s->fill, step, 1);
	}
	crc = crc16(init, field, 1 + size);
	if (s->bad_data_crc)
	{
		crc ^= 0xFFFF;
	}
	put(track, pos, (u_char)(crc >> 8), step, 1);
	put(track, pos, (u_char)crc, step, 1);
	put(track, pos, dd ? 0x4E : 0xFF, step, 24);
}

static void build_dmk(int tracks, int sides, u_char flags, u_char protect, const test_sector *sectors, int count)
{
	int cylinder, side, i;
	long size = 16 + (long)tracks * sides * DMK_TRACK_LENGTH;

	memset(image, 0, size);
	image[0] = protect;
	image[1] = (u_char)tracks;
	image[2] = DMK_TRACK_LENGTH & 0xFF;
	image[3] = DMK_TRACK_LENGTH >> 8;
	image[4] = flags;
	for (cylinder = 0; cylinder < tracks; cylinder++)
	{
		for (side = 0; side < sides; side++)
		{
			u_char *track = &image[16 + (long)(cylinder * sides + side) * DMK_TRACK_LENGTH];
			int pos = 128;
			int slot = 0;

			for (i = 0; i < count; i++)
			{
				const test_sector *s = &sectors[i];

				if (s->cylinder == cylinder && s->side == side)
				{
					int step = (s->single_density && !(flags & DMK_FLAG_SD_ONE_BYTE)) ? 2 : 1;
					int idam, dam;
					unsigned int pointer;

					put_sector(track, &pos, s, step, &idam, &dam);
					pointer = idam | (s->single_density ? 0 : 0x8000);
					track[slot * 2] = pointer & 0xFF;
					track[slot * 2 + 1] = (u_char)(pointer >> 8);
					slot++;
				}
			}
		}
	}
	write_image(size);
}

/* dmk2sdf never sets the single density bit, and its ID offset is one past
   the start of the mark even in single density. */
static int sdf_dmk2sdf_offsets = 0;

static void build_sdf(int tracks, int sides, u_char protect, const test_sector *sectors, int count)
{
	int cylinder, side, i;
	long size = 512 + (long)tracks * sides * SDF_RECORD;

	memset(image, 0, size);
	memcpy(image, "SDF1", 4);
	image[4] = (u_char)tracks;
	image[5] = (u_char)sides;
	image[6] = protect;
	for (cylinder = 0; cylinder < tracks; cylinder++)
	{
		for (side = 0; side < sides; side++)
		{
			u_char *record = &image[512 + (long)(cylinder * sides + side) * SDF_RECORD];
			int pos = 256;
			int slot = 0;

			for (i = 0; i < count; i++)
			{
				const test_sector *s = &sectors[i];

				if (s->cylinder == cylinder && s->side == side)
				{
					int step = s->single_density ? 2 : 1;
					u_char *entry = &record[8 + slot * 8];
					u_char mark = s->mark ? s->mark : 0xFB;
					int idam, dam;
					unsigned int id_word, data_word;

					put_sector(record, &pos, s, step, &idam, &dam);

					/* The offsets point after each address mark. */
					id_word = sdf_dmk2sdf_offsets ? (idam + 1) : ((idam + step) | (s->single_density ? 0x4000 : 0));
					id_word |= s->bad_id_crc ? 0x8000 : 0;
					data_word = dam ? (dam + step) : 0;
					data_word |= (mark == 0xF8 ? 0x4000 : 0) | (s->bad_data_crc ? 0x8000 : 0);
					entry[0] = id_word & 0xFF;
					entry[1] = (u_char)(id_word >> 8);
					entry[2] = data_word & 0xFF;
					entry[3] = (u_char)(data_word >> 8);
					entry[4] = (u_char)(s->id_track >= 0 ? s->id_track : s->cylinder);
					entry[5] = (u_char)s->side;
					entry[6] = (u_char)s->id;
					entry[7] = (u_char)s->size_code;
					slot++;
				}
			}
			record[0] = (u_char)slot;
		}
	}
	write_image(size);
}

static void fill_pattern(u_char *buffer, int size, u_char seed)
{
	int i;

	for (i = 0; i < size; i++)
	{
		buffer[i] = (u_char)(seed + i);
	}
}

/* Build a raw or headered image whose sectors each hold their index + 1. */
static long build_linear(const u_char *header, int header_size, int sectors, int sector_size)
{
	int i;

	memcpy(image, header, header_size);
	for (i = 0; i < sectors; i++)
	{
		memset(&image[header_size + (long)i * sector_size], i + 1, sector_size);
	}
	return header_size + (long)sectors * sector_size;
}

static diskimage_format identify(void)
{
	diskimage_format format = DISKIMAGE_UNKNOWN;

	_diskimage_identify(TEST_IMAGE, &format);
	return format;
}

static const test_sector dd_cylinder_sectors[] = {
	/* Recorded out of order: logical sectors must still follow sector number. */
	{0, 0, 2, 1, 0, 0x22, 0, -1},
	{0, 0, 1, 1, 0, 0x11, 0, -1},
	{1, 0, 1, 1, 0, 0x33, 0, -1},
	{1, 0, 2, 1, 0, 0x44, 0, -1},
};

/* Identify */

static void test_identify_missing_file(void)
{
	diskimage_format format;

	remove(TEST_IMAGE);
	ASSERT_CALL(EOS_PNNF, _diskimage_identify(TEST_IMAGE, &format));
}

static void test_identify_directory(void)
{
	diskimage_format format;
	diskimage_id disk;

	ASSERT_CALL(EOS_FNA, _diskimage_identify(".", &format));
	ASSERT_CALL(EOS_FNA, _diskimage_open(&disk, ".", FAM_READ));
}

static void test_identify_formats(void)
{
	static const u_char jvc_header[] = {18, 1};
	static const test_sector one[] = {{0, 0, 1, 1, 0, 0x11, 0, -1}};

	write_image(0);
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());

	write_image(build_linear(NULL, 0, 36, 256));
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());

	write_image(build_linear(jvc_header, 2, 18, 256));
	ASSERT_EQUALS(DISKIMAGE_JVC, identify());

	build_dmk(1, 1, DMK_FLAG_SINGLE_SIDED, 0, one, 1);
	ASSERT_EQUALS(DISKIMAGE_DMK, identify());

	build_sdf(1, 1, 0, one, 1);
	ASSERT_EQUALS(DISKIMAGE_SDF, identify());
	remove(TEST_IMAGE);
}

static void test_identify_rejects(void)
{
	static const test_sector one[] = {{0, 0, 1, 1, 0, 0x11, 0, -1}};
	diskimage_id disk;

	/* JVC header with an invalid sectors per track. */
	memset(image, 0, 256 * 18 + 3);
	write_image(256 * 18 + 3);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());
	ASSERT_CALL(EOS_WT, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));

	/* JVC header with no room for a whole track. */
	image[0] = 18;
	write_image(3);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	/* A DMK specification file for a real floppy: fails DMK, and its header
	   length then gives a JVC header with no sectors per track. */
	build_dmk(1, 1, DMK_FLAG_SINGLE_SIDED, 0, one, 1);
	image[12] = 0x78;
	write_image(16 + DMK_TRACK_LENGTH);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	/* A later SDF version is treated as plain data. */
	build_sdf(1, 1, 0, one, 1);
	image[3] = '2';
	write_image(512 + SDF_RECORD);
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());
	remove(TEST_IMAGE);
}

static void test_open_bad_mode(void)
{
	diskimage_id disk;

	write_image(build_linear(NULL, 0, 18, 256));
	ASSERT_CALL(EOS_BMODE, _diskimage_open(&disk, TEST_IMAGE, 0));
	remove(TEST_IMAGE);
}

/* Raw */

static void test_raw_geometry(void)
{
	diskimage_id disk;
	diskimage_geometry g;

	write_image(build_linear(NULL, 0, 36, 256));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	_diskimage_close(disk);
	remove(TEST_IMAGE);

	ASSERT_EQUALS(DISKIMAGE_RAW, g.format);
	ASSERT_EQUALS(2, g.cylinders);
	ASSERT_EQUALS(1, g.sides);
	ASSERT_EQUALS(18, g.sectors_per_track);
	ASSERT_EQUALS(256, g.sector_size);
	ASSERT_EQUALS(1, g.first_sector);
	ASSERT_EQUALS(36, g.total_sectors);
	ASSERT_EQUALS(1, g.uniform);
	ASSERT_EQUALS(0, g.write_protected);
	ASSERT_EQUALS(1, g.extendable);
}

static void test_raw_read_write_sector(void)
{
	diskimage_id disk;
	u_char buffer[256], out[256];
	u_int size = sizeof(buffer);

	write_image(build_linear(NULL, 0, 36, 256));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));

	/* Track 1 sector 3 is logical sector 20, which holds 21. */
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 3, buffer, &size));
	ASSERT_EQUALS(256, size);
	ASSERT_EQUALS(21, buffer[0]);
	ASSERT_EQUALS(21, buffer[255]);

	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 2, 0, 1, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 1, 1, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 0, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 19, buffer, &size));

	size = 255;
	ASSERT_CALL(EOS_IA, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(256, size);

	fill_pattern(out, 256, 7);
	ASSERT_CALL(EOS_IA, _diskimage_write_sector(disk, 0, 0, 2, out, 128));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 2, out, 256));
	_diskimage_close(disk);

	ASSERT_EQUALS(36 * 256, read_image());
	ASSERT_MEM_EQUALS(out, &image[256], 256);
	ASSERT_EQUALS(1, image[255]);
	ASSERT_EQUALS(3, image[512]);
	remove(TEST_IMAGE);
}

static void test_raw_extend_by_sector(void)
{
	diskimage_id disk;
	diskimage_geometry g;
	u_char out[256];
	u_int size = sizeof(out);

	write_image(build_linear(NULL, 0, 2, 256));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	fill_pattern(out, 256, 1);

	/* Track 1 sector 2 is logical sector 19: 2..18 become zero. */
	ASSERT_CALL(0, _diskimage_write_sector(disk, 1, 0, 2, out, 256));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 2, out, &size));
	_diskimage_close(disk);

	ASSERT_EQUALS(20, g.total_sectors);
	ASSERT_EQUALS(2, g.cylinders);
	ASSERT_EQUALS(0, g.uniform);
	ASSERT_EQUALS(20 * 256, read_image());
	ASSERT_EQUALS(0, count_other(0, 256, 1));
	ASSERT_EQUALS(0, count_other(256, 2 * 256, 2));
	ASSERT_EQUALS(0, count_other(2 * 256, 19 * 256, 0x00));
	ASSERT_EQUALS(1, image[19 * 256]);
	remove(TEST_IMAGE);
}

/* Only a gap is zeroed: an image with the sector already has nothing to fill. */
static void test_raw_write_without_gap_adds_no_zeros(void)
{
	diskimage_id disk;
	u_char out[256];

	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 2, 256, 18, 0, 0, 0xFF));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	memset(out, 0x5C, sizeof(out));

	/* One inside the image, and one just past its end. */
	ASSERT_CALL(0, _diskimage_write_sector(disk, 1, 0, 2, out, 256));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 2, 0, 1, out, 256));
	_diskimage_close(disk);

	ASSERT_EQUALS(37 * 256, read_image());
	ASSERT_EQUALS(0, count_other(0, 19 * 256, 0xFF));
	ASSERT_EQUALS(0, count_other(19 * 256, 20 * 256, 0x5C));
	ASSERT_EQUALS(0, count_other(20 * 256, 36 * 256, 0xFF));
	ASSERT_EQUALS(0, count_other(36 * 256, 37 * 256, 0x5C));
	remove(TEST_IMAGE);
}

/* JVC */

static void test_jvc_header_geometry(void)
{
	static const u_char header[] = {9, 2, 2, 5, 0};
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[512];
	u_int size = sizeof(buffer);

	/* 3 cylinders, 2 sides, 9 sectors of 512 bytes numbered from 5. */
	write_image(build_linear(header, 5, 54, 512));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(DISKIMAGE_JVC, g.format);
	ASSERT_EQUALS(3, g.cylinders);
	ASSERT_EQUALS(2, g.sides);
	ASSERT_EQUALS(9, g.sectors_per_track);
	ASSERT_EQUALS(512, g.sector_size);
	ASSERT_EQUALS(5, g.first_sector);
	ASSERT_EQUALS(54, g.total_sectors);
	ASSERT_EQUALS(0, g.extendable);

	/* Cylinder 1 side 1 is the fourth track in the file. */
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 1, 6, buffer, &size));
	ASSERT_EQUALS(29, buffer[0]);
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 4, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 14, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 3, 0, 5, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_write_sector(disk, 3, 0, 5, buffer, 512));

	ASSERT_CALL(0, _diskimage_query_sector(disk, 28, &info));
	ASSERT_EQUALS(1, info.cylinder);
	ASSERT_EQUALS(1, info.side);
	ASSERT_EQUALS(6, info.sector);
	ASSERT_CALL(EOS_SECT, _diskimage_query_sector(disk, 54, &info));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_jvc_short_header_uses_defaults(void)
{
	static const u_char header[] = {10};
	diskimage_id disk;
	diskimage_geometry g;

	write_image(build_linear(header, 1, 20, 256));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	_diskimage_close(disk);
	remove(TEST_IMAGE);

	ASSERT_EQUALS(2, g.cylinders);
	ASSERT_EQUALS(1, g.sides);
	ASSERT_EQUALS(10, g.sectors_per_track);
	ASSERT_EQUALS(256, g.sector_size);
	ASSERT_EQUALS(1, g.first_sector);
}

static void test_jvc_write_and_no_extend(void)
{
	static const u_char header[] = {18, 1};
	diskimage_id disk;
	u_char out[256];

	write_image(build_linear(header, 2, 18, 256));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	memset(out, 0x77, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 2, out, 256));

	/* Past the last track is not a sector, and the image doesn't grow. */
	ASSERT_CALL(EOS_SECT, _diskimage_write_sector(disk, 1, 0, 2, out, 256));
	_diskimage_close(disk);

	ASSERT_EQUALS(2 + 18 * 256, read_image());
	ASSERT_EQUALS(1, image[2 + 255]);
	ASSERT_EQUALS(0x77, image[2 + 256]);
	ASSERT_EQUALS(3, image[2 + 512]);
	remove(TEST_IMAGE);
}

static void test_jvc_sector_attributes(void)
{
	/* 16 cylinders of 16 sectors: only a multiple of 256 sectors lets the
	   header length formula work when attributes are present. */
	static const u_char header[] = {16, 1, 1, 1, 1};
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[256], out[256];
	u_int size = sizeof(buffer);
	long length = 5 + 256L * 257;
	int i;

	memcpy(image, header, 5);
	for (i = 0; i < 256; i++)
	{
		u_char *record = &image[5 + i * 257L];

		record[0] = 0;
		memset(&record[1], i, 256);
	}
	image[5 + 1 * 257L] = 0x10;	/* sector 2: not found */
	image[5 + 2 * 257L] = 0x08;	/* sector 3: CRC error */
	image[5 + 3 * 257L] = 0x20;	/* sector 4: deleted */
	write_image(length);

	ASSERT_EQUALS(DISKIMAGE_JVC, identify());
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(16, g.cylinders);
	ASSERT_EQUALS(255, g.total_sectors);
	ASSERT_EQUALS(0, g.uniform);

	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_CALL(EOS_CRC, _diskimage_read_sector(disk, 0, 0, 3, buffer, &size));
	ASSERT_EQUALS(2, buffer[0]);
	ASSERT_CALL(0, _diskimage_query_sector(disk, 2, &info));
	ASSERT_EQUALS(4, info.sector);
	ASSERT_EQUALS(DISKIMAGE_SECTOR_DELETED, info.flags);

	/* Writing a sector clears its CRC error and keeps a deleted mark. */
	memset(out, 0x5A, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 3, out, 256));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 4, out, 256));
	_diskimage_close(disk);

	read_image();
	ASSERT_EQUALS(0x00, image[5 + 2 * 257L]);
	ASSERT_EQUALS(0x5A, image[5 + 2 * 257L + 1]);
	ASSERT_EQUALS(0x20, image[5 + 3 * 257L]);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 3, buffer, &size));
	ASSERT_EQUALS(0x5A, buffer[0]);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

/* DMK and SDF share most behavior, so these tests run against both. */

typedef void (*image_builder)(u_char protect, const test_sector *sectors, int count);

static void build_dmk_2t(u_char protect, const test_sector *sectors, int count)
{
	build_dmk(2, 1, DMK_FLAG_SINGLE_SIDED, protect, sectors, count);
}

static void build_sdf_2t(u_char protect, const test_sector *sectors, int count)
{
	build_sdf(2, 1, protect, sectors, count);
}

static void check_track_geometry_and_order(image_builder build, diskimage_format format)
{
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[1024];
	u_int size = sizeof(buffer);
	static const u_char expected[4] = {0x11, 0x22, 0x33, 0x44};
	int i;

	build(0, dd_cylinder_sectors, 4);
	ASSERT_EQUALS(format, identify());
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(format, g.format);
	ASSERT_EQUALS(2, g.cylinders);
	ASSERT_EQUALS(1, g.sides);
	ASSERT_EQUALS(2, g.sectors_per_track);
	ASSERT_EQUALS(256, g.sector_size);
	ASSERT_EQUALS(1, g.first_sector);
	ASSERT_EQUALS(4, g.total_sectors);
	ASSERT_EQUALS(1, g.uniform);
	ASSERT_EQUALS(0, g.extendable);

	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_EQUALS(256, size);
	ASSERT_EQUALS(0x22, buffer[0]);
	ASSERT_EQUALS(0x22, buffer[255]);
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 3, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 2, 0, 1, buffer, &size));

	ASSERT_CALL(0, _diskimage_query_sector(disk, 1, &info));
	ASSERT_EQUALS(2, info.sector);

	/* Logical sectors follow sector numbers, not recording order. */
	for (i = 0; i < 4; i++)
	{
		ASSERT_CALL(0, _diskimage_query_sector(disk, i, &info));
		size = sizeof(buffer);
		ASSERT_CALL(0, _diskimage_read_sector(disk, info.cylinder, info.side, info.sector, buffer, &size));
		ASSERT_EQUALS(expected[i], buffer[0]);
	}
	ASSERT_CALL(EOS_SECT, _diskimage_query_sector(disk, 4, &info));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_geometry_and_order(void)
{
	check_track_geometry_and_order(build_dmk_2t, DISKIMAGE_DMK);
}

static void test_sdf_geometry_and_order(void)
{
	check_track_geometry_and_order(build_sdf_2t, DISKIMAGE_SDF);
}

static void check_mixed_sizes(image_builder build)
{
	static const test_sector sectors[] = {
		{0, 0, 1, 0, 0, 0x11, 0, -1},	/* 128 bytes */
		{0, 0, 2, 2, 0, 0x22, 0, -1},	/* 512 bytes */
		{1, 0, 1, 1, 1, 0x33, 0, -1},	/* 256 bytes, single density */
	};
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[512], out[512];
	u_int size = sizeof(buffer);

	build(0, sectors, 3);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(512, g.sector_size);
	ASSERT_EQUALS(0, g.uniform);

	/* Each is read at its own size. */
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(128, size);
	ASSERT_EQUALS(0x11, buffer[127]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_EQUALS(512, size);
	ASSERT_EQUALS(0x22, buffer[511]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 1, buffer, &size));
	ASSERT_EQUALS(256, size);
	ASSERT_EQUALS(0x33, buffer[255]);

	/* A write must be the size of the sector. */
	memset(out, 0x99, sizeof(out));
	ASSERT_CALL(EOS_IA, _diskimage_write_sector(disk, 0, 0, 2, out, 256));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 2, out, 512));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 1, 0, 1, out, 256));
	_diskimage_close(disk);

	/* Reopening proves the CRCs written were right, and the neighbor is untouched. */
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(0x11, buffer[127]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_MEM_EQUALS(out, buffer, 512);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 1, buffer, &size));
	ASSERT_MEM_EQUALS(out, buffer, 256);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_mixed_sizes(void)
{
	check_mixed_sizes(build_dmk_2t);
}

static void test_sdf_mixed_sizes(void)
{
	check_mixed_sizes(build_sdf_2t);
}

static void check_unavailable_sectors_excluded(image_builder build)
{
	static const test_sector sectors[] = {
		{0, 0, 1, 1, 0, 0x11, 0, -1},
		{0, 0, 2, 1, 0, 0x22, 0, -1, 1},		/* bad ID CRC */
		{0, 0, 3, 1, 0, 0x33, 0, 9},			/* wrong track */
		{0, 0, 4, 1, 0, 0x44, 0, -1, 0, 0, 1},		/* no data */
		{0, 0, 5, 1, 0, 0x55, 0, -1},
		{0, 0, 5, 1, 0, 0x66, 0, -1},			/* duplicate number */
	};
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[256];
	u_int size = sizeof(buffer);

	build(0, sectors, 6);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(2, g.total_sectors);
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 3, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 4, buffer, &size));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 5, buffer, &size));
	ASSERT_EQUALS(0x55, buffer[0]);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_unavailable_sectors_excluded(void)
{
	check_unavailable_sectors_excluded(build_dmk_2t);
}

static void test_sdf_unavailable_sectors_excluded(void)
{
	check_unavailable_sectors_excluded(build_sdf_2t);
}

static void check_crc_error_then_rewrite(image_builder build)
{
	static const test_sector sectors[] = {
		{0, 0, 1, 1, 0, 0x11, 0, -1},
		{0, 0, 2, 1, 0, 0x22, 0, -1, 0, 1},
		{0, 0, 3, 1, 0, 0x33, 0, -1},
	};
	diskimage_id disk;
	diskimage_sector_info info;
	u_char buffer[512], out[256];
	u_int size = 256;

	build(0, sectors, 3);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));

	/* The data is still delivered, with the error. */
	ASSERT_CALL(EOS_CRC, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_EQUALS(0x22, buffer[0]);
	ASSERT_CALL(0, _diskimage_query_sector(disk, 1, &info));
	ASSERT_EQUALS(DISKIMAGE_SECTOR_CRC_ERROR, info.flags);

	memset(out, 0xC3, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 2, out, 256));
	size = 256;
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	_diskimage_close(disk);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_sector(disk, 1, &info));
	ASSERT_EQUALS(0, info.flags);
	size = 256;
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_MEM_EQUALS(out, buffer, 256);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_crc_error_then_rewrite(void)
{
	check_crc_error_then_rewrite(build_dmk_2t);
}

static void test_sdf_crc_error_then_rewrite(void)
{
	check_crc_error_then_rewrite(build_sdf_2t);
}

static void check_deleted_mark_kept(image_builder build)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 0, 0x11, 0xF8, -1}};
	diskimage_id disk;
	diskimage_sector_info info;
	u_char out[256];

	build(0, sectors, 1);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_sector(disk, 0, &info));
	ASSERT_EQUALS(DISKIMAGE_SECTOR_DELETED, info.flags);
	memset(out, 0x12, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_sector(disk, 0, &info));
	ASSERT_EQUALS(DISKIMAGE_SECTOR_DELETED, info.flags);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_deleted_mark_kept(void)
{
	check_deleted_mark_kept(build_dmk_2t);
}

static void test_sdf_deleted_mark_kept(void)
{
	check_deleted_mark_kept(build_sdf_2t);
}

static void check_protection_and_modes(image_builder build)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 0, 0x11, 0, -1}};
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[256];
	u_int size = sizeof(buffer);

	/* A write protected image can be read but not written. */
	build(0xFF, sectors, 1);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(1, g.write_protected);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_CALL(EOS_WP, _diskimage_write_sector(disk, 0, 0, 1, buffer, 256));
	_diskimage_close(disk);

	/* Neither can an image opened only for reading... */
	build(0, sectors, 1);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(EOS_BMODE, _diskimage_write_sector(disk, 0, 0, 1, buffer, 256));
	_diskimage_close(disk);

	/* ...and one opened only for writing cannot be read. */
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_WRITE));
	size = sizeof(buffer);
	ASSERT_CALL(EOS_BMODE, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_protection_and_modes(void)
{
	check_protection_and_modes(build_dmk_2t);
}

static void test_sdf_protection_and_modes(void)
{
	check_protection_and_modes(build_sdf_2t);
}

static void check_no_track_extension(image_builder build)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 0, 0x11, 0, -1}};
	diskimage_id disk;
	u_char out[300];

	build(0, sectors, 1);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	memset(out, 0x21, sizeof(out));
	ASSERT_CALL(EOS_SECT, _diskimage_write_sector(disk, 0, 0, 2, out, 256));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_no_track_extension(void)
{
	check_no_track_extension(build_dmk_2t);
}

static void test_sdf_no_track_extension(void)
{
	check_no_track_extension(build_sdf_2t);
}

/* DMK specifics */

static void test_dmk_double_sided(void)
{
	static const test_sector sectors[] = {
		{0, 0, 1, 1, 0, 0xA0, 0, -1},
		{0, 1, 1, 1, 0, 0xB0, 0, -1},
		{1, 0, 1, 1, 0, 0xC0, 0, -1},
		{1, 1, 1, 1, 0, 0xD0, 0, -1},
	};
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[1024];
	u_int size = sizeof(buffer);

	build_dmk(2, 2, 0, 0, sectors, 4);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(2, g.sides);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(0xA0, buffer[0]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 1, 1, buffer, &size));
	ASSERT_EQUALS(0xB0, buffer[0]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 1, buffer, &size));
	ASSERT_EQUALS(0xC0, buffer[0]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 1, 1, buffer, &size));
	ASSERT_EQUALS(0xD0, buffer[0]);

	/* Logical sectors go through both sides of a cylinder before the next. */
	ASSERT_CALL(0, _diskimage_query_sector(disk, 1, &info));
	ASSERT_EQUALS(0, info.cylinder);
	ASSERT_EQUALS(1, info.side);
	ASSERT_CALL(0, _diskimage_query_sector(disk, 2, &info));
	ASSERT_EQUALS(1, info.cylinder);
	ASSERT_EQUALS(0, info.side);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_single_density_doubled_on_write(void)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 1, 0x11, 0, -1}};
	/* File header, track header, then the doubled ID field, gap and mark. */
	long data_start = 16 + 128 + 2 * (7 + 17 + 1);
	diskimage_id disk;
	u_char buffer[256], out[256];
	u_int size = sizeof(buffer);

	build_dmk_2t(0, sectors, 1);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(0x11, buffer[255]);
	fill_pattern(out, 256, 0x40);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	read_image();
	ASSERT_EQUALS(0x40, image[data_start]);
	ASSERT_EQUALS(0x40, image[data_start + 1]);
	ASSERT_EQUALS(0x41, image[data_start + 2]);
	ASSERT_EQUALS(0x41, image[data_start + 3]);
	ASSERT_EQUALS(out[255], image[data_start + 511]);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_MEM_EQUALS(out, buffer, 256);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_single_density_one_byte_flag(void)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 1, 0x11, 0, -1}};
	long data_start = 16 + 128 + 7 + 17 + 1;
	diskimage_id disk;
	u_char buffer[256], out[256];
	u_int size = sizeof(buffer);

	build_dmk(1, 1, DMK_FLAG_SINGLE_SIDED | DMK_FLAG_SD_ONE_BYTE, 0, sectors, 1);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(0x11, buffer[0]);
	fill_pattern(out, 256, 0x40);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	read_image();
	ASSERT_EQUALS(0x40, image[data_start]);
	ASSERT_EQUALS(0x41, image[data_start + 1]);
	remove(TEST_IMAGE);
}

/* A sector whose data field contains another sector's ID refuses writes. */
static void test_dmk_nested_sector_not_written(void)
{
	static const test_sector outer[] = {{0, 0, 1, 1, 0, 0x4E, 0, -1}};
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[256];
	u_int size = sizeof(buffer);
	u_char *track = &image[16];
	u_char id[5] = {0xFE, 0, 0, 2, 1};
	unsigned short crc;
	long data = 128 + 3 + 7 + 22 + 12 + 3 + 1;
	long nested = data + 20;

	build_dmk(1, 1, DMK_FLAG_SINGLE_SIDED, 0, outer, 1);

	/* Plant an ID inside sector 1's data, then reseal sector 1's CRC. */
	memcpy(&track[nested], id, 5);
	crc = crc16(0xCDB4, id, 5);
	track[nested + 5] = (u_char)(crc >> 8);
	track[nested + 6] = (u_char)crc;
	{
		u_char field[257];

		field[0] = 0xFB;
		memcpy(&field[1], &track[data], 256);
		crc = crc16(0xCDB4, field, 257);
		track[data + 256] = (u_char)(crc >> 8);
		track[data + 257] = (u_char)crc;
	}
	track[2] = (u_char)nested;
	track[3] = (u_char)((nested >> 8) | 0x80);
	write_image(16 + DMK_TRACK_LENGTH);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(1, g.total_sectors);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_CALL(EOS_WRITE, _diskimage_write_sector(disk, 0, 0, 1, buffer, 256));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

/* SDF specifics */

static void test_sdf_offsets_point_past_marks(void)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 0, 0x11, 0, -1}};
	long record = 512;
	long data_start = record + 256 + 3 + 7 + 22 + 12 + 3 + 1;
	diskimage_id disk;
	u_char out[256];

	build_sdf(1, 1, 0, sectors, 1);
	ASSERT_EQUALS(data_start - record, (image[record + 10] | (image[record + 11] << 8)) & 0x3FFF);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	fill_pattern(out, 256, 0x70);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	read_image();
	ASSERT_EQUALS(0xFB, image[data_start - 1]);
	ASSERT_MEM_EQUALS(out, &image[data_start], 256);
	remove(TEST_IMAGE);
}

static void test_sdf_crc_flag_cleared_on_disk(void)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 0, 0x11, 0, -1, 0, 1}};
	long entry = 512 + 8;
	u_int before;
	diskimage_id disk;
	u_char out[256];

	build_sdf(1, 1, 0, sectors, 1);
	before = image[entry + 2] | (image[entry + 3] << 8);
	ASSERT("bad CRC flag starts set", before & 0x8000);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	memset(out, 0x31, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	read_image();
	ASSERT_EQUALS(before & 0x7FFF, (u_int)(image[entry + 2] | (image[entry + 3] << 8)));
	remove(TEST_IMAGE);
}

static void test_sdf_single_density_without_density_bit(void)
{
	static const test_sector sectors[] = {{0, 0, 1, 1, 1, 0x11, 0, -1}};
	/* Track record header, then the doubled ID field, gap and mark. */
	long record = 512;
	long data_start = record + 256 + 2 * (7 + 17 + 1);
	diskimage_id disk;
	diskimage_sector_info info;
	u_char buffer[256], out[256], field[257];
	u_int size = sizeof(buffer);
	unsigned short crc;
	int i;

	sdf_dmk2sdf_offsets = 1;
	build_sdf(1, 1, 0, sectors, 1);
	sdf_dmk2sdf_offsets = 0;
	ASSERT_EQUALS(0, image[record + 9] & 0x40);

	/* A varying pattern shows whether the doubled bytes are read correctly. */
	for (i = 0; i < 256; i++)
	{
		image[data_start + 2 * i] = (u_char)i;
		image[data_start + 2 * i + 1] = (u_char)i;
	}
	write_image(512 + SDF_RECORD);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_sector(disk, 0, &info));
	ASSERT_EQUALS(256, info.size);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	for (i = 0; i < 256; i++)
	{
		ASSERT_EQUALS(i, buffer[i]);
	}

	fill_pattern(out, 256, 0x40);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	/* The data and CRC are stored doubled, in their proper places. */
	read_image();
	field[0] = 0xFB;
	memcpy(&field[1], out, 256);
	crc = crc16(0xFFFF, field, 257);
	for (i = 0; i < 256; i++)
	{
		ASSERT_EQUALS(out[i], image[data_start + 2 * i]);
		ASSERT_EQUALS(out[i], image[data_start + 2 * i + 1]);
	}
	ASSERT_EQUALS(crc >> 8, image[data_start + 512]);
	ASSERT_EQUALS(crc >> 8, image[data_start + 513]);
	ASSERT_EQUALS(crc & 0xFF, image[data_start + 514]);
	ASSERT_EQUALS(crc & 0xFF, image[data_start + 515]);
	remove(TEST_IMAGE);
}

/* Create */

/* Check every logical sector of a new image holds the fill byte, with no
   CRC errors, reading by sector. */
static void check_created(u_int want_total, u_int size, u_char fill)
{
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[1024];
	u_int i, j, got;

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(want_total, g.total_sectors);
	ASSERT_EQUALS(size, g.sector_size);
	for (i = 0; i < want_total; i++)
	{
		ASSERT_CALL(0, _diskimage_query_sector(disk, i, &info));
		ASSERT_EQUALS(0, info.flags);
		got = sizeof(buffer);
		ASSERT_CALL(0, _diskimage_read_sector(disk, info.cylinder, info.side, info.sector, buffer, &got));
		ASSERT_EQUALS(size, got);
		for (j = 0; j < size; j++)
		{
			ASSERT_EQUALS(fill, buffer[j]);
		}
	}
	ASSERT_CALL(EOS_SECT, _diskimage_query_sector(disk, want_total, &info));
	_diskimage_close(disk);
}

static void test_create_raw(void)
{
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());
	ASSERT_EQUALS(35 * 18 * 256, read_image());
	check_created(35 * 18, 256, 0xFF);
	remove(TEST_IMAGE);

	/* Two sides double the size, and it opens as one side of more tracks. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 2, 3, 256, 18, 18, 1, 0xE5));
	ASSERT_EQUALS(2 * 3 * 18 * 256, read_image());
	check_created(2 * 3 * 18, 256, 0xE5);
	remove(TEST_IMAGE);
}

static void test_create_jvc(void)
{
	diskimage_id disk;
	diskimage_geometry g;
	long size;

	/* All defaults: no header, so it reads back as raw. */
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 1, 2, 256, 18, 0, 1, 0x11));
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());
	ASSERT_EQUALS(2 * 18 * 256, read_image());
	remove(TEST_IMAGE);

	/* Only sectors per track differs: a one byte header. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 1, 2, 256, 9, 0, 1, 0x22));
	ASSERT_EQUALS(DISKIMAGE_JVC, identify());
	size = read_image();
	ASSERT_EQUALS(1 + 2 * 9 * 256, size);
	ASSERT_EQUALS(9, image[0]);
	check_created(18, 256, 0x22);
	remove(TEST_IMAGE);

	/* Sides differ: two bytes. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 2, 2, 256, 18, 0, 1, 0x33));
	size = read_image();
	ASSERT_EQUALS(2 + 2 * 2 * 18 * 256, size);
	ASSERT_EQUALS(18, image[0]);
	ASSERT_EQUALS(2, image[1]);
	check_created(2 * 2 * 18, 256, 0x33);
	remove(TEST_IMAGE);

	/* Sector size differs: three bytes. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 1, 2, 512, 9, 0, 1, 0x44));
	ASSERT_EQUALS(3 + 2 * 9 * 512, read_image());
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	_diskimage_close(disk);
	ASSERT_EQUALS(2, g.cylinders);
	ASSERT_EQUALS(9, g.sectors_per_track);
	check_created(18, 512, 0x44);
	remove(TEST_IMAGE);
}

static void check_create_tracked(diskimage_format format)
{
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[256], out[256];
	u_int size = sizeof(buffer);

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 2, 3, 256, 18, 0, 1, 0xE5));
	ASSERT_EQUALS(format, identify());
	check_created(2 * 3 * 18, 256, 0xE5);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(format, g.format);
	ASSERT_EQUALS(3, g.cylinders);
	ASSERT_EQUALS(2, g.sides);
	ASSERT_EQUALS(18, g.sectors_per_track);
	ASSERT_EQUALS(1, g.first_sector);
	ASSERT_EQUALS(1, g.uniform);
	ASSERT_EQUALS(0, g.write_protected);

	/* A new image can be written. */
	fill_pattern(out, 256, 3);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 2, 1, 18, out, 256));
	_diskimage_close(disk);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 2, 1, 18, buffer, &size));
	ASSERT_MEM_EQUALS(out, buffer, 256);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_create_dmk(void)
{
	check_create_tracked(DISKIMAGE_DMK);
}

static void test_create_sdf(void)
{
	check_create_tracked(DISKIMAGE_SDF);
}

static void check_create_short_track0(diskimage_format format)
{
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[256];
	u_int size = sizeof(buffer);

	/* Only side 0 of track 0 is short. */
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 2, 2, 256, 18, 10, 1, 0x6D));
	check_created(2 * 2 * 18 - 18 + 10, 256, 0x6D);

	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(18, g.sectors_per_track);
	ASSERT_EQUALS(0, g.uniform);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 10, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 11, buffer, &size));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 1, 18, buffer, &size));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 18, buffer, &size));

	/* The next logical sector after the short side is side 1, sector 1. */
	ASSERT_CALL(0, _diskimage_query_sector(disk, 10, &info));
	ASSERT_EQUALS(0, info.cylinder);
	ASSERT_EQUALS(1, info.side);
	ASSERT_EQUALS(1, info.sector);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_create_dmk_short_track0(void)
{
	check_create_short_track0(DISKIMAGE_DMK);
}

static void test_create_sdf_short_track0(void)
{
	check_create_short_track0(DISKIMAGE_SDF);
}

static void put_be(u_char *p, u_int value, int size)
{
	int i;

	for (i = size - 1; i >= 0; i--)
	{
		p[i] = (u_char)value;
		value >>= 8;
	}
}

/* Fill slots with their slot number + 1 and add an OS-9 LSN0 of that shape. */
static long build_raw_lsn0(int slots, u_int total, u_int spt, u_int sct, u_int t0s, int osk)
{
	long size = build_linear(NULL, 0, slots, 256);
	lsn0_sect *lsn0 = (lsn0_sect *)image;

	memset(image, 0, 256);
	put_be(lsn0->dd_tot, total, 3);
	put_be(lsn0->dd_spt, spt, 2);
	if (osk)
	{
		memcpy(lsn0->dd_sync, "Cruz", 4);
		put_be(lsn0->dd_opt.m68k.pd_sct, sct, 2);
		put_be(lsn0->dd_opt.m68k.pd_t0s, t0s, 2);
	}
	else
	{
		put_be(lsn0->dd_opt.m6809.pd_sct, sct, 2);
		put_be(lsn0->dd_opt.m6809.pd_t0s, t0s, 2);
	}
	return size;
}

static void check_raw_short_track0(int osk)
{
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[256];
	u_int size = sizeof(buffer);

	/* 2 tracks in 36 slots, with 10 sectors used on track 0: 28 sectors. */
	write_image(build_raw_lsn0(36, 28, 18, 18, 10, osk));
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(28, g.total_sectors);
	ASSERT_EQUALS(2, g.cylinders);
	ASSERT_EQUALS(18, g.sectors_per_track);
	ASSERT_EQUALS(10, g.sectors_track0);
	ASSERT_EQUALS(0, g.uniform);

	/* Sector numbers still address the physical slots. */
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 10, buffer, &size));
	ASSERT_EQUALS(10, buffer[0]);
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 11, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 18, buffer, &size));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 1, buffer, &size));
	ASSERT_EQUALS(19, buffer[0]);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 18, buffer, &size));
	ASSERT_EQUALS(36, buffer[0]);

	ASSERT_CALL(0, _diskimage_query_sector(disk, 10, &info));
	ASSERT_EQUALS(1, info.cylinder);
	ASSERT_EQUALS(1, info.sector);

	/* LSN 9 and LSN 10 are next to each other in logical order. */
	ASSERT_CALL(0, _diskimage_query_sector(disk, 9, &info));
	ASSERT_EQUALS(0, info.cylinder);
	ASSERT_EQUALS(10, info.sector);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_raw_short_track0(void)
{
	check_raw_short_track0(0);
}

static void test_raw_short_track0_osk(void)
{
	check_raw_short_track0(1);
}

/* os9 format writes only the low sectors, so the file can end anywhere. */
static void test_raw_short_track0_short_file(void)
{
	diskimage_id disk;
	diskimage_geometry g;
	u_char out[256];

	/* Ending inside the unused slots: only the used ones count. */
	write_image(build_raw_lsn0(11, 28, 18, 18, 10, 0));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(10, g.total_sectors);
	ASSERT_EQUALS(10, g.sectors_track0);
	ASSERT_EQUALS(1, g.cylinders);

	/* Writing the next sector goes past the unused slots, which are zeroed. */
	memset(out, 0x5C, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 1, 0, 2, out, 256));
	ASSERT_CALL(EOS_SECT, _diskimage_write_sector(disk, 0, 0, 11, out, 256));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(12, g.total_sectors);
	ASSERT_EQUALS(2, g.cylinders);
	_diskimage_close(disk);

	ASSERT_EQUALS(20 * 256, read_image());
	ASSERT_EQUALS(11, image[10 * 256]);
	ASSERT_EQUALS(0, count_other(11 * 256, 19 * 256, 0x00));
	ASSERT_EQUALS(0x5C, image[19 * 256]);
	remove(TEST_IMAGE);
}

/* Any disagreement in LSN0 means it is not describing a short track 0. */
static void test_raw_short_track0_needs_consistent_lsn0(void)
{
	static const struct { int slots; u_int total, spt, sct, t0s; } cases[] = {
		{36, 28, 16, 18, 10},	/* wrong sectors per track */
		{36, 28, 18, 16, 10},	/* wrong descriptor sectors per track */
		{36, 29, 18, 18, 10},	/* total does not fit the shape */
		{36, 28, 18, 18, 0},	/* no track 0 count */
		{36, 28, 18, 18, 18},	/* not short */
		{36, 28, 18, 18, 19},	/* not short */
		{36, 5, 18, 18, 10},	/* fewer sectors than track 0 */
		{54, 28, 18, 18, 10},	/* file longer than the disk */
	};
	diskimage_id disk;
	diskimage_geometry g;
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		write_image(build_raw_lsn0(cases[i].slots, cases[i].total, cases[i].spt, cases[i].sct, cases[i].t0s, 0));
		ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
		ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
		_diskimage_close(disk);
		ASSERT_EQUALS(cases[i].slots, g.total_sectors);
		ASSERT_EQUALS(18, g.sectors_track0);
	}
	remove(TEST_IMAGE);
}

static void test_create_raw_short_track0(void)
{
	diskimage_id disk;
	diskimage_geometry g;
	u_char out[256];

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 2, 2, 256, 18, 10, 1, 0xE5));
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());

	/* The file keeps every slot, and reports only the real sectors. */
	ASSERT_EQUALS(2 * 2 * 18 * 256, read_image());
	ASSERT_EQUALS(0xE5, image[256]);
	ASSERT_EQUALS(0xE5, image[10 * 256]);
	ASSERT_EQUALS(0xE5, image[2 * 2 * 18 * 256 - 1]);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(2 * 2 * 18 - 18 + 10, g.total_sectors);
	ASSERT_EQUALS(10, g.sectors_track0);

	/* The first logical sector past track 0 lands in the file's track 1. */
	memset(out, 0x3C, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 1, 0, 1, out, 256));
	_diskimage_close(disk);
	read_image();
	ASSERT_EQUALS(0x3C, image[18 * 256]);
	ASSERT_EQUALS(0xE5, image[18 * 256 - 1]);
	remove(TEST_IMAGE);
}

/* Read the sector numbers of a track in the order they are recorded, from
   a DMK or SDF image loaded by read_image(). */
static int recorded_order(diskimage_format format, int track_index, u_char *order)
{
	int count = 0;
	int i;

	if (format == DISKIMAGE_DMK)
	{
		const u_char *track = &image[16 + (long)track_index * (image[2] | (image[3] << 8))];
		int previous = 0;

		for (i = 0; i < 64; i++)
		{
			int position = (track[i * 2] | (track[i * 2 + 1] << 8)) & 0x3FFF;

			if (position == 0)
			{
				break;
			}
			if (position <= previous)
			{
				return -1;	/* DMK pointers must ascend */
			}
			previous = position;
			order[count++] = track[position + 3];
		}
	}
	else
	{
		const u_char *record = &image[512 + (long)track_index * SDF_RECORD];
		int previous = 0;

		count = record[0];
		for (i = 0; i < count; i++)
		{
			const u_char *entry = &record[8 + i * 8];
			int position = (entry[0] | (entry[1] << 8)) & 0x3FFF;

			if (position <= previous || record[position + 2] != entry[6])
			{
				return -1;	/* the table must match the track */
			}
			previous = position;
			order[i] = entry[6];
		}
	}
	return count;
}

static void check_create_interleave(diskimage_format format)
{
	static const u_char sequential[18] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
	static const u_char three[18] = {1, 7, 13, 2, 8, 14, 3, 9, 15, 4, 10, 16, 5, 11, 17, 6, 12, 18};
	static const u_char two[18] = {1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7, 16, 8, 17, 9, 18};
	static const u_char short_three[10] = {1, 8, 5, 2, 9, 6, 3, 10, 7, 4};
	diskimage_id disk;
	diskimage_sector_info info;
	diskimage_geometry g;
	u_char order[64], buffer[256];
	u_int size = sizeof(buffer);
	int i;

	/* 0 and 1 are both in order. */
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 0, 0, 0xE5));
	read_image();
	ASSERT_EQUALS(18, recorded_order(format, 0, order));
	ASSERT_MEM_EQUALS(sequential, order, 18);
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 0, 1, 0xE5));
	read_image();
	ASSERT_EQUALS(18, recorded_order(format, 1, order));
	ASSERT_MEM_EQUALS(sequential, order, 18);
	remove(TEST_IMAGE);

	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 0, 2, 0xE5));
	read_image();
	ASSERT_EQUALS(18, recorded_order(format, 0, order));
	ASSERT_MEM_EQUALS(two, order, 18);
	remove(TEST_IMAGE);

	/* Every track, on both sides, follows the same order. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 2, 2, 256, 18, 0, 3, 0xE5));
	read_image();
	for (i = 0; i < 4; i++)
	{
		ASSERT_EQUALS(18, recorded_order(format, i, order));
		ASSERT_MEM_EQUALS(three, order, 18);
	}
	check_created(2 * 2 * 18, 256, 0xE5);

	/* Addressing and logical order still follow sector numbers. */
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(1, g.uniform);
	ASSERT_EQUALS(1, g.first_sector);
	for (i = 0; i < 18; i++)
	{
		ASSERT_CALL(0, _diskimage_query_sector(disk, i, &info));
		ASSERT_EQUALS(i + 1, info.sector);
	}
	memset(buffer, 0x7A, sizeof(buffer));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 2, buffer, 256));
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(0xE5, buffer[255]);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_EQUALS(0x7A, buffer[0]);
	_diskimage_close(disk);
	remove(TEST_IMAGE);

	/* A short track 0 uses the same spacing over its own length. */
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 10, 3, 0xE5));
	read_image();
	ASSERT_EQUALS(10, recorded_order(format, 0, order));
	ASSERT_MEM_EQUALS(short_three, order, 10);
	ASSERT_EQUALS(18, recorded_order(format, 1, order));
	ASSERT_MEM_EQUALS(three, order, 18);
	check_created(10 + 18, 256, 0xE5);
	remove(TEST_IMAGE);

	/* Interleave can't reach or pass a whole track, except a track of one. */
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 0, 18, 0xE5));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 0, 19, 0xE5));
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 1, 0, 1, 0xE5));
	check_created(2, 256, 0xE5);
	remove(TEST_IMAGE);
}

static void test_create_dmk_interleave(void)
{
	check_create_interleave(DISKIMAGE_DMK);
}

static void test_create_sdf_interleave(void)
{
	check_create_interleave(DISKIMAGE_SDF);
}

/* Raw, JVC and VDK have no order to record, so the files come out the same. */
static void test_create_interleave_ignored_without_order(void)
{
	static const diskimage_format formats[] = {DISKIMAGE_RAW, DISKIMAGE_JVC, DISKIMAGE_VDK};
	static u_char plain[64 * 1024];
	size_t i;
	long size;

	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
	{
		remove(TEST_IMAGE);
		ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, formats[i], 2, 2, 256, 18, 0, 1, 0xE5));
		size = read_image();
		memcpy(plain, image, size);
		remove(TEST_IMAGE);
		ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, formats[i], 2, 2, 256, 18, 0, 3, 0xE5));
		ASSERT_EQUALS(size, read_image());
		ASSERT_MEM_EQUALS(plain, image, size);
		remove(TEST_IMAGE);
		ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, formats[i], 2, 2, 256, 18, 0, 99, 0xE5));
		remove(TEST_IMAGE);
	}
}

/* Created images must match these byte for byte.  They hold a blank RS-DOS
   disk of 35 tracks of 18 sectors, at interleave 5, filled with $FF.  The
   tests run in build/unix/unittest. */
#define TESTDATA "../../../unittest/testdata/"

static u_char expected_image[sizeof(image)];

static long read_expected(const char *pathname)
{
	FILE *fp = fopen(pathname, "rb");
	long size;

	if (fp == NULL)
	{
		return -1;
	}
	size = (long)fread(expected_image, 1, sizeof(expected_image), fp);
	fclose(fp);
	return size;
}

/* Find where two images first differ, or -1 if they do not. */
static long first_difference(long size)
{
	long i;

	for (i = 0; i < size; i++)
	{
		if (image[i] != expected_image[i])
		{
			return i;
		}
	}
	return -1;
}

static void check_create_matches_testdata(diskimage_format format, const char *pathname)
{
	long expected_size = read_expected(pathname);

	ASSERT("test image must be readable", expected_size > 0);
	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 35, 256, 18, 0, 5, 0xFF));
	ASSERT_EQUALS(expected_size, read_image());
	ASSERT_EQUALS(-1, first_difference(expected_size));
	remove(TEST_IMAGE);
}

static void test_create_dmk_matches_testdata(void)
{
	check_create_matches_testdata(DISKIMAGE_DMK, TESTDATA "rsdos artifact.dmk");
}

static void test_create_sdf_matches_testdata(void)
{
	check_create_matches_testdata(DISKIMAGE_SDF, TESTDATA "rsdos artifact.sdf");
}

/* Images this library did not create must read correctly too. */
static void check_testdata_reads(diskimage_format format, const char *pathname)
{
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	diskimage_format found = DISKIMAGE_UNKNOWN;
	u_char buffer[256];
	u_int size, i, j;

	ASSERT_CALL(0, _diskimage_identify(pathname, &found));
	ASSERT_EQUALS(format, found);
	ASSERT_CALL(0, _diskimage_open(&disk, pathname, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(35, g.cylinders);
	ASSERT_EQUALS(1, g.sides);
	ASSERT_EQUALS(18, g.sectors_per_track);
	ASSERT_EQUALS(18, g.sectors_track0);
	ASSERT_EQUALS(256, g.sector_size);
	ASSERT_EQUALS(35 * 18, g.total_sectors);
	ASSERT_EQUALS(1, g.uniform);
	for (i = 0; i < g.total_sectors; i++)
	{
		ASSERT_CALL(0, _diskimage_query_sector(disk, i, &info));
		ASSERT_EQUALS(i / 18, info.cylinder);
		ASSERT_EQUALS(i % 18 + 1, info.sector);
		ASSERT_EQUALS(0, info.flags);
		size = sizeof(buffer);
		ASSERT_CALL(0, _diskimage_read_sector(disk, info.cylinder, 0, info.sector, buffer, &size));
		for (j = 0; j < 256; j++)
		{
			ASSERT_EQUALS(0xFF, buffer[j]);
		}
	}
	_diskimage_close(disk);
}

static void test_dmk_testdata_reads(void)
{
	check_testdata_reads(DISKIMAGE_DMK, TESTDATA "rsdos artifact.dmk");
}

static void test_sdf_testdata_reads(void)
{
	check_testdata_reads(DISKIMAGE_SDF, TESTDATA "rsdos artifact.sdf");
}

static void check_create_sizes(diskimage_format format)
{
	static const struct { u_int size, sectors; } layouts[] = {{128, 16}, {256, 18}, {512, 9}, {1024, 5}};
	size_t i;

	for (i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++)
	{
		remove(TEST_IMAGE);
		ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, layouts[i].size, layouts[i].sectors, 0, 1, 0xA1));
		check_created(2 * layouts[i].sectors, layouts[i].size, 0xA1);
		remove(TEST_IMAGE);
	}
}

static void test_create_dmk_sizes(void)
{
	check_create_sizes(DISKIMAGE_DMK);
}

static void test_create_sdf_sizes(void)
{
	check_create_sizes(DISKIMAGE_SDF);
}

/* A fill byte that looks like an address mark must not confuse a reader. */
static void test_create_fill_like_marks(void)
{
	static const u_char fills[] = {0xFE, 0xFB, 0xF8, 0xA1, 0x00};
	size_t i;

	for (i = 0; i < sizeof(fills); i++)
	{
		remove(TEST_IMAGE);
		ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 2, 256, 18, 0, 1, fills[i]));
		check_created(36, 256, fills[i]);
		remove(TEST_IMAGE);
		ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_SDF, 1, 2, 256, 18, 0, 1, fills[i]));
		check_created(36, 256, fills[i]);
		remove(TEST_IMAGE);
	}
}

/* VDK */

/* A VDK image with sectors holding their index + 1, and no flags set. */
static long build_vdk(int header_size, int tracks, int sides, int sectors_per_track)
{
	long size = build_linear(NULL, 0, tracks * sides * sectors_per_track, 256);

	memmove(&image[header_size], image, size);
	memset(image, 0, header_size);
	image[0] = 'd';
	image[1] = 'k';
	image[2] = (u_char)header_size;
	image[3] = (u_char)(header_size >> 8);
	image[4] = 0x10;
	image[5] = 0x10;
	image[8] = (u_char)tracks;
	image[9] = (u_char)sides;
	return header_size + size;
}

static void test_vdk_geometry_and_sectors(void)
{
	diskimage_id disk;
	diskimage_geometry g;
	diskimage_sector_info info;
	u_char buffer[256];
	u_int size = sizeof(buffer);

	/* 3 tracks on 2 sides: 108 sectors. */
	write_image(build_vdk(12, 3, 2, 18));
	ASSERT_EQUALS(DISKIMAGE_VDK, identify());
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(DISKIMAGE_VDK, g.format);
	ASSERT_EQUALS(3, g.cylinders);
	ASSERT_EQUALS(2, g.sides);
	ASSERT_EQUALS(18, g.sectors_per_track);
	ASSERT_EQUALS(18, g.sectors_track0);
	ASSERT_EQUALS(256, g.sector_size);
	ASSERT_EQUALS(1, g.first_sector);
	ASSERT_EQUALS(108, g.total_sectors);
	ASSERT_EQUALS(1, g.uniform);
	ASSERT_EQUALS(0, g.write_protected);
	ASSERT_EQUALS(0, g.extendable);

	/* Track 1, side 1, sector 2 follows 3 whole tracks: index 55. */
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 1, 2, buffer, &size));
	ASSERT_EQUALS(56, buffer[0]);
	ASSERT_EQUALS(56, buffer[255]);
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 3, 0, 1, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 2, 1, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 0, buffer, &size));
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 19, buffer, &size));

	ASSERT_CALL(0, _diskimage_query_sector(disk, 55, &info));
	ASSERT_EQUALS(1, info.cylinder);
	ASSERT_EQUALS(1, info.side);
	ASSERT_EQUALS(2, info.sector);

	/* The first sector starts after the header. */
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_EQUALS(1, buffer[0]);
	ASSERT_EQUALS(1, buffer[255]);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

/* The header size field decides where sectors start, and the flags and
   any extension bytes are left alone by writes. */
static void test_vdk_long_header_and_flags_kept(void)
{
	diskimage_id disk;
	u_char buffer[256], out[256];
	u_int size = sizeof(buffer);
	long length = build_vdk(44, 2, 1, 18);

	image[10] = 0xA4;
	image[11] = 0x28;
	memcpy(&image[12], "NAME", 4);
	write_image(length);

	ASSERT_EQUALS(DISKIMAGE_VDK, identify());
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 2, buffer, &size));
	ASSERT_EQUALS(2, buffer[0]);

	memset(out, 0x7E, sizeof(out));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));

	/* VDK images do not grow. */
	ASSERT_CALL(EOS_SECT, _diskimage_write_sector(disk, 2, 0, 1, out, 256));
	ASSERT_CALL(EOS_IA, _diskimage_write_sector(disk, 0, 0, 1, out, 128));
	_diskimage_close(disk);

	ASSERT_EQUALS(length, read_image());
	ASSERT_EQUALS(0xA4, image[10]);
	ASSERT_EQUALS(0x28, image[11]);
	ASSERT_MEM_EQUALS("NAME", &image[12], 4);
	ASSERT_EQUALS(0x7E, image[44]);
	ASSERT_EQUALS(0x7E, image[44 + 255]);
	ASSERT_EQUALS(2, image[44 + 256]);
	remove(TEST_IMAGE);
}

/* With no field for it, sectors per track come from the file size. */
static void test_vdk_other_sectors_per_track(void)
{
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[256];
	u_int size = sizeof(buffer);

	write_image(build_vdk(12, 2, 1, 10));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(10, g.sectors_per_track);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 1, 0, 1, buffer, &size));
	ASSERT_EQUALS(11, buffer[0]);
	ASSERT_CALL(EOS_SECT, _diskimage_read_sector(disk, 0, 0, 11, buffer, &size));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_vdk_rejects(void)
{
	long length;

	/* Not whole tracks, as a compressed or truncated file would be. */
	length = build_vdk(12, 2, 1, 18);
	write_image(length - 256);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	/* Header fields that cannot be right. */
	length = build_vdk(12, 2, 1, 18);
	image[9] = 3;
	write_image(length);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	length = build_vdk(12, 2, 1, 18);
	image[9] = 0;
	write_image(length);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	length = build_vdk(12, 2, 1, 18);
	image[8] = 0;
	write_image(length);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	length = build_vdk(12, 2, 1, 18);
	image[2] = 11;
	write_image(length);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	/* No sectors at all. */
	build_vdk(12, 2, 1, 18);
	write_image(12);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());

	/* The magic bytes are required, and a raw disk may start with data
	   that merely resembles them. */
	length = build_vdk(12, 2, 1, 18);
	image[1] = 'K';
	write_image(length);
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, identify());
	length = build_linear(NULL, 0, 18, 256);
	memcpy(image, "dk", 2);
	write_image(length);
	ASSERT_EQUALS(DISKIMAGE_RAW, identify());
	remove(TEST_IMAGE);
}

/* Write protect, and the override, work the same in each format. */
static void build_vdk_protectable(u_char protect, const test_sector *sectors, int count)
{
	(void)sectors;
	(void)count;
	long length = build_vdk(12, 2, 1, 18);

	/* Only bit 0 protects, so the other flags are set without it. */
	image[10] = protect ? 0x01 : 0xFE;
	write_image(length);
}

static void check_write_protect_override(image_builder build, int flag_offset, u_char flag_value)
{
	static const test_sector sectors[] = {
		{0, 0, 1, 1, 0, 0x11, 0, -1},
		{1, 0, 1, 1, 0, 0x11, 0, -1},
	};
	diskimage_id disk;
	diskimage_geometry g;
	u_char buffer[256], out[256];
	u_int size;

	fill_pattern(out, 256, 9);

	/* Protected: writes are refused, and the file is unchanged. */
	build(0xFF, sectors, 2);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(1, g.write_protected);
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_CALL(EOS_WP, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	/* The override writes, and still reports the flag. */
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW | DISKIMAGE_IGNORE_WRITE_PROTECT));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(1, g.write_protected);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	ASSERT_CALL(0, _diskimage_write_sector(disk, 1, 0, 1, out, 256));
	_diskimage_close(disk);

	/* The flag on disk is untouched, and protects the next open as before. */
	read_image();
	ASSERT_EQUALS(flag_value, image[flag_offset]);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	size = sizeof(buffer);
	ASSERT_CALL(0, _diskimage_read_sector(disk, 0, 0, 1, buffer, &size));
	ASSERT_MEM_EQUALS(out, buffer, 256);
	ASSERT_CALL(EOS_WP, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);

	/* The override does not stand in for opening with write access. */
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ | DISKIMAGE_IGNORE_WRITE_PROTECT));
	ASSERT_CALL(EOS_BMODE, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);
	ASSERT_CALL(EOS_BMODE, _diskimage_open(&disk, TEST_IMAGE, DISKIMAGE_IGNORE_WRITE_PROTECT));

	/* Not protected: no override is needed. */
	build(0, sectors, 2);
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &g));
	ASSERT_EQUALS(0, g.write_protected);
	ASSERT_CALL(0, _diskimage_write_sector(disk, 0, 0, 1, out, 256));
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_dmk_write_protect_override(void)
{
	check_write_protect_override(build_dmk_2t, 0, 0xFF);
}

static void test_sdf_write_protect_override(void)
{
	check_write_protect_override(build_sdf_2t, 6, 0xFF);
}

static void test_vdk_write_protect_override(void)
{
	check_write_protect_override(build_vdk_protectable, 10, 0x01);
}

static void test_create_vdk(void)
{
	static const u_char header[12] = {'d', 'k', 12, 0, 0x10, 0x10, 'S', 1, 35, 2, 0, 0};
	long expected = 12 + 35L * 2 * 18 * 256;
	long i;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_VDK, 2, 35, 256, 18, 0, 1, 0xE5));
	ASSERT_EQUALS(DISKIMAGE_VDK, identify());
	ASSERT_EQUALS(expected, read_image());
	ASSERT_MEM_EQUALS(header, image, 12);
	for (i = 12; i < expected; i++)
	{
		if (image[i] != 0xE5)
		{
			break;
		}
	}
	ASSERT_EQUALS(expected, i);
	check_created(35 * 2 * 18, 256, 0xE5);
	remove(TEST_IMAGE);

	/* Only the shape every reader assumes. */
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_VDK, 1, 35, 512, 18, 0, 1, 0xE5));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_VDK, 1, 35, 256, 16, 0, 1, 0xE5));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_VDK, 1, 35, 256, 18, 10, 1, 0xE5));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_VDK, 1, 256, 256, 18, 0, 1, 0xE5));
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_VDK, 1, 255, 256, 18, 0, 1, 0xE5));
	ASSERT_EQUALS(DISKIMAGE_VDK, identify());
	remove(TEST_IMAGE);
}

/* An image open twice sees what the other open added, and does not overwrite it. */
static void test_raw_growth_seen_by_other_open(void)
{
	diskimage_id first, second;
	u_char data[256], back[256];
	u_int size = sizeof(back);

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 1, 256, 18, 0, 0, 0xFF));
	ASSERT_CALL(0, _diskimage_open(&first, TEST_IMAGE, RW));
	ASSERT_CALL(0, _diskimage_open(&second, TEST_IMAGE, RW));

	fill_pattern(data, sizeof(data), 0x11);
	ASSERT_CALL(0, _diskimage_write_sector(first, 3, 0, 5, data, sizeof(data)));
	ASSERT_CALL(0, _diskimage_read_sector(second, 3, 0, 5, back, &size));
	ASSERT_EQUALS(0, memcmp(data, back, sizeof(data)));

	/* Writing past the new end must keep the sector already added. */
	ASSERT_CALL(0, _diskimage_write_sector(second, 4, 0, 1, data, sizeof(data)));
	size = sizeof(back);
	ASSERT_CALL(0, _diskimage_read_sector(first, 3, 0, 5, back, &size));
	ASSERT_EQUALS(0, memcmp(data, back, sizeof(data)));

	_diskimage_close(first);
	_diskimage_close(second);
	remove(TEST_IMAGE);
}

static void check_set_write_protect(diskimage_format format)
{
	diskimage_geometry geometry;
	diskimage_id disk;

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, format, 1, 2, 256, 18, 0, 0, 0xFF));

	ASSERT_CALL(0, _diskimage_set_write_protect(TEST_IMAGE, 1));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &geometry));
	ASSERT_EQUALS(format, geometry.format);
	ASSERT_EQUALS(1, geometry.write_protected);
	_diskimage_close(disk);

	ASSERT_CALL(0, _diskimage_set_write_protect(TEST_IMAGE, 0));
	ASSERT_CALL(0, _diskimage_open(&disk, TEST_IMAGE, FAM_READ));
	ASSERT_CALL(0, _diskimage_query_geometry(disk, &geometry));
	ASSERT_EQUALS(0, geometry.write_protected);
	_diskimage_close(disk);
	remove(TEST_IMAGE);
}

static void test_set_write_protect(void)
{
	check_set_write_protect(DISKIMAGE_DMK);
	check_set_write_protect(DISKIMAGE_SDF);
	check_set_write_protect(DISKIMAGE_VDK);

	remove(TEST_IMAGE);
	ASSERT_CALL(0, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 2, 256, 18, 0, 0, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_set_write_protect(TEST_IMAGE, 1));
	remove(TEST_IMAGE);
}

static void test_create_keeps_existing_file(void)
{
	write_image(build_linear(NULL, 0, 2, 256));
	ASSERT_CALL(EOS_FAE, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_EQUALS(2 * 256, read_image());
	ASSERT_EQUALS(1, image[0]);
	remove(TEST_IMAGE);
}

static void test_create_rejects(void)
{
	remove(TEST_IMAGE);

	/* Any format */
	ASSERT_CALL(EOS_IA, _diskimage_create(NULL, DISKIMAGE_DMK, 1, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_UNKNOWN, 1, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 0, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 3, 35, 256, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 0, 256, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 35, 300, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 35, 256, 0, 0, 1, 0xFF));

	/* Raw is always 18 sectors of 256 bytes, and track 0 can only be short. */
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 512, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 16, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 19, 1, 0xFF));

	/* JVC: uniform tracks, and a header length that can be found. */
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 1, 35, 256, 18, 10, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 1, 35, 256, 256, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_JVC, 1, 1, 128, 3, 0, 1, 0xFF));

	/* DMK and SDF limits */
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 256, 256, 18, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_SDF, 1, 81, 256, 18, 0, 1, 0xFF));

	/* More than fits in a track, on all tracks or only on track 0. */
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 35, 256, 20, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_SDF, 1, 35, 256, 20, 0, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_DMK, 1, 35, 256, 18, 20, 1, 0xFF));
	ASSERT_CALL(EOS_IA, _diskimage_create(TEST_IMAGE, DISKIMAGE_SDF, 1, 35, 1024, 6, 0, 1, 0xFF));

	/* A refused request leaves no file behind. */
	ASSERT_EQUALS(EOS_PNNF, _diskimage_identify(TEST_IMAGE, (diskimage_format[]){DISKIMAGE_UNKNOWN}));
}

static void test_format_names(void)
{
	ASSERT_EQUALS(DISKIMAGE_RAW, _diskimage_format_named("raw"));
	ASSERT_EQUALS(DISKIMAGE_DMK, _diskimage_format_named("DMK"));
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, _diskimage_format_named("dsk"));
	ASSERT_EQUALS(DISKIMAGE_SDF, _diskimage_format_of_extension("a/b.sdf"));
	ASSERT_EQUALS(DISKIMAGE_VDK, _diskimage_format_of_extension("x.y.VDK"));
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, _diskimage_format_of_extension("x.dsk"));
	ASSERT_EQUALS(DISKIMAGE_UNKNOWN, _diskimage_format_of_extension("x"));
}

int main(void)
{
	RUN(test_identify_missing_file);
	RUN(test_identify_directory);
	RUN(test_identify_formats);
	RUN(test_identify_rejects);
	RUN(test_open_bad_mode);

	RUN(test_raw_geometry);
	RUN(test_raw_read_write_sector);
	RUN(test_raw_extend_by_sector);
	RUN(test_raw_write_without_gap_adds_no_zeros);

	RUN(test_jvc_header_geometry);
	RUN(test_jvc_short_header_uses_defaults);
	RUN(test_jvc_write_and_no_extend);
	RUN(test_jvc_sector_attributes);

	RUN(test_dmk_geometry_and_order);
	RUN(test_sdf_geometry_and_order);
	RUN(test_dmk_mixed_sizes);
	RUN(test_sdf_mixed_sizes);
	RUN(test_dmk_unavailable_sectors_excluded);
	RUN(test_sdf_unavailable_sectors_excluded);
	RUN(test_dmk_crc_error_then_rewrite);
	RUN(test_sdf_crc_error_then_rewrite);
	RUN(test_dmk_deleted_mark_kept);
	RUN(test_sdf_deleted_mark_kept);
	RUN(test_dmk_protection_and_modes);
	RUN(test_sdf_protection_and_modes);
	RUN(test_dmk_no_track_extension);
	RUN(test_sdf_no_track_extension);

	RUN(test_dmk_double_sided);
	RUN(test_dmk_single_density_doubled_on_write);
	RUN(test_dmk_single_density_one_byte_flag);
	RUN(test_dmk_nested_sector_not_written);

	RUN(test_sdf_offsets_point_past_marks);
	RUN(test_sdf_crc_flag_cleared_on_disk);
	RUN(test_sdf_single_density_without_density_bit);

	RUN(test_raw_short_track0);
	RUN(test_raw_short_track0_osk);
	RUN(test_raw_short_track0_short_file);
	RUN(test_raw_short_track0_needs_consistent_lsn0);

	RUN(test_create_raw);
	RUN(test_create_raw_short_track0);
	RUN(test_create_jvc);
	RUN(test_create_dmk);
	RUN(test_create_sdf);
	RUN(test_create_dmk_short_track0);
	RUN(test_create_sdf_short_track0);
	RUN(test_create_dmk_sizes);
	RUN(test_create_sdf_sizes);
	RUN(test_create_dmk_interleave);
	RUN(test_create_sdf_interleave);
	RUN(test_create_interleave_ignored_without_order);
	RUN(test_create_dmk_matches_testdata);
	RUN(test_create_sdf_matches_testdata);
	RUN(test_dmk_testdata_reads);
	RUN(test_sdf_testdata_reads);
	RUN(test_vdk_geometry_and_sectors);
	RUN(test_vdk_long_header_and_flags_kept);
	RUN(test_vdk_other_sectors_per_track);
	RUN(test_vdk_rejects);
	RUN(test_dmk_write_protect_override);
	RUN(test_sdf_write_protect_override);
	RUN(test_vdk_write_protect_override);
	RUN(test_create_vdk);
	RUN(test_create_fill_like_marks);
	RUN(test_create_keeps_existing_file);
	RUN(test_raw_growth_seen_by_other_open);
	RUN(test_set_write_protect);
	RUN(test_create_rejects);

	RUN(test_format_names);
	return TEST_REPORT();
}

/*
 * Damage the disk images in files, as tests of what reads them need.
 *
 * DMK and SDF images record each sector's ID, so one can be made unfindable or
 * given a bad CRC.  JVC images can do both only with sector attributes, which
 * image_add_jvc_attributes() adds.  Raw and VDK images have no way to say so.
 *
 * Include this after tinytest.h and diskimage.h.
 */
#ifndef IMAGEDAMAGE_H
#define IMAGEDAMAGE_H

#include <stdio.h>
#include <stdlib.h>

#define DAMAGE_DMK_HEADER_SIZE		16
#define DAMAGE_DMK_POINTERS		64
#define DAMAGE_DMK_SINGLE_SIDED		0x10
#define DAMAGE_SDF_HEADER_SIZE		512
#define DAMAGE_SDF_RECORD_SIZE		6656
#define DAMAGE_SDF_TABLE_OFFSET		8
#define DAMAGE_SDF_ENTRY_SIZE		8
#define DAMAGE_SDF_BAD_DATA_CRC		0x80
#define DAMAGE_JVC_HEADER_SIZE		5
#define DAMAGE_JVC_RECORD_SIZE		257
#define DAMAGE_JVC_CRC_ERROR		0x08
#define DAMAGE_JVC_NOT_FOUND		0x10
#define DAMAGE_WRONG_TRACK		0xEE

/* ASSERT_EQUALS evaluates its second argument twice, so calls with side
   effects keep their result here first. */
#define ASSERT_EC(expected, call) \
	{ \
		error_code assert_ec = (call); \
		ASSERT_EQUALS(expected, assert_ec); \
	}


static inline void damage_flip_byte(FILE *fp, long offset)
{
	int byte;

	fseek(fp, offset, SEEK_SET);
	byte = fgetc(fp);
	fseek(fp, offset, SEEK_SET);
	fputc(byte ^ 0xFF, fp);
}


/* Find a sector's ID mark in a DMK image, or -1 if it has none. */
static inline long damage_dmk_id(FILE *fp, int cylinder, int side, int sector)
{
	u_char header[DAMAGE_DMK_HEADER_SIZE];
	int sides, i;
	long track;

	fseek(fp, 0, SEEK_SET);
	if (fread(header, 1, sizeof(header), fp) != sizeof(header))
	{
		return -1;
	}
	sides = (header[4] & DAMAGE_DMK_SINGLE_SIDED) ? 1 : 2;
	track = DAMAGE_DMK_HEADER_SIZE + (long) (cylinder * sides + side) * (header[2] | (header[3] << 8));

	for (i = 0; i < DAMAGE_DMK_POINTERS; i++)
	{
		u_char pointer[2], id[5];
		long offset;

		fseek(fp, track + i * 2, SEEK_SET);
		if (fread(pointer, 1, sizeof(pointer), fp) != sizeof(pointer))
		{
			return -1;
		}
		offset = (pointer[0] | (pointer[1] << 8)) & 0x3FFF;
		if (offset == 0)
		{
			continue;
		}
		fseek(fp, track + offset, SEEK_SET);
		if (fread(id, 1, sizeof(id), fp) != sizeof(id))
		{
			return -1;
		}
		if (id[0] == 0xFE && id[1] == cylinder && id[2] == side && id[3] == sector)
		{
			return track + offset;
		}
	}

	return -1;
}


/* Find a sector's entry in an SDF image's table, or -1 if it has none. */
static inline long damage_sdf_entry(FILE *fp, int cylinder, int side, int sector, long *record)
{
	u_char header[6];
	u_char count;
	int i;

	fseek(fp, 0, SEEK_SET);
	if (fread(header, 1, sizeof(header), fp) != sizeof(header))
	{
		return -1;
	}
	*record = DAMAGE_SDF_HEADER_SIZE + (long) (cylinder * header[5] + side) * DAMAGE_SDF_RECORD_SIZE;

	fseek(fp, *record, SEEK_SET);
	if (fread(&count, 1, 1, fp) != 1)
	{
		return -1;
	}
	for (i = 0; i < count; i++)
	{
		u_char entry[DAMAGE_SDF_ENTRY_SIZE];
		long offset = *record + DAMAGE_SDF_TABLE_OFFSET + i * DAMAGE_SDF_ENTRY_SIZE;

		fseek(fp, offset, SEEK_SET);
		if (fread(entry, 1, sizeof(entry), fp) != sizeof(entry))
		{
			return -1;
		}
		if (entry[4] == cylinder && entry[6] == sector)
		{
			return offset;
		}
	}

	return -1;
}


/* Find a sector's attribute in a JVC image with them. */
static inline long damage_jvc_attribute(FILE *fp, int cylinder, int side, int sector)
{
	u_char header[DAMAGE_JVC_HEADER_SIZE];

	fseek(fp, 0, SEEK_SET);
	if (fread(header, 1, sizeof(header), fp) != sizeof(header))
	{
		return -1;
	}

	return DAMAGE_JVC_HEADER_SIZE +
		((long) (cylinder * header[1] + side) * header[0] + (sector - header[3])) * DAMAGE_JVC_RECORD_SIZE;
}


/* Rewrite a raw image of spt 256 byte sectors per track as a JVC image with
   sector attributes.  It needs a multiple of 256 sectors to be recognized. */
static inline void image_add_jvc_attributes(const char *pathname, int sectors_per_track, int sides)
{
	static u_char sector[256];
	const u_char header[DAMAGE_JVC_HEADER_SIZE] = { (u_char) sectors_per_track, (u_char) sides, 1, 1, 1 };
	char temporary[256];
	FILE *in, *out;

	snprintf(temporary, sizeof(temporary), "%s.tmp", pathname);
	in = fopen(pathname, "rb");
	out = fopen(temporary, "wb");
	ASSERT("the image to convert exists", in != NULL && out != NULL);
	fwrite(header, 1, sizeof(header), out);
	while (fread(sector, 1, sizeof(sector), in) == sizeof(sector))
	{
		fputc(0, out);
		fwrite(sector, 1, sizeof(sector), out);
	}
	fclose(in);
	fclose(out);
	remove(pathname);
	rename(temporary, pathname);
}


/* Make a sector unfindable. */
static inline void image_remove_sector(const char *pathname, int cylinder, int side, int sector)
{
	diskimage_format format = DISKIMAGE_UNKNOWN;
	FILE *fp;
	long offset, record;
	u_char byte;

	_diskimage_identify(pathname, &format);
	fp = fopen(pathname, "r+b");
	ASSERT("the image to damage exists", fp != NULL);

	switch (format)
	{
	case DISKIMAGE_DMK:
		offset = damage_dmk_id(fp, cylinder, side, sector);
		ASSERT("the sector to remove exists", offset >= 0);
		fseek(fp, offset + 1, SEEK_SET);
		fputc(DAMAGE_WRONG_TRACK, fp);
		break;

	case DISKIMAGE_SDF:
		offset = damage_sdf_entry(fp, cylinder, side, sector, &record);
		ASSERT("the sector to remove exists", offset >= 0);
		fseek(fp, offset + 4, SEEK_SET);
		fputc(DAMAGE_WRONG_TRACK, fp);
		break;

	case DISKIMAGE_JVC:
		offset = damage_jvc_attribute(fp, cylinder, side, sector);
		fseek(fp, offset, SEEK_SET);
		byte = (u_char) fgetc(fp);
		fseek(fp, offset, SEEK_SET);
		fputc(byte | DAMAGE_JVC_NOT_FOUND, fp);
		break;

	default:
		ASSERT("this format can't lose a sector", 0);
	}

	fclose(fp);
}


/* Change the first byte of a sector's data, and record a bad CRC for it. */
static inline void image_damage_sector(const char *pathname, int cylinder, int side, int sector)
{
	diskimage_format format = DISKIMAGE_UNKNOWN;
	FILE *fp;
	long offset, record, position;
	u_char byte = 0;
	u_char entry[DAMAGE_SDF_ENTRY_SIZE];

	_diskimage_identify(pathname, &format);
	fp = fopen(pathname, "r+b");
	ASSERT("the image to damage exists", fp != NULL);

	switch (format)
	{
	case DISKIMAGE_DMK:
		/* The CRC is checked on opening, so it goes bad by itself. */
		offset = damage_dmk_id(fp, cylinder, side, sector);
		ASSERT("the sector to damage exists", offset >= 0);
		for (position = offset + 7; position < offset + 80 && byte != 0xFB; position++)
		{
			fseek(fp, position, SEEK_SET);
			byte = (u_char) fgetc(fp);
		}
		ASSERT_EQUALS(0xFB, byte);
		damage_flip_byte(fp, position);
		break;

	case DISKIMAGE_SDF:
		offset = damage_sdf_entry(fp, cylinder, side, sector, &record);
		ASSERT("the sector to damage exists", offset >= 0);
		fseek(fp, offset, SEEK_SET);
		if (fread(entry, 1, sizeof(entry), fp) != sizeof(entry))
		{
			ASSERT("the table entry can be read", 0);
		}
		damage_flip_byte(fp, record + (entry[2] | ((entry[3] & 0x3F) << 8)));
		fseek(fp, offset + 3, SEEK_SET);
		fputc(entry[3] | DAMAGE_SDF_BAD_DATA_CRC, fp);
		break;

	case DISKIMAGE_JVC:
		offset = damage_jvc_attribute(fp, cylinder, side, sector);
		fseek(fp, offset, SEEK_SET);
		byte = (u_char) fgetc(fp);
		fseek(fp, offset, SEEK_SET);
		fputc(byte | DAMAGE_JVC_CRC_ERROR, fp);
		damage_flip_byte(fp, offset + 1);
		break;

	default:
		ASSERT("this format has no CRC to go bad", 0);
	}

	fclose(fp);
}

#endif	/* IMAGEDAMAGE_H */

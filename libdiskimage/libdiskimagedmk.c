/********************************************************************
 * libdiskimagedmk.c - DMK image parsing
 *
 * A DMK file has no signature, so it is recognized by its header
 * fields and by its size matching them exactly.
 ********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libdiskimageint.h"

#define DMK_HEADER_SIZE		16
#define DMK_TRACK_HEADER_SIZE	128
#define DMK_MAX_IDAMS		64
#define DMK_MAX_TRACK_LENGTH	0x2940
#define DMK_CREATE_TRACK_LENGTH	(DMK_TRACK_HEADER_SIZE + DI_MFM_TRACK_SIZE)
#define DMK_FLAG_SINGLE_SIDED	0x10
#define DMK_FLAG_SD_ONE_BYTE	0x40
#define DMK_FLAG_IGNORE_DENSITY	0x80
#define DMK_IDAM_DOUBLE_DENSITY	0x8000
#define DMK_IDAM_OFFSET_MASK	0x3FFF

/* How far the WD179x looks past an ID field for the data address mark. */
#define DMK_DAM_WINDOW_SD	30
#define DMK_DAM_WINDOW_DD	43


int di_dmk_probe(const u_char *header, long long file_size)
{
	u_int tracks = header[1];
	u_int track_length = header[2] | (header[3] << 8);
	u_int sides = (header[4] & DMK_FLAG_SINGLE_SIDED) ? 1 : 2;

	if (header[0] != 0x00 && header[0] != 0xFF)
	{
		return 0;
	}
	if (tracks == 0 || track_length <= DMK_TRACK_HEADER_SIZE || track_length > DMK_MAX_TRACK_LENGTH)
	{
		return 0;
	}

	/* Non-zero here is a specification file for a real floppy. */
	if (header[12] != 0 || header[13] != 0 || header[14] != 0 || header[15] != 0)
	{
		return 0;
	}

	return file_size == DMK_HEADER_SIZE + (long long)tracks * sides * track_length;
}


static int is_data_mark(u_char byte)
{
	return byte >= 0xF8 && byte <= 0xFB;
}


/* Return the position of the data address mark, or -1 if there is none. */
static int find_data_mark(const u_char *track, int length, int start, int step, int double_density)
{
	int i;

	if (double_density)
	{
		for (i = 0; i <= DMK_DAM_WINDOW_DD && start + i + 3 < length; i++)
		{
			const u_char *p = &track[start + i];

			if (p[0] == 0xA1 && p[1] == 0xA1 && p[2] == 0xA1 && is_data_mark(p[3]))
			{
				return start + i + 3;
			}
		}
	}
	else
	{
		for (i = 0; i <= DMK_DAM_WINDOW_SD && start + i * step < length; i++)
		{
			if (is_data_mark(track[start + i * step]))
			{
				return start + i * step;
			}
		}
	}

	return -1;
}


/* Decide if a sector ID is a logical sector, and fill in its details. */
static void parse_sector(di_track_id *entry, const u_char *track, int length, int position,
			 long long file_offset, u_int cylinder, u_int side, int step, int double_density)
{
	di_sector *sector = &entry->sector;
	u_char id[7];
	u_char field[1 + DI_MAX_SECTOR_SIZE];
	unsigned short crc_init = double_density ? DI_CRC_INIT_DD : DI_CRC_INIT_SD;
	unsigned short crc;
	int mark, data, size, i;

	for (i = 0; i < 7; i++)
	{
		id[i] = track[position + i * step];
	}
	crc = di_crc16(crc_init, id, 5);
	if (crc != ((id[5] << 8) | id[6]) || id[1] != cylinder)
	{
		return;
	}

	mark = find_data_mark(track, length, position + 7 * step, step, double_density);
	if (mark < 0)
	{
		return;
	}

	size = 128 << (id[4] & 3);
	data = mark + step;
	if (data + (size + 2) * step > length)
	{
		return;
	}

	field[0] = track[mark];
	for (i = 0; i < size; i++)
	{
		field[1 + i] = track[data + i * step];
	}
	crc = di_crc16(crc_init, field, 1 + size);

	sector->data_off = file_offset + data;
	sector->cylinder = (unsigned short)cylinder;
	sector->side = (u_char)side;
	sector->id = id[3];
	sector->size = (unsigned short)size;
	sector->mark = track[mark];
	sector->step = (u_char)step;
	if (double_density)
	{
		sector->flags |= DI_FLAG_DOUBLE_DENSITY;
	}
	if (track[mark] == 0xF8 || track[mark] == 0xF9)
	{
		sector->flags |= DISKIMAGE_SECTOR_DELETED;
	}
	if (crc != ((track[data + size * step] << 8) | track[data + (size + 1) * step]))
	{
		sector->flags |= DISKIMAGE_SECTOR_CRC_ERROR;
	}
	entry->valid = 1;
}


error_code di_dmk_create(FILE *fd, const di_layout *layout)
{
	u_char header[DMK_HEADER_SIZE] = {0};
	u_char track[DMK_CREATE_TRACK_LENGTH];
	int id_pos[DMK_MAX_IDAMS], data_pos[DMK_MAX_IDAMS];
	u_int cylinder, side;
	error_code ec;

	header[1] = (u_char)layout->cylinders;
	header[2] = DMK_CREATE_TRACK_LENGTH & 0xFF;
	header[3] = DMK_CREATE_TRACK_LENGTH >> 8;
	header[4] = layout->sides == 1 ? DMK_FLAG_SINGLE_SIDED : 0;
	ec = di_file_write(fd, header, sizeof(header));

	for (cylinder = 0; cylinder < layout->cylinders && ec == 0; cylinder++)
	{
		for (side = 0; side < layout->sides && ec == 0; side++)
		{
			int count, i;

			memset(track, 0, DMK_TRACK_HEADER_SIZE);
			count = di_mfm_track(layout, cylinder, side, &track[DMK_TRACK_HEADER_SIZE], id_pos, data_pos);
			for (i = 0; i < count; i++)
			{
				u_int pointer = (u_int)(DMK_TRACK_HEADER_SIZE + id_pos[i]) | DMK_IDAM_DOUBLE_DENSITY;

				track[i * 2] = (u_char)pointer;
				track[i * 2 + 1] = (u_char)(pointer >> 8);
			}
			ec = di_file_write(fd, track, sizeof(track));
		}
	}

	return ec;
}


error_code di_dmk_load(diskimage_id disk, const u_char *header)
{
	di_track_id ids[DMK_MAX_IDAMS];
	int length = header[2] | (header[3] << 8);
	int one_byte_sd = (header[4] & (DMK_FLAG_SD_ONE_BYTE | DMK_FLAG_IGNORE_DENSITY)) != 0;
	long long file_offset = DMK_HEADER_SIZE;
	u_char *track;
	u_int cylinder, side;
	error_code ec = 0;

	disk->cylinders = header[1];
	disk->sides = (header[4] & DMK_FLAG_SINGLE_SIDED) ? 1 : 2;
	disk->write_protected = header[0] == 0xFF;

	track = malloc(length);
	if (track == NULL)
	{
		return UnixToCoCoError(ENOMEM);
	}

	for (cylinder = 0; cylinder < disk->cylinders && ec == 0; cylinder++)
	{
		for (side = 0; side < disk->sides && ec == 0; side++)
		{
			int count = 0;
			int i;

			ec = di_read_at(disk, file_offset, track, length);
			for (i = 0; i < DMK_MAX_IDAMS && ec == 0; i++)
			{
				u_int pointer = track[i * 2] | (track[i * 2 + 1] << 8);
				int position = pointer & DMK_IDAM_OFFSET_MASK;
				int double_density = (pointer & DMK_IDAM_DOUBLE_DENSITY) != 0;
				int step = (double_density || one_byte_sd) ? 1 : 2;

				if (pointer == 0)
				{
					break;
				}
				if (position < DMK_TRACK_HEADER_SIZE || position + 7 * step > length || track[position] != 0xFE)
				{
					continue;
				}

				memset(&ids[count], 0, sizeof(ids[count]));
				ids[count].id_start = file_offset + position;
				ids[count].id_end = ids[count].id_start + 7 * step;
				parse_sector(&ids[count], track, length, position, file_offset,
					     cylinder, side, step, double_density);
				count++;
			}

			if (ec == 0)
			{
				ec = di_table_add_track(disk, ids, count);
			}
			file_offset += length;
		}
	}

	free(track);

	return ec;
}

/********************************************************************
 * libdiskimagesdf.c - SDF image parsing
 *
 * The specification calls the Sector ID Table offsets the start of the
 * ID and Data fields.  As dmk2sdf writes them, the data offset is the
 * first data byte after the address mark, and the ID offset is one byte
 * past the start of the mark: the second copy of a single density mark.
 *
 * dmk2sdf also never sets the specification's single density bit, so
 * density is taken from the track: a single density mark is written
 * twice, and a double density mark follows $A1 sync bytes.
 ********************************************************************/
#include <string.h>

#include "libdiskimageint.h"

#define SDF_HEADER_SIZE		512
#define SDF_TRACK_RECORD_SIZE	6656
#define SDF_TRACK_HEADER_SIZE	256
#define SDF_TRACK_DATA_SIZE	6250
#define SDF_TRACK_DATA_END	(SDF_TRACK_HEADER_SIZE + SDF_TRACK_DATA_SIZE)
#define SDF_TABLE_OFFSET	8
#define SDF_ENTRY_SIZE		8
#define SDF_MAX_ENTRIES		31
#define SDF_MAX_CYLINDERS	80
#define SDF_OFFSET_MASK		0x3FFF
#define SDF_ID_BAD_CRC		0x8000
#define SDF_DATA_DELETED	0x4000
#define SDF_DATA_BAD_CRC	0x8000


int di_sdf_probe(const u_char *header, long long file_size)
{
	if (memcmp(header, "SDF1", 4) != 0)
	{
		return 0;
	}
	if (header[4] == 0 || header[4] > SDF_MAX_CYLINDERS || (header[5] != 1 && header[5] != 2))
	{
		return 0;
	}

	return file_size == SDF_HEADER_SIZE + (long long)header[4] * header[5] * SDF_TRACK_RECORD_SIZE;
}


error_code di_sdf_create(FILE *fd, const di_layout *layout)
{
	u_char header[SDF_HEADER_SIZE] = {'S', 'D', 'F', '1'};
	u_char record[SDF_TRACK_RECORD_SIZE];
	int id_pos[SDF_MAX_ENTRIES], data_pos[SDF_MAX_ENTRIES];
	u_int cylinder, side;
	error_code ec;

	header[4] = (u_char)layout->cylinders;
	header[5] = (u_char)layout->sides;
	ec = di_file_write(fd, header, sizeof(header));

	for (cylinder = 0; cylinder < layout->cylinders && ec == 0; cylinder++)
	{
		for (side = 0; side < layout->sides && ec == 0; side++)
		{
			int count, i;

			memset(record, 0, sizeof(record));
			count = di_mfm_track(layout, cylinder, side, &record[SDF_TRACK_HEADER_SIZE], id_pos, data_pos);
			record[0] = (u_char)count;
			for (i = 0; i < count; i++)
			{
				u_char *entry = &record[SDF_TABLE_OFFSET + i * SDF_ENTRY_SIZE];
				int id = SDF_TRACK_HEADER_SIZE + id_pos[i] + 1;
				int data = SDF_TRACK_HEADER_SIZE + data_pos[i];

				entry[0] = (u_char)id;
				entry[1] = (u_char)(id >> 8);
				entry[2] = (u_char)data;
				entry[3] = (u_char)(data >> 8);
				entry[4] = (u_char)cylinder;
				entry[5] = (u_char)side;
				entry[6] = record[SDF_TRACK_HEADER_SIZE + id_pos[i] + 3];
				entry[7] = (u_char)layout->size_code;
			}
			ec = di_file_write(fd, record, sizeof(record));
		}
	}

	return ec;
}


/* Decide if a table entry is a logical sector, and fill in its details. */
static void parse_entry(di_track_id *entry, const u_char *record, const u_char *table_entry,
			long long file_offset, long long status_off, u_int cylinder, u_int side,
			u_int id_word)
{
	di_sector *sector = &entry->sector;
	u_int data_word = table_entry[2] | (table_entry[3] << 8);
	int data = data_word & SDF_OFFSET_MASK;
	int size = 128 << (table_entry[7] & 3);
	int step;
	u_char mark;

	sector->cylinder = (unsigned short)cylinder;
	sector->side = (u_char)side;
	sector->id = table_entry[6];
	sector->size = (unsigned short)size;
	if (data < SDF_TRACK_HEADER_SIZE + 1)
	{
		return;
	}

	mark = record[data - 1];
	if (mark < 0xF8 || mark > 0xFB)
	{
		return;
	}

	step = (data >= SDF_TRACK_HEADER_SIZE + 2 && record[data - 2] == mark) ? 2 : 1;
	if (data + (size + 2) * step > SDF_TRACK_DATA_END)
	{
		return;
	}

	sector->data_off = file_offset + data;
	sector->status_off = status_off;
	sector->cylinder = (unsigned short)cylinder;
	sector->side = (u_char)side;
	sector->id = table_entry[6];
	sector->size = (unsigned short)size;
	sector->mark = mark;
	sector->step = (u_char)step;
	if (step == 1)
	{
		sector->flags |= DI_FLAG_DOUBLE_DENSITY;
	}
	if (data_word & SDF_DATA_DELETED)
	{
		sector->flags |= DISKIMAGE_SECTOR_DELETED;
	}
	if (data_word & SDF_DATA_BAD_CRC)
	{
		sector->flags |= DISKIMAGE_SECTOR_CRC_ERROR;
	}
	entry->valid = !(id_word & SDF_ID_BAD_CRC) && table_entry[4] == cylinder;
}


error_code di_sdf_load(diskimage_id disk, const u_char *header)
{
	u_char record[SDF_TRACK_RECORD_SIZE];
	di_track_id ids[SDF_MAX_ENTRIES];
	long long file_offset = SDF_HEADER_SIZE;
	u_int cylinder, side;

	disk->cylinders = header[4];
	disk->sides = header[5];
	disk->write_protected = header[6] == 0xFF;
	disk->data_start = SDF_HEADER_SIZE;
	disk->track_size = SDF_TRACK_RECORD_SIZE;
	disk->track_header_size = SDF_TRACK_HEADER_SIZE;

	for (cylinder = 0; cylinder < disk->cylinders; cylinder++)
	{
		for (side = 0; side < disk->sides; side++)
		{
			int entries, i;
			int count = 0;
			error_code ec = di_read_at(disk, file_offset, record, sizeof(record));

			if (ec != 0)
			{
				return ec;
			}

			entries = record[0] < SDF_MAX_ENTRIES ? record[0] : SDF_MAX_ENTRIES;
			for (i = 0; i < entries; i++)
			{
				int table_offset = SDF_TABLE_OFFSET + i * SDF_ENTRY_SIZE;
				const u_char *table_entry = &record[table_offset];
				u_int id_word = table_entry[0] | (table_entry[1] << 8);
				int id = id_word & SDF_OFFSET_MASK;
				int start, step;

				if (id < SDF_TRACK_HEADER_SIZE + 1 || id >= SDF_TRACK_DATA_END)
				{
					continue;
				}

				/* Find the start of the 7 byte ID field from its mark. */
				if (record[id - 1] == 0xFE && record[id] == 0xFE)
				{
					start = id - 1;
					step = 2;
				}
				else if (id >= SDF_TRACK_HEADER_SIZE + 2 && record[id - 2] == 0xFE)
				{
					start = id - 2;
					step = 2;
				}
				else
				{
					start = id - 1;
					step = 1;
				}
				if (start + 7 * step > SDF_TRACK_DATA_END)
				{
					continue;
				}

				memset(&ids[count], 0, sizeof(ids[count]));
				ids[count].id_start = file_offset + start;
				ids[count].id_end = ids[count].id_start + 7 * step;
				parse_entry(&ids[count], record, table_entry, file_offset,
					    file_offset + table_offset + 2, cylinder, side, id_word);
				if (ids[count].sector.step == 0)
					ids[count].sector.step = (u_char)step;
				count++;
			}

			ec = di_table_add_track(disk, ids, count);
			if (ec != 0)
			{
				return ec;
			}
			file_offset += SDF_TRACK_RECORD_SIZE;
		}
	}

	return 0;
}

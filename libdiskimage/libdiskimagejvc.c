/********************************************************************
 * libdiskimagejvc.c - JVC image parsing
 *
 * The header length is the file length modulo 256, so a header is only
 * found reliably when the sector data is a multiple of 256 bytes.  With
 * sector attributes that holds only if the sector count is a multiple
 * of 256, which di_jvc_parse() requires by checking for whole tracks;
 * other attribute images cannot be told apart, as the format's errata
 * notes.
 ********************************************************************/
#include "libdiskimageint.h"

#define JVC_DEFAULT_SIDES		1
#define JVC_DEFAULT_SIZE_CODE		1
#define JVC_DEFAULT_FIRST_SECTOR	1
#define JVC_ATTRIBUTE_CRC_ERROR		0x08
#define JVC_ATTRIBUTE_NOT_FOUND		0x10
#define JVC_ATTRIBUTE_DELETED		0x20


/* Return non-zero if the file has a valid JVC header.  A file with no
   header is a raw image, so that is not accepted here. */
int di_jvc_parse(const u_char *header, long long file_size, di_jvc_header *jvc)
{
	long long data_size, track_size;

	jvc->header_size = (u_int)(file_size % 256);
	if (jvc->header_size == 0)
	{
		return 0;
	}

	jvc->sectors_per_track = header[0];
	jvc->sides = jvc->header_size > 1 ? header[1] : JVC_DEFAULT_SIDES;
	jvc->size_code = jvc->header_size > 2 ? header[2] : JVC_DEFAULT_SIZE_CODE;
	jvc->first_sector = jvc->header_size > 3 ? header[3] : JVC_DEFAULT_FIRST_SECTOR;
	jvc->attribute = jvc->header_size > 4 ? header[4] : 0;

	if (jvc->sectors_per_track == 0 || (jvc->sides != 1 && jvc->sides != 2) || jvc->size_code > 3)
	{
		return 0;
	}
	if (jvc->first_sector + jvc->sectors_per_track - 1 > 255)
	{
		return 0;
	}

	data_size = file_size - jvc->header_size;
	track_size = (long long)jvc->sectors_per_track * ((128 << jvc->size_code) + (jvc->attribute ? 1 : 0)) * jvc->sides;
	if (data_size < track_size || (jvc->attribute && data_size % track_size != 0))
	{
		return 0;
	}
	jvc->cylinders = (u_int)(data_size / track_size);

	return 1;
}


error_code di_jvc_load(diskimage_id disk, const di_jvc_header *jvc)
{
	u_int size = 128 << jvc->size_code;
	long long total = (long long)jvc->cylinders * jvc->sides * jvc->sectors_per_track;
	long long file_offset = jvc->header_size;
	u_int cylinder, side, n;

	disk->cylinders = jvc->cylinders;
	disk->sides = jvc->sides;
	disk->sector_size = size;
	disk->sectors_per_track = jvc->sectors_per_track;
	disk->first_sector = jvc->first_sector;

	if (!jvc->attribute)
	{
		disk->linear = 1;
		disk->data_start = file_offset;
		disk->total_sectors = (u_int)total;
		return 0;
	}

	for (cylinder = 0; cylinder < jvc->cylinders; cylinder++)
	{
		for (side = 0; side < jvc->sides; side++)
		{
			for (n = 0; n < jvc->sectors_per_track; n++)
			{
				di_sector sector = {0};
				u_char attribute;
				error_code ec = di_read_at(disk, file_offset, &attribute, 1);

				if (ec != 0)
				{
					return ec;
				}

				if (!(attribute & JVC_ATTRIBUTE_NOT_FOUND))
				{
					sector.data_off = file_offset + 1;
					sector.status_off = file_offset;
					sector.cylinder = (unsigned short)cylinder;
					sector.side = (u_char)side;
					sector.id = (u_char)(jvc->first_sector + n);
					sector.size = (unsigned short)size;
					sector.step = 1;
					if (attribute & JVC_ATTRIBUTE_DELETED)
					{
						sector.flags |= DISKIMAGE_SECTOR_DELETED;
					}
					if (attribute & JVC_ATTRIBUTE_CRC_ERROR)
					{
						sector.flags |= DISKIMAGE_SECTOR_CRC_ERROR;
					}

					ec = di_table_append(disk, &sector);
					if (ec != 0)
					{
						return ec;
					}
				}
				file_offset += 1 + size;
			}
		}
	}

	return 0;
}

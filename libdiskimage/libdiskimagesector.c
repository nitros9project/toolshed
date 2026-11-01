/********************************************************************
 * libdiskimagesector.c - Read and write sectors
 ********************************************************************/
#include <string.h>

#include "libdiskimageint.h"

#define JVC_ATTRIBUTE_CRC_ERROR	0x08
#define SDF_DATA_BAD_CRC_HIGH	0x80


int di_write_blocked(diskimage_id disk)
{
	return disk->write_protected && !(disk->mode & DISKIMAGE_IGNORE_WRITE_PROTECT);
}


u_int di_index_size(diskimage_id disk, long long index)
{
	return disk->linear ? disk->sector_size : disk->table[index].size;
}


/* Where a logical sector sits in the file, counted in sectors. */
long long di_linear_slot(diskimage_id disk, long long index)
{
	return index < disk->raw_track0 ? index : index + disk->raw_missing;
}


static long long linear_offset(diskimage_id disk, long long index)
{
	return disk->data_start + di_linear_slot(disk, index) * disk->sector_size;
}


error_code di_index_read(diskimage_id disk, long long index, void *buffer)
{
	const di_sector *sector;
	u_char stored[2 * DI_MAX_SECTOR_SIZE];
	u_int i;
	error_code ec;

	if (disk->linear)
	{
		return di_read_at(disk, linear_offset(disk, index), buffer, disk->sector_size);
	}

	sector = &disk->table[index];
	if (sector->step == 1)
	{
		ec = di_read_at(disk, sector->data_off, buffer, sector->size);
	}
	else
	{
		ec = di_read_at(disk, sector->data_off, stored, (size_t)sector->size * sector->step);
		for (i = 0; ec == 0 && i < sector->size; i++)
		{
			((u_char *)buffer)[i] = stored[i * sector->step];
		}
	}
	if (ec != 0)
	{
		return ec;
	}

	return (sector->flags & DISKIMAGE_SECTOR_CRC_ERROR) ? EOS_CRC : 0;
}


/* Fill any sectors between the end of a raw image and index, including
   the unused slots of a short track 0. */
static error_code raw_extend(diskimage_id disk, long long index)
{
	u_char fill[DI_RAW_SECTOR_SIZE];
	long long target = di_linear_slot(disk, index);

	memset(fill, DI_RAW_FILL, sizeof(fill));
	while (disk->raw_slots < target)
	{
		error_code ec = di_write_at(disk, disk->raw_slots * DI_RAW_SECTOR_SIZE, fill, sizeof(fill));

		if (ec != 0)
		{
			return ec;
		}
		disk->raw_slots++;
	}

	return 0;
}


/* Write the data and CRC of a sector that records them on the track. */
static error_code write_track_data(diskimage_id disk, const di_sector *sector, const u_char *data)
{
	u_char field[1 + DI_MAX_SECTOR_SIZE];
	u_char stored[2 * (DI_MAX_SECTOR_SIZE + 2)];
	unsigned short crc;
	u_int total = sector->size + 2;
	u_int i, j;

	field[0] = sector->mark;
	memcpy(&field[1], data, sector->size);
	crc = di_crc16((sector->flags & DI_FLAG_DOUBLE_DENSITY) ? DI_CRC_INIT_DD : DI_CRC_INIT_SD,
		       field, 1 + sector->size);

	for (i = 0; i < total; i++)
	{
		u_char byte = i < sector->size ? data[i] : (u_char)(i == sector->size ? crc >> 8 : crc);

		for (j = 0; j < sector->step; j++)
		{
			stored[i * sector->step + j] = byte;
		}
	}

	return di_write_at(disk, sector->data_off, stored, (size_t)total * sector->step);
}


/* Clear the CRC error recorded for a sector outside its data field. */
static error_code clear_status(diskimage_id disk, di_sector *sector)
{
	long long offset = sector->status_off;
	u_char mask = JVC_ATTRIBUTE_CRC_ERROR;
	u_char byte;
	error_code ec;

	if (disk->format == DISKIMAGE_SDF)
	{
		offset += 1;	/* the flag is in the high byte of a little-endian word */
		mask = SDF_DATA_BAD_CRC_HIGH;
	}

	ec = di_read_at(disk, offset, &byte, 1);
	if (ec == 0)
	{
		byte &= (u_char)~mask;
		ec = di_write_at(disk, offset, &byte, 1);
	}

	return ec;
}


/* Write a whole sector.  In a raw image index may be past the end. */
error_code di_index_write(diskimage_id disk, long long index, const void *buffer)
{
	di_sector *sector;
	error_code ec;

	if (!(disk->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}
	if (di_write_blocked(disk))
	{
		return EOS_WP;
	}

	if (disk->linear)
	{
		if (index >= disk->total_sectors)
		{
			if (disk->format != DISKIMAGE_RAW)
			{
				return EOS_SECT;
			}
			if (index >= DI_RAW_MAX_SECTORS)
			{
				return EOS_DF;
			}
			ec = raw_extend(disk, index);
			if (ec != 0)
			{
				return ec;
			}
		}

		ec = di_write_at(disk, linear_offset(disk, index), buffer, disk->sector_size);
		if (ec == 0 && index >= disk->total_sectors)
		{
			disk->raw_slots = di_linear_slot(disk, index) + 1;
			disk->total_sectors = (u_int)index + 1;
			disk->stream_size = (long long)disk->total_sectors * disk->sector_size;
		}

		return ec;
	}

	sector = &disk->table[index];
	if (sector->flags & DI_FLAG_OVERLAP)
	{
		return EOS_WRITE;
	}

	if (disk->format == DISKIMAGE_JVC)
	{
		ec = di_write_at(disk, sector->data_off, buffer, sector->size);
	}
	else
	{
		ec = write_track_data(disk, sector, buffer);
	}

	if (ec == 0 && (sector->flags & DISKIMAGE_SECTOR_CRC_ERROR))
	{
		if (sector->status_off != 0)
		{
			ec = clear_status(disk, sector);
		}
		if (ec == 0)
		{
			sector->flags &= (u_char)~DISKIMAGE_SECTOR_CRC_ERROR;
		}
	}

	return ec;
}


/* Another open of the same file may have grown it. */
static void raw_refresh(diskimage_id disk)
{
	if (fseeko(disk->fd, 0, SEEK_END) == 0)
	{
		long long slots = (long long)ftello(disk->fd) / DI_RAW_SECTOR_SIZE;

		if (slots > disk->raw_slots)
		{
			di_raw_set_slots(disk, slots);
		}
	}
}


/* Find a sector's logical index.  Sectors a raw image could hold are
   found even if they are past its end. */
static error_code locate(diskimage_id disk, u_int cylinder, u_int side, u_int sector, long long *index)
{
	if (disk->format == DISKIMAGE_RAW)
	{
		if (side != 0 || sector < 1 || sector > DI_RAW_SECTORS_PER_TRACK)
		{
			return EOS_SECT;
		}
		*index = (long long)cylinder * DI_RAW_SECTORS_PER_TRACK + (sector - 1);
		if (disk->raw_missing != 0 && *index >= disk->raw_track0)
		{
			if (*index < DI_RAW_SECTORS_PER_TRACK)
			{
				return EOS_SECT;
			}
			*index -= disk->raw_missing;
		}
		if (*index >= disk->total_sectors)
		{
			raw_refresh(disk);
		}
	}
	else if (disk->linear)
	{
		if (cylinder >= disk->cylinders || side >= disk->sides ||
		    sector < disk->first_sector || sector - disk->first_sector >= disk->sectors_per_track)
		{
			return EOS_SECT;
		}
		*index = ((long long)cylinder * disk->sides + side) * disk->sectors_per_track + (sector - disk->first_sector);
	}
	else
	{
		*index = di_table_find(disk, cylinder, side, sector);
		if (*index < 0)
		{
			return EOS_SECT;
		}
	}

	return 0;
}


error_code _diskimage_read_sector(diskimage_id disk, u_int cylinder, u_int side, u_int sector, void *buffer, u_int *size)
{
	long long index;
	u_int sector_size;
	error_code ec;

	if (disk == NULL || buffer == NULL || size == NULL)
	{
		return EOS_IA;
	}
	if (!(disk->mode & FAM_READ))
	{
		return EOS_BMODE;
	}

	ec = locate(disk, cylinder, side, sector, &index);
	if (ec != 0)
	{
		return ec;
	}
	if (index >= disk->total_sectors)
	{
		return EOS_SECT;
	}

	sector_size = di_index_size(disk, index);
	if (*size < sector_size)
	{
		*size = sector_size;
		return EOS_IA;
	}
	*size = sector_size;

	return di_index_read(disk, index, buffer);
}


error_code _diskimage_write_sector(diskimage_id disk, u_int cylinder, u_int side, u_int sector, const void *buffer, u_int size)
{
	long long index;
	error_code ec;

	if (disk == NULL || buffer == NULL)
	{
		return EOS_IA;
	}

	ec = locate(disk, cylinder, side, sector, &index);
	if (ec != 0)
	{
		return ec;
	}
	if (size != di_index_size(disk, index))
	{
		return EOS_IA;
	}

	return di_index_write(disk, index, buffer);
}

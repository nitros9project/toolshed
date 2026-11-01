/********************************************************************
 * libdiskimagetable.c - File access, CRC and the logical sector table
 ********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libdiskimageint.h"


unsigned short di_crc16(unsigned short crc, const u_char *data, size_t size)
{
	size_t i;
	int bit;

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


error_code di_read_at(diskimage_id disk, long long offset, void *buffer, size_t size)
{
	if (fseeko(disk->fd, (off_t)offset, SEEK_SET) != 0 ||
	    fread(buffer, 1, size, disk->fd) != size)
	{
		return EOS_READ;
	}

	return 0;
}


error_code di_write_at(diskimage_id disk, long long offset, const void *buffer, size_t size)
{
	if (fseeko(disk->fd, (off_t)offset, SEEK_SET) != 0 ||
	    fwrite(buffer, 1, size, disk->fd) != size ||
	    fflush(disk->fd) != 0)
	{
		return EOS_WRITE;
	}

	return 0;
}


error_code di_table_append(diskimage_id disk, const di_sector *sector)
{
	di_sector *entry;

	if (disk->total_sectors == disk->table_capacity)
	{
		u_int capacity = disk->table_capacity ? disk->table_capacity * 2 : 256;
		di_sector *table = realloc(disk->table, capacity * sizeof(di_sector));

		if (table == NULL)
		{
			return UnixToCoCoError(ENOMEM);
		}
		disk->table = table;
		disk->table_capacity = capacity;
	}

	entry = &disk->table[disk->total_sectors++];
	*entry = *sector;
	entry->stream_off = disk->stream_size;
	disk->stream_size += sector->size;

	return 0;
}


/* Add one track's sector IDs, given in the order they are recorded. */
error_code di_table_add_track(diskimage_id disk, di_track_id *ids, int count)
{
	di_track_id *ordered[DI_MAX_TRACK_IDS];
	int ordered_count = 0;
	int i, j;

	for (i = 0; i < count; i++)
	{
		di_sector *sector = &ids[i].sector;
		long long data_end;

		if (!ids[i].valid)
		{
			continue;
		}

		/* A write here would destroy a nested sector's ID. */
		data_end = sector->data_off + (long long)(sector->size + 2) * sector->step;
		for (j = 0; j < count; j++)
		{
			if (j != i && ids[j].id_start < data_end && ids[j].id_end > sector->data_off)
			{
				sector->flags |= DI_FLAG_OVERLAP;
			}
		}

		/* Stable insertion by number; the first of a duplicate number wins. */
		for (j = ordered_count; j > 0 && ordered[j - 1]->sector.id > sector->id; j--)
		{
		}
		if (j > 0 && ordered[j - 1]->sector.id == sector->id)
		{
			continue;
		}
		memmove(&ordered[j + 1], &ordered[j], (ordered_count - j) * sizeof(ordered[0]));
		ordered[j] = &ids[i];
		ordered_count++;
	}

	for (i = 0; i < ordered_count; i++)
	{
		error_code ec = di_table_append(disk, &ordered[i]->sector);

		if (ec != 0)
		{
			return ec;
		}
	}

	return 0;
}


/* Return the sector's logical index, or -1 if it is not in the table. */
long long di_table_find(diskimage_id disk, u_int cylinder, u_int side, u_int id)
{
	long long low = 0;
	long long high = (long long)disk->total_sectors - 1;

	while (low <= high)
	{
		long long middle = low + (high - low) / 2;
		const di_sector *entry = &disk->table[middle];
		int order;

		if (entry->cylinder != cylinder)
		{
			order = entry->cylinder < cylinder ? -1 : 1;
		}
		else if (entry->side != side)
		{
			order = entry->side < side ? -1 : 1;
		}
		else if (entry->id != id)
		{
			order = entry->id < id ? -1 : 1;
		}
		else
		{
			return middle;
		}

		if (order < 0)
		{
			low = middle + 1;
		}
		else
		{
			high = middle - 1;
		}
	}

	return -1;
}

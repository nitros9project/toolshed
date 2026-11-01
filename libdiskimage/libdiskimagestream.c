/********************************************************************
 * libdiskimagestream.c - Treat logical sectors as one run of bytes
 ********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libdiskimageint.h"


/* Find the sector holding a stream position.  Past the end of the image
   the index is at least total_sectors. */
static void find_position(diskimage_id disk, long long position, long long *index, u_int *offset)
{
	if (disk->linear)
	{
		*index = position / disk->sector_size;
		*offset = (u_int)(position % disk->sector_size);
	}
	else if (position >= disk->stream_size)
	{
		*index = disk->total_sectors;
		*offset = 0;
	}
	else
	{
		long long low = 0;
		long long high = (long long)disk->total_sectors - 1;

		while (low < high)
		{
			long long middle = low + (high - low + 1) / 2;

			if (disk->table[middle].stream_off <= position)
			{
				low = middle;
			}
			else
			{
				high = middle - 1;
			}
		}
		*index = low;
		*offset = (u_int)(position - disk->table[low].stream_off);
	}
}


error_code _diskimage_stream_open(diskimage_stream_id *stream, diskimage_id disk)
{
	diskimage_stream_id new_stream;

	if (stream == NULL || disk == NULL)
	{
		return EOS_IA;
	}

	new_stream = calloc(1, sizeof(*new_stream));
	if (new_stream == NULL)
	{
		return UnixToCoCoError(ENOMEM);
	}

	new_stream->disk = disk;
	new_stream->next = disk->streams;
	disk->streams = new_stream;
	*stream = new_stream;

	return 0;
}


error_code _diskimage_stream_close(diskimage_stream_id stream)
{
	diskimage_stream_id *link;

	if (stream == NULL)
	{
		return EOS_IA;
	}

	for (link = &stream->disk->streams; *link != NULL; link = &(*link)->next)
	{
		if (*link == stream)
		{
			*link = stream->next;
			break;
		}
	}
	free(stream);

	return 0;
}


error_code _diskimage_stream_read(diskimage_stream_id stream, void *buffer, u_int *size)
{
	diskimage_id disk;
	u_char sector[DI_MAX_SECTOR_SIZE];
	u_int requested, done = 0;
	error_code ec = 0;

	if (stream == NULL || buffer == NULL || size == NULL)
	{
		return EOS_IA;
	}
	disk = stream->disk;
	requested = *size;
	*size = 0;
	if (!(disk->mode & FAM_READ))
	{
		return EOS_BMODE;
	}

	while (done < requested)
	{
		long long index;
		u_int offset, count;

		find_position(disk, stream->position, &index, &offset);
		if (index >= disk->total_sectors)
		{
			if (done == 0)
			{
				ec = EOS_EOF;
			}
			break;
		}

		ec = di_index_read(disk, index, sector);
		if (ec != 0)
		{
			break;
		}

		count = di_index_size(disk, index) - offset;
		if (count > requested - done)
		{
			count = requested - done;
		}
		memcpy((u_char *)buffer + done, sector + offset, count);
		done += count;
		stream->position += count;
	}

	*size = done;

	return ec;
}


error_code _diskimage_stream_write(diskimage_stream_id stream, const void *buffer, u_int *size)
{
	diskimage_id disk;
	u_char sector[DI_MAX_SECTOR_SIZE];
	u_int requested, done = 0;
	error_code ec = 0;

	if (stream == NULL || buffer == NULL || size == NULL)
	{
		return EOS_IA;
	}
	disk = stream->disk;
	requested = *size;
	*size = 0;
	if (!(disk->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}
	if (di_write_blocked(disk))
	{
		return EOS_WP;
	}

	while (done < requested)
	{
		long long index;
		u_int offset, sector_size, count;

		find_position(disk, stream->position, &index, &offset);
		if (index >= disk->total_sectors && disk->format != DISKIMAGE_RAW)
		{
			ec = EOS_DF;
			break;
		}

		sector_size = di_index_size(disk, index);
		count = sector_size - offset;
		if (count > requested - done)
		{
			count = requested - done;
		}

		/* Keep the rest of a sector only partly written. */
		if (count < sector_size)
		{
			if (index >= disk->total_sectors)
			{
				memset(sector, DI_RAW_FILL, sector_size);
			}
			else
			{
				ec = di_index_read(disk, index, sector);
				if (ec != 0 && ec != EOS_CRC)
				{
					break;
				}
			}
		}
		memcpy(sector + offset, (const u_char *)buffer + done, count);

		ec = di_index_write(disk, index, sector);
		if (ec != 0)
		{
			break;
		}
		done += count;
		stream->position += count;
	}

	*size = done;

	return ec;
}


error_code _diskimage_stream_seek(diskimage_stream_id stream, long long offset, int mode)
{
	long long base;

	if (stream == NULL)
	{
		return EOS_IA;
	}

	switch (mode)
	{
	case SEEK_SET:
		base = 0;
		break;
	case SEEK_CUR:
		base = stream->position;
		break;
	case SEEK_END:
		base = stream->disk->stream_size;
		break;
	default:
		return EOS_IA;
	}

	if (base + offset < 0)
	{
		return EOS_SE;
	}
	stream->position = base + offset;

	return 0;
}


error_code _diskimage_stream_tell(diskimage_stream_id stream, long long *offset)
{
	if (stream == NULL || offset == NULL)
	{
		return EOS_IA;
	}
	*offset = stream->position;

	return 0;
}

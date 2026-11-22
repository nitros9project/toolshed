/********************************************************************
 * libflexread.c - Reading files and getting status
 ********************************************************************/
#include <stdio.h>
#include <string.h>

#include "libflexint.h"


error_code _fx_set_data_start(flex_path_id path)
{
	u_int sectors = _flex_entry_sectors(&path->entry);

	path->data_track = path->entry.start_track;
	path->data_sector = path->entry.start_sector;
	path->data_sectors = sectors;
	path->buffer_valid = 0;
	path->buffer_dirty = 0;

	if (sectors == 0)
	{
		path->data_track = 0;
		path->data_sector = 0;
		return 0;
	}
	if (!_fx_valid_address(path, path->data_track, path->data_sector))
	{
		return EOS_SECT;
	}

	if (path->entry.random)
	{
		if (sectors <= FLEX_RANDOM_MAP_SECTORS)
		{
			path->data_sectors = 0;
			return 0;
		}
		path->data_sectors = sectors - FLEX_RANDOM_MAP_SECTORS;
		return _fx_follow(path, path->entry.start_track, path->entry.start_sector, FLEX_RANDOM_MAP_SECTORS,
				  &path->data_track, &path->data_sector);
	}
	return 0;
}

/* Put a data sector of the file in the buffer, following the chain from the
   sector already there if that is on the way. */
error_code _fx_load_data(flex_path_id path, u_int index)
{
	u_int track, sector, steps;
	error_code ec;

	if (path->buffer_valid && path->buffer_index == index)
	{
		return 0;
	}
	if (index >= path->data_sectors)
	{
		return EOS_EOF;
	}

	ec = _fx_flush_buffer(path);
	if (ec != 0)
	{
		return ec;
	}

	if (path->buffer_valid && path->buffer_index < index)
	{
		track = path->buffer[0];
		sector = path->buffer[1];
		if (!_fx_valid_address(path, track, sector))
		{
			return EOS_SECT;
		}
		steps = index - path->buffer_index - 1;
	}
	else
	{
		track = path->data_track;
		sector = path->data_sector;
		steps = index;
	}

	ec = _fx_follow(path, track, sector, steps, &track, &sector);
	if (ec == 0)
	{
		ec = _fx_read_sector(path, track, sector, path->buffer);
	}
	if (ec != 0 && ec != EOS_CRC)
	{
		path->buffer_valid = 0;
		return ec;
	}

	path->buffer_valid = 1;
	path->buffer_index = index;
	path->buffer_track = track;
	path->buffer_sector = sector;
	return ec;
}

static u_int file_size(flex_path_id path)
{
	return path->data_sectors * FLEX_DATA_SIZE;
}

static error_code transfer_in(flex_path_id path, u_char *buffer, u_int *size, int line)
{
	u_int wanted = *size;
	u_int done = 0;
	error_code ec = 0;

	*size = 0;
	if (path->israw)
	{
		return EOS_BMODE;
	}
	if (!(path->mode & FAM_READ))
	{
		return EOS_BMODE;
	}
	if (path->filepos >= file_size(path))
	{
		return EOS_EOF;
	}

	while (done < wanted && path->filepos < file_size(path))
	{
		u_int index = path->filepos / FLEX_DATA_SIZE;
		u_int offset = path->filepos % FLEX_DATA_SIZE;
		u_int count = FLEX_DATA_SIZE - offset;
		u_char *data;

		ec = _fx_load_data(path, index);
		if (ec != 0)
		{
			break;
		}
		if (count > wanted - done)
		{
			count = wanted - done;
		}
		data = &path->buffer[FLEX_DATA_OFFSET + offset];
		if (line)
		{
			u_char *cr = memchr(data, '\r', count);

			if (cr != NULL)
			{
				count = (u_int)(cr - data) + 1;
				memcpy(&buffer[done], data, count);
				done += count;
				path->filepos += count;
				break;
			}
		}
		memcpy(&buffer[done], data, count);
		done += count;
		path->filepos += count;
	}

	*size = done;
	return ec;
}

error_code _flex_read(flex_path_id path, void *buffer, u_int *size)
{
	return transfer_in(path, buffer, size, 0);
}

error_code _flex_readln(flex_path_id path, void *buffer, u_int *size)
{
	return transfer_in(path, buffer, size, 1);
}

error_code _flex_seek(flex_path_id path, long offset, int mode)
{
	long base;

	switch (mode)
	{
	case SEEK_SET:
		base = 0;
		break;
	case SEEK_CUR:
		base = (long)path->filepos;
		break;
	case SEEK_END:
		base = (long)file_size(path);
		break;
	default:
		return EOS_IA;
	}

	if (base + offset < 0)
	{
		return EOS_SE;
	}
	path->filepos = (u_int)(base + offset);
	return 0;
}

error_code _flex_gs_size(flex_path_id path, u_int *size)
{
	*size = file_size(path);
	return 0;
}

error_code _flex_gs_pos(flex_path_id path, u_int *pos)
{
	*pos = path->filepos;
	return 0;
}

error_code _flex_gs_eof(flex_path_id path)
{
	return path->filepos >= file_size(path) ? EOS_EOF : 0;
}

error_code _flex_gs_fd(flex_path_id path, flex_file_stat *stat)
{
	if (path->israw)
	{
		return EOS_BMODE;
	}

	stat->attributes = path->entry.attributes;
	stat->random = path->entry.random != 0;
	stat->sectors = _flex_entry_sectors(&path->entry);
	stat->file_size = file_size(path);
	stat->month = path->entry.month;
	stat->day = path->entry.day;
	stat->year = path->entry.year;
	stat->start_track = path->entry.start_track;
	stat->start_sector = path->entry.start_sector;
	stat->end_track = path->entry.end_track;
	stat->end_sector = path->entry.end_sector;
	return 0;
}

error_code _flex_gs_sir(flex_path_id path, flex_sir *sir)
{
	_fx_decode_sir(path->sir, sir);
	return 0;
}

error_code _flex_gs_free(flex_path_id path, u_int *sectors)
{
	*sectors = _fx_get2(&path->sir[FX_SIR_FREE_COUNT]);
	return 0;
}

error_code _flex_gs_sector(flex_path_id path, u_int track, u_int sector, void *buffer)
{
	return _fx_read_sector(path, track, sector, buffer);
}

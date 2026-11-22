/********************************************************************
 * libflexwrite.c - Writing files and setting status
 *
 * New sectors of a file come from the start of the free chain, one at a
 * time, and are numbered with their record number, from 1, as FLEX numbers
 * those of a sequential file.  Sectors cut from a file go to the end of
 * the free chain.
 ********************************************************************/
#include <string.h>

#include "libflexint.h"


error_code _fx_flush_buffer(flex_path_id path)
{
	error_code ec;

	if (!path->buffer_valid || !path->buffer_dirty)
	{
		return 0;
	}
	ec = _fx_write_sector(path, path->buffer_track, path->buffer_sector, path->buffer);
	if (ec == 0)
	{
		path->buffer_dirty = 0;
	}
	return ec;
}

static error_code writable(flex_path_id path)
{
	if (path->israw || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}
	if (path->entry.attributes & FLEX_ATTR_WRITE)
	{
		return EOS_FNA;
	}
	return 0;
}

/* Point the file's last sector at a new one. */
static error_code link_last(flex_path_id path, u_int track, u_int sector)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	error_code ec;

	if (path->buffer_valid && path->buffer_index == path->data_sectors - 1)
	{
		path->buffer[0] = (u_char)track;
		path->buffer[1] = (u_char)sector;
		path->buffer_dirty = 1;
		return 0;
	}

	ec = _fx_read_sector(path, path->entry.end_track, path->entry.end_sector, buffer);
	if (ec == 0)
	{
		buffer[0] = (u_char)track;
		buffer[1] = (u_char)sector;
		ec = _fx_write_sector(path, path->entry.end_track, path->entry.end_sector, buffer);
	}
	return ec;
}

error_code _fx_extend(flex_path_id path, u_int data_sectors)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	error_code ec = 0;

	if (data_sectors > path->data_sectors && path->entry.random)
	{
		return EOS_FNA;
	}

	while (path->data_sectors < data_sectors)
	{
		u_int track, sector;
		u_int sectors = _flex_entry_sectors(&path->entry);

		ec = _fx_allocate(path, &track, &sector);
		if (ec != 0)
		{
			break;
		}

		memset(buffer, 0, sizeof(buffer));
		_fx_put2(&buffer[2], path->data_sectors + 1);
		ec = _fx_write_sector(path, track, sector, buffer);
		if (ec == 0 && path->data_sectors > 0)
		{
			ec = link_last(path, track, sector);
		}
		if (ec != 0)
		{
			break;
		}

		if (path->data_sectors == 0)
		{
			path->entry.start_track = (u_char)track;
			path->entry.start_sector = (u_char)sector;
			path->data_track = track;
			path->data_sector = sector;
		}
		path->entry.end_track = (u_char)track;
		path->entry.end_sector = (u_char)sector;
		_fx_put2(path->entry.sectors, sectors + 1);
		path->entry_dirty = 1;
		path->data_sectors++;
	}

	return ec;
}

error_code _fx_truncate(flex_path_id path, u_int data_sectors)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int last_track = 0, last_sector = 0, cut_track, cut_sector;
	u_int cut = path->data_sectors - data_sectors;
	error_code ec;

	if (data_sectors >= path->data_sectors)
	{
		return 0;
	}
	if (path->entry.random)
	{
		return EOS_FNA;
	}

	ec = _fx_flush_buffer(path);
	if (ec != 0)
	{
		return ec;
	}
	path->buffer_valid = 0;

	if (data_sectors == 0)
	{
		ec = _fx_release(path, path->entry.start_track, path->entry.start_sector,
				 path->entry.end_track, path->entry.end_sector, _flex_entry_sectors(&path->entry));
		if (ec != 0)
		{
			return ec;
		}
		path->entry.start_track = path->entry.start_sector = 0;
		path->entry.end_track = path->entry.end_sector = 0;
		_fx_put2(path->entry.sectors, 0);
		path->entry_dirty = 1;
		path->data_track = path->data_sector = 0;
		path->data_sectors = 0;
		return 0;
	}

	ec = _fx_follow(path, path->data_track, path->data_sector, data_sectors - 1, &last_track, &last_sector);
	if (ec == 0)
	{
		ec = _fx_read_sector(path, last_track, last_sector, buffer);
	}
	if (ec != 0)
	{
		return ec;
	}
	cut_track = buffer[0];
	cut_sector = buffer[1];
	if (!_fx_valid_address(path, cut_track, cut_sector))
	{
		return EOS_SECT;
	}

	buffer[0] = 0;
	buffer[1] = 0;
	ec = _fx_write_sector(path, last_track, last_sector, buffer);
	if (ec == 0)
	{
		ec = _fx_release(path, cut_track, cut_sector, path->entry.end_track, path->entry.end_sector, cut);
	}
	if (ec != 0)
	{
		return ec;
	}

	path->entry.end_track = (u_char)last_track;
	path->entry.end_sector = (u_char)last_sector;
	_fx_put2(path->entry.sectors, _flex_entry_sectors(&path->entry) - cut);
	path->entry_dirty = 1;
	path->data_sectors = data_sectors;
	return 0;
}

error_code _flex_write(flex_path_id path, const void *buffer, u_int *size)
{
	const u_char *source = buffer;
	u_int wanted = *size;
	u_int done = 0;
	u_int needed;
	error_code ec = writable(path);

	*size = 0;
	if (ec != 0)
	{
		return ec;
	}
	if (wanted == 0)
	{
		return 0;
	}

	needed = (path->filepos + wanted + FLEX_DATA_SIZE - 1) / FLEX_DATA_SIZE;
	if (needed > path->data_sectors)
	{
		ec = _fx_extend(path, needed);
	}

	while (done < wanted && path->filepos < path->data_sectors * FLEX_DATA_SIZE)
	{
		u_int index = path->filepos / FLEX_DATA_SIZE;
		u_int offset = path->filepos % FLEX_DATA_SIZE;
		u_int count = FLEX_DATA_SIZE - offset;
		error_code load_ec = _fx_load_data(path, index);

		if (load_ec != 0 && load_ec != EOS_CRC)
		{
			ec = load_ec;
			break;
		}
		if (count > wanted - done)
		{
			count = wanted - done;
		}
		memcpy(&path->buffer[FLEX_DATA_OFFSET + offset], &source[done], count);
		path->buffer_dirty = 1;
		done += count;
		path->filepos += count;
	}

	*size = done;
	return ec;
}

error_code _flex_ss_size(flex_path_id path, u_int size)
{
	u_int sectors = (size + FLEX_DATA_SIZE - 1) / FLEX_DATA_SIZE;
	error_code ec = writable(path);

	if (ec != 0)
	{
		return ec;
	}
	if (sectors > path->data_sectors)
	{
		return _fx_extend(path, sectors);
	}
	return _fx_truncate(path, sectors);
}

error_code _flex_ss_fd(flex_path_id path, const flex_file_stat *stat)
{
	error_code ec;

	if (path->israw || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}

	/* Bits FLEX doesn't define are kept as they were. */
	path->entry.attributes = (u_char)((path->entry.attributes & ~FX_ATTR_DEFINED) |
					  (stat->attributes & FX_ATTR_DEFINED));
	path->entry.month = (u_char)stat->month;
	path->entry.day = (u_char)stat->day;
	path->entry.year = (u_char)stat->year;
	ec = _fx_write_entry(path, path->entry_track, path->entry_sector, path->entry_slot, &path->entry);
	if (ec == 0)
	{
		path->entry_dirty = 0;
	}
	return ec;
}

error_code _flex_ss_sector(flex_path_id path, u_int track, u_int sector, const void *buffer)
{
	if (!path->israw || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}
	return _fx_write_sector(path, track, sector, buffer);
}

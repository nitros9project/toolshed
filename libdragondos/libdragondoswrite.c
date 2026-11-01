/********************************************************************
 * libdragondoswrite.c - Write and resize files
 *
 * _dd_extend follows the routine at $CBEC of the Dragon DOS 1.0 ROM, and gives
 * the same disk given the same writes, apart from these bugs of the ROM, which
 * are not reproduced:
 *   - A whole sector write ($C9F8) leaves any dirty buffer of that sector to
 *     overwrite it later, undoing the write.
 *   - The search for a free entry ($D0FC) can evict the buffer holding the
 *     file's last entry, which then doesn't get the link to its continuation.
 *   - Freeing sectors above LSN 1440 ($D026) changes the sector before the
 *     directory rather than the bitmap's second sector.
 *   - Freeing a run that ends at LSN 2880 ($D02F) is an error.
 *   - On a double sided disk, the bitmap's first read leaves it cached as LSN
 *     360 ($D191), and a write to track 20 side 1 overruns the table at $06AA
 *     into the file control blocks ($D2EB).
 ********************************************************************/
#include <string.h>

#include "libdragondosint.h"


error_code _dd_writable_file(dragondos_path_id path)
{
	dragondos_dir_entry *entry;
	error_code ec;

	if (path->israw || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}

	ec = _dd_entry(path, path->entry_index, &entry);
	if (ec != 0)
	{
		return ec;
	}
	return (entry->flags & DRAGONDOS_FLAG_PROTECTED) ? EOS_FNA : 0;
}

/* Chain a new continuation entry onto a file's last entry. */
static error_code add_continuation(dragondos_path_id path, u_int *entry_index, dragondos_dir_entry **last)
{
	dragondos_dir_entry *added;
	u_int index;
	error_code ec = _dd_find_free_entry(path, 1, &index);

	if (ec == 0)
	{
		ec = _dd_entry(path, index, &added);
	}
	if (ec != 0)
	{
		return ec;
	}

	(*last)->flags |= DRAGONDOS_FLAG_CONTINUED;
	(*last)->last_or_next = (u_char)index;
	_dd_entry_dirty(path, *entry_index);

	memset(added, 0, sizeof(*added));
	added->flags = DRAGONDOS_FLAG_CONTINUATION;
	_dd_entry_dirty(path, index);

	*entry_index = index;
	*last = added;
	return 0;
}

/* Find a file's last entry, and its last extent in use, if it has one. */
static error_code find_last_extent(dragondos_path_id path, u_int *entry_index, dragondos_dir_entry **last, int *extent_index)
{
	u_int index = path->entry_index;
	int guard;

	for (guard = 0; guard < DRAGONDOS_MAX_DIR_ENTRIES; guard++)
	{
		dragondos_dir_entry *entry;
		dragondos_extent *extent;
		int count, more, i;
		u_int next;
		error_code ec = _dd_entry(path, index, &entry);

		if (ec == 0)
		{
			ec = _dd_next_entry(entry, &next, &more);
		}
		if (ec != 0)
		{
			return ec;
		}
		if (more)
		{
			index = next;
			continue;
		}

		_dd_extents(entry, &extent, &count);
		for (i = 0; i < count && extent[i].count != 0; i++)
		{
		}
		*entry_index = index;
		*last = entry;
		*extent_index = i - 1;
		return 0;
	}
	return EOS_WT;
}

static error_code grow(dragondos_path_id path, u_int size)
{
	u_int last_bytes = path->filesize % DRAGONDOS_SECTOR_SIZE;
	u_int span = (last_bytes ? last_bytes : DRAGONDOS_SECTOR_SIZE) + (size - path->filesize);
	u_int needed = (span - 1) / DRAGONDOS_SECTOR_SIZE;
	u_int entry_index, hint;
	dragondos_dir_entry *last;
	dragondos_extent *extent;
	int extent_index, extent_count;
	int new_extent;
	error_code ec;

	ec = find_last_extent(path, &entry_index, &last, &extent_index);
	if (ec != 0)
	{
		return ec;
	}
	_dd_extents(last, &extent, &extent_count);

	if (needed != 0)
	{
		if (extent_index < 0)
		{
			hint = DD_NO_HINT;
		}
		else
		{
			hint = int2(extent[extent_index].lsn) + extent[extent_index].count;
		}

		/* Spare sectors that don't follow the file are only used when
		   nothing that does can be allocated. */
		new_extent = extent_index < 0 && path->spare_count != 0;
		if (extent_index >= 0 && path->spare_count != 0 && path->spare_lsn != hint)
		{
			u_int lsn, count;

			ec = _dd_allocate(path, hint, &lsn, &count);
			if (ec == 0)
			{
				ec = _dd_release(path, path->spare_lsn, path->spare_count);
				if (ec != 0)
				{
					return ec;
				}
				path->spare_lsn = lsn;
				path->spare_count = count;
				new_extent = lsn != hint;
			}
			else if (ec == EOS_DF)
			{
				new_extent = 1;
			}
			else
			{
				return ec;
			}
		}
		else if (path->spare_count == 0)
		{
			goto allocate;
		}

		for (;;)
		{
			u_int used;

			if (new_extent)
			{
				if (extent_index + 1 >= extent_count)
				{
					ec = add_continuation(path, &entry_index, &last);
					if (ec != 0)
					{
						return ec;
					}
					_dd_extents(last, &extent, &extent_count);
					extent_index = -1;
				}
				extent_index++;
				_int2(path->spare_lsn, extent[extent_index].lsn);
				extent[extent_index].count = 0;
				new_extent = 0;
			}

			/* A run that would overflow the extent goes in a new one. */
			used = needed < path->spare_count ? needed : path->spare_count;
			if (extent[extent_index].count + used > DD_MAX_EXTENT)
			{
				new_extent = 1;
				continue;
			}

			extent[extent_index].count += (u_char)used;
			_dd_entry_dirty(path, entry_index);
			path->spare_count -= used;
			path->spare_lsn += used;
			needed -= used;
			if (needed == 0)
			{
				break;
			}

allocate:
			ec = _dd_allocate(path, hint, &path->spare_lsn, &path->spare_count);
			if (ec != 0)
			{
				return ec;
			}
			new_extent = extent_index < 0 || path->spare_lsn != hint;
		}
	}

	last->last_or_next = (u_char)size;
	_dd_entry_dirty(path, entry_index);
	path->filesize = size;
	return 0;
}

/* Grow a file.  It is left as it was if that can't be done. */
error_code _dd_extend(dragondos_path_id path, u_int size)
{
	error_code ec = grow(path, size);

	if (ec != 0)
	{
		_dd_truncate(path, path->filesize);
		_dd_flush(path);
	}
	return ec;
}

/* Cut a file to a size no larger than its sectors can hold. */
error_code _dd_truncate(dragondos_path_id path, u_int size)
{
	u_int keep = (size + DRAGONDOS_SECTOR_SIZE - 1) / DRAGONDOS_SECTOR_SIZE;
	u_int index = path->entry_index;
	u_int running = 0;
	int ended = 0;
	int guard;

	for (guard = 0; guard < DRAGONDOS_MAX_DIR_ENTRIES; guard++)
	{
		dragondos_dir_entry *entry;
		dragondos_extent *extent;
		int count, i, more;
		u_int next;
		int was_ended = ended;
		error_code ec = _dd_entry(path, index, &entry);

		if (ec == 0)
		{
			ec = _dd_next_entry(entry, &next, &more);
		}
		if (ec != 0)
		{
			return ec;
		}

		_dd_extents(entry, &extent, &count);
		for (i = 0; i < count; i++)
		{
			u_int sectors = extent[i].count;
			u_int kept = 0;

			if (running < keep)
			{
				kept = keep - running;
				if (kept > sectors)
				{
					kept = sectors;
				}
			}
			if (kept < sectors)
			{
				ec = _dd_release(path, int2(extent[i].lsn) + kept, sectors - kept);
				if (ec != 0)
				{
					return ec;
				}
				extent[i].count = (u_char)kept;
				if (kept == 0)
				{
					_int2(0, extent[i].lsn);
				}
			}
			running += kept;
		}

		if (was_ended)
		{
			entry->flags = DD_KILLED_FLAGS;
			entry->last_or_next = 0;
		}
		else if (running >= keep || !more)
		{
			entry->flags &= (u_char)~DRAGONDOS_FLAG_CONTINUED;
			entry->last_or_next = (u_char)size;
			ended = 1;
		}
		_dd_entry_dirty(path, index);

		if (!more)
		{
			path->filesize = size;
			return 0;
		}
		index = next;
	}
	return EOS_WT;
}

error_code _dragondos_write(dragondos_path_id path, const void *buffer, u_int *size)
{
	const u_char *in = buffer;
	u_int wanted = *size;
	u_int done = 0;
	u_int old_size = path->filesize;
	u_int start = path->filepos;
	error_code ec;

	*size = 0;
	ec = _dd_writable_file(path);
	if (ec != 0)
	{
		return ec;
	}
	if (wanted == 0)
	{
		return 0;
	}

	if (start + wanted > path->filesize)
	{
		ec = _dd_extend(path, start + wanted);
		if (ec != 0)
		{
			return ec;
		}
	}

	while (done < wanted)
	{
		u_char sector[DRAGONDOS_SECTOR_SIZE];
		u_int position = start + done;
		u_int offset = position % DRAGONDOS_SECTOR_SIZE;
		u_int chunk = DRAGONDOS_SECTOR_SIZE - offset;
		u_int lsn;

		if (chunk > wanted - done)
		{
			chunk = wanted - done;
		}

		ec = _dd_sector_lsn(path, position / DRAGONDOS_SECTOR_SIZE, &lsn);
		if (ec == 0 && chunk != DRAGONDOS_SECTOR_SIZE)
		{
			/* A bad CRC only matters if bytes of the file would survive. */
			ec = _dd_read_lsn(path, lsn, sector);
			if (ec == EOS_CRC && position - offset >= old_size)
			{
				ec = 0;
			}
		}
		if (ec == 0)
		{
			memcpy(sector + offset, in + done, chunk);
			ec = _dd_write_lsn(path, lsn, sector);
		}
		if (ec != 0)
		{
			break;
		}
		done += chunk;
	}

	path->filepos = start + done;
	if (ec != 0 && path->filesize > old_size)
	{
		_dd_truncate(path, path->filepos > old_size ? path->filepos : old_size);
	}
	*size = done;

	{
		error_code flush_ec = _dd_flush(path);

		return ec != 0 ? ec : flush_ec;
	}
}

error_code _dragondos_ss_size(dragondos_path_id path, u_int size)
{
	error_code ec = _dd_writable_file(path);

	if (ec != 0)
	{
		return ec;
	}

	if (size > path->filesize)
	{
		ec = _dd_extend(path, size);
	}
	else if (size < path->filesize)
	{
		ec = _dd_truncate(path, size);
	}
	if (ec != 0)
	{
		return ec;
	}
	return _dd_flush(path);
}

error_code _dragondos_ss_fd(dragondos_path_id path, const dragondos_file_stat *stat)
{
	dragondos_dir_entry *entry;
	error_code ec;

	if (path->israw || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}

	ec = _dd_entry(path, path->entry_index, &entry);
	if (ec != 0)
	{
		return ec;
	}

	if (stat->protected_file)
	{
		entry->flags |= DRAGONDOS_FLAG_PROTECTED;
	}
	else
	{
		entry->flags &= (u_char)~DRAGONDOS_FLAG_PROTECTED;
	}
	_dd_entry_dirty(path, path->entry_index);
	return _dd_flush(path);
}

error_code _dragondos_ss_sector(dragondos_path_id path, u_int lsn, const void *buffer)
{
	error_code ec;

	if (!path->israw || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}

	ec = _dd_write_lsn(path, lsn, buffer);
	if (ec == 0 && lsn >= path->directory_lsn && lsn < path->directory_lsn + DRAGONDOS_TRACK_SECTORS)
	{
		path->track_loaded[lsn - path->directory_lsn] = 0;
	}
	return ec;
}

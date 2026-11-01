/********************************************************************
 * libdragondosdir.c - Directory entries and the extents of a file
 ********************************************************************/
#include <string.h>

#include "libdragondosint.h"


/* Parse a filename as $C811 of the ROM does, into 8 and 3 nul filled bytes. */
error_code _dd_parse_name(const char *name, u_char *filename_and_extension)
{
	int position = 0;
	int in_extension = 0;

	memset(filename_and_extension, 0, 11);

	for (; *name != '\0'; name++)
	{
		char c = *name;

		if (c == '.' || c == '/')
		{
			if (position == 0 || in_extension)
			{
				return EOS_BPNAM;
			}
			in_extension = 1;
			position = 8;
			continue;
		}

		/* A character after a full name, unless it starts the extension,
		   ends the parse.  One after a full extension is an error. */
		if (position == 8 && !in_extension)
		{
			break;
		}
		if (position == 11)
		{
			return EOS_BPNAM;
		}

		if (c >= 'a' && c <= 'z')
		{
			c -= 'a' - 'A';
		}
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'))
		{
			return EOS_BPNAM;
		}
		filename_and_extension[position++] = (u_char)c;
	}

	if (filename_and_extension[0] == 0)
	{
		return EOS_BPNAM;
	}
	return 0;
}

error_code _dd_entry(dragondos_path_id path, u_int index, dragondos_dir_entry **entry)
{
	u_char *sector;
	error_code ec;

	if (index >= DRAGONDOS_MAX_DIR_ENTRIES)
	{
		return EOS_WT;
	}

	ec = _dd_track_sector(path, 2 + index / DD_ENTRIES_PER_SECTOR, &sector);
	if (ec != 0)
	{
		return ec;
	}

	*entry = (dragondos_dir_entry *)(sector + (index % DD_ENTRIES_PER_SECTOR) * DD_ENTRY_SIZE);
	return 0;
}

void _dd_entry_dirty(dragondos_path_id path, u_int index)
{
	_dd_track_dirty(path, 2 + index / DD_ENTRIES_PER_SECTOR);
}

error_code _dd_find_file(dragondos_path_id path, const u_char *name, u_int *index)
{
	u_int i;

	for (i = 0; i < DRAGONDOS_MAX_DIR_ENTRIES; i++)
	{
		dragondos_dir_entry *entry;
		error_code ec = _dd_entry(path, i, &entry);

		if (ec != 0)
		{
			return ec;
		}
		if (!(entry->flags & (DRAGONDOS_FLAG_DELETED | DRAGONDOS_FLAG_CONTINUATION)) &&
		    memcmp(entry->u.header.filename, name, 11) == 0)
		{
			*index = i;
			return 0;
		}
		if (entry->flags & DRAGONDOS_FLAG_END)
		{
			break;
		}
	}
	return EOS_PNNF;
}

/* Find the first deleted entry, as $D0FC of the ROM does.  It starts at entry
   1 for a continuation, whose number must not be 0. */
error_code _dd_find_free_entry(dragondos_path_id path, u_int first, u_int *index)
{
	u_int i;

	for (i = first; i < DRAGONDOS_MAX_DIR_ENTRIES; i++)
	{
		dragondos_dir_entry *entry;
		error_code ec = _dd_entry(path, i, &entry);

		if (ec != 0)
		{
			return ec;
		}
		if (entry->flags & DRAGONDOS_FLAG_DELETED)
		{
			*index = i;
			return 0;
		}
	}
	return EOS_DF;
}

error_code _dd_next_entry(const dragondos_dir_entry *entry, u_int *next, int *more)
{
	*more = (entry->flags & DRAGONDOS_FLAG_CONTINUED) != 0;
	*next = entry->last_or_next;
	if (*more && *next >= DRAGONDOS_MAX_DIR_ENTRIES)
	{
		return EOS_WT;
	}
	return 0;
}

void _dd_extents(dragondos_dir_entry *entry, dragondos_extent **extent, int *count)
{
	if (entry->flags & DRAGONDOS_FLAG_CONTINUATION)
	{
		*extent = entry->u.continuation.extent;
		*count = 7;
	}
	else
	{
		*extent = entry->u.header.extent;
		*count = 4;
	}
}

/* Walk the file's entries, stopping at the last or at file_sector.  With
   stop_at set, lsn gets that file sector's LSN, or EOS_EOF if the file is
   shorter.  Otherwise total gets the file's sectors. */
static error_code walk_file(dragondos_path_id path, int stop_at, u_int file_sector, u_int *lsn, u_int *total, u_int *last_entry)
{
	u_int index = path->entry_index;
	u_int sectors = 0;
	int guard;

	for (guard = 0; guard < DRAGONDOS_MAX_DIR_ENTRIES; guard++)
	{
		dragondos_dir_entry *entry;
		dragondos_extent *extent;
		int count, i, more;
		u_int next;
		error_code ec = _dd_entry(path, index, &entry);

		if (ec != 0)
		{
			return ec;
		}

		_dd_extents(entry, &extent, &count);
		for (i = 0; i < count; i++)
		{
			if (stop_at && file_sector < sectors + extent[i].count)
			{
				*lsn = int2(extent[i].lsn) + (file_sector - sectors);
				return 0;
			}
			sectors += extent[i].count;
		}

		ec = _dd_next_entry(entry, &next, &more);
		if (ec != 0)
		{
			return ec;
		}
		if (!more)
		{
			if (stop_at)
			{
				return EOS_EOF;
			}
			*total = sectors;
			*last_entry = index;
			return 0;
		}
		index = next;
	}
	return EOS_WT;
}

error_code _dd_file_size(dragondos_path_id path, u_int *size)
{
	u_int sectors, last_entry;
	dragondos_dir_entry *entry;
	error_code ec = walk_file(path, 0, 0, NULL, &sectors, &last_entry);

	if (ec == 0)
	{
		ec = _dd_entry(path, last_entry, &entry);
	}
	if (ec != 0)
	{
		return ec;
	}

	*size = sectors * DRAGONDOS_SECTOR_SIZE;
	if (sectors != 0 && entry->last_or_next != 0)
	{
		*size -= DRAGONDOS_SECTOR_SIZE - entry->last_or_next;
	}
	return 0;
}

error_code _dragondos_gs_size_entry(dragondos_path_id path, u_int index, u_int *size)
{
	u_int entry_index = path->entry_index;
	error_code ec;

	path->entry_index = index;
	ec = _dd_file_size(path, size);
	path->entry_index = entry_index;
	return ec;
}

error_code _dd_sector_lsn(dragondos_path_id path, u_int file_sector, u_int *lsn)
{
	return walk_file(path, 1, file_sector, lsn, NULL, NULL);
}

error_code _dragondos_readdir(dragondos_path_id path, dragondos_dir_entry *entry)
{
	dragondos_dir_entry *found;
	error_code ec;

	if (path->directory_entry_index >= DRAGONDOS_MAX_DIR_ENTRIES)
	{
		return EOS_EOF;
	}

	ec = _dd_entry(path, path->directory_entry_index, &found);
	if (ec != 0)
	{
		path->directory_entry_index++;
		return ec;
	}

	if (found->flags & DRAGONDOS_FLAG_END)
	{
		return EOS_EOF;
	}

	memcpy(entry, found, sizeof(*entry));
	path->directory_entry_index++;
	return 0;
}

error_code _dragondos_seekdir(dragondos_path_id path, u_int entry)
{
	path->directory_entry_index = entry;
	return 0;
}

error_code _dragondos_ncpy_name(const dragondos_dir_entry *entry, char *name, size_t length)
{
	char work[13];
	int i, position = 0;

	for (i = 0; i < 8 && entry->u.header.filename[i] != 0; i++)
	{
		work[position++] = (char)entry->u.header.filename[i];
	}
	if (entry->u.header.file_extension[0] != 0)
	{
		work[position++] = '.';
		for (i = 0; i < 3 && entry->u.header.file_extension[i] != 0; i++)
		{
			work[position++] = (char)entry->u.header.file_extension[i];
		}
	}
	work[position] = '\0';

	if (length <= (size_t)position)
	{
		return EOS_IA;
	}
	memcpy(name, work, (size_t)position + 1);
	return 0;
}

/********************************************************************
 * libdragondoskill.c - Delete and rename files
 ********************************************************************/
#include <string.h>

#include "libdragondosint.h"


/* As $CFB6 of the ROM does, mark each entry deleted and free its sectors.
   The entries otherwise stay as they were. */
error_code _dragondos_kill(const char *pathlist)
{
	dragondos_path_id path;
	u_int index;
	int guard;
	error_code ec = _dragondos_open(&path, pathlist, FAM_READ | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}

	if (path->israw)
	{
		ec = EOS_BPNAM;
	}
	else
	{
		ec = _dd_writable_file(path);
	}

	index = path->entry_index;
	for (guard = 0; ec == 0; guard++)
	{
		dragondos_dir_entry *entry;
		dragondos_extent *extent;
		int count, i, more = 0;
		u_int next = 0;

		if (guard == DRAGONDOS_MAX_DIR_ENTRIES)
		{
			ec = EOS_WT;
			break;
		}

		ec = _dd_entry(path, index, &entry);
		if (ec == 0)
		{
			ec = _dd_next_entry(entry, &next, &more);
		}
		if (ec != 0)
		{
			break;
		}

		_dd_extents(entry, &extent, &count);
		for (i = 0; ec == 0 && i < count; i++)
		{
			ec = _dd_release(path, int2(extent[i].lsn), extent[i].count);
		}
		entry->flags = DD_KILLED_FLAGS;
		_dd_entry_dirty(path, index);

		if (!more)
		{
			break;
		}
		index = next;
	}

	if (ec == 0)
	{
		ec = _dragondos_close(path);
	}
	else
	{
		/* Abandon any changes. */
		memset(path->track_dirty, 0, sizeof(path->track_dirty));
		_dragondos_close(path);
	}
	return ec;
}

error_code _dragondos_rename(const char *pathlist, const char *new_name)
{
	dragondos_dir_entry entry;

	return _dragondos_rename_ex(pathlist, new_name, &entry);
}

error_code _dragondos_rename_ex(const char *pathlist, const char *new_name, dragondos_dir_entry *renamed)
{
	dragondos_path_id path;
	dragondos_dir_entry *entry;
	u_char name[11];
	u_int existing;
	error_code ec = _dd_parse_name(new_name, name);

	if (ec != 0)
	{
		return ec;
	}

	ec = _dragondos_open(&path, pathlist, FAM_READ | FAM_WRITE);
	if (ec != 0)
	{
		return ec;
	}

	ec = path->israw ? EOS_BPNAM : _dd_writable_file(path);
	if (ec == 0)
	{
		ec = _dd_find_file(path, name, &existing);
		if (ec == 0)
		{
			ec = EOS_FAE;
		}
		else if (ec == EOS_PNNF)
		{
			ec = _dd_entry(path, path->entry_index, &entry);
			if (ec == 0)
			{
				memcpy(entry->u.header.filename, name, 11);
				_dd_entry_dirty(path, path->entry_index);
				memcpy(renamed, entry, sizeof(*renamed));
			}
		}
	}

	{
		error_code close_ec = _dragondos_close(path);

		return ec != 0 ? ec : close_ec;
	}
}

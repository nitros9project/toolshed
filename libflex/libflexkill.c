/********************************************************************
 * libflexkill.c - Deleting and renaming files
 ********************************************************************/
#include <string.h>

#include "libflexint.h"


static error_code open_file(flex_path_id *path, const char *pathlist)
{
	flex_path_id new_path;
	error_code ec = _fx_open_image(&new_path, pathlist, FAM_READ | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}
	if (new_path->israw)
	{
		return _fx_close_image(new_path, EOS_BPNAM);
	}

	{
		u_char name[11];

		ec = _fx_parse_name(new_path->filename, name);
		if (ec == 0)
		{
			ec = _fx_find_file(new_path, name, &new_path->entry_track, &new_path->entry_sector,
					   &new_path->entry_slot, &new_path->entry);
		}
	}
	if (ec != 0)
	{
		return _fx_close_image(new_path, ec);
	}

	*path = new_path;
	return 0;
}

error_code _flex_kill(const char *pathlist)
{
	flex_path_id path = NULL;
	u_int sectors;
	error_code ec = open_file(&path, pathlist);

	if (ec != 0)
	{
		return ec;
	}
	if (path->entry.attributes & FX_ATTR_UNDELETABLE)
	{
		return _fx_close_image(path, EOS_FNA);
	}

	sectors = _flex_entry_sectors(&path->entry);
	path->entry.filename[0] = 0xFF;
	ec = _fx_write_entry(path, path->entry_track, path->entry_sector, path->entry_slot, &path->entry);
	if (ec == 0 && sectors != 0)
	{
		ec = _fx_release(path, path->entry.start_track, path->entry.start_sector,
				 path->entry.end_track, path->entry.end_sector, sectors);
	}
	if (ec == 0)
	{
		ec = _fx_flush(path);
	}
	return _fx_close_image(path, ec);
}

error_code _flex_rename_ex(const char *pathlist, const char *new_name, flex_dir_entry *entry)
{
	flex_path_id path = NULL;
	u_char name[11];
	u_int track, sector, slot;
	error_code ec = open_file(&path, pathlist);

	if (ec != 0)
	{
		return ec;
	}
	if (path->entry.attributes & FX_ATTR_UNDELETABLE)
	{
		return _fx_close_image(path, EOS_FNA);
	}

	ec = _fx_parse_name(new_name, name);
	if (ec != 0)
	{
		return _fx_close_image(path, ec);
	}

	/* As in FLEX, the extension stays the same unless another is given. */
	if (name[8] == '\0')
	{
		memcpy(&name[8], path->entry.file_extension, 3);
	}

	ec = _fx_find_file(path, name, &track, &sector, &slot, NULL);
	if (ec == 0)
	{
		int same = track == path->entry_track && sector == path->entry_sector && slot == path->entry_slot;

		if (!same)
		{
			return _fx_close_image(path, EOS_FAE);
		}
	}
	else if (ec != EOS_PNNF)
	{
		return _fx_close_image(path, ec);
	}

	memcpy(path->entry.filename, name, 8);
	memcpy(path->entry.file_extension, &name[8], 3);
	ec = _fx_write_entry(path, path->entry_track, path->entry_sector, path->entry_slot, &path->entry);
	if (ec == 0 && entry != NULL)
	{
		*entry = path->entry;
	}
	return _fx_close_image(path, ec);
}

error_code _flex_rename(const char *pathlist, const char *new_name)
{
	return _flex_rename_ex(pathlist, new_name, NULL);
}

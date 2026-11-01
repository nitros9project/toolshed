/********************************************************************
 * libdragondosopen.c - Open, create and close paths
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "libdragondosint.h"


void _dd_free_path(dragondos_path_id path)
{
	free(path->imgfile);
	free(path->filename);
	free(path);
}

/* Open the image of a pathlist and read its geometry.  The path is raw if
   the pathlist names no file. */
error_code _dd_open_image(dragondos_path_id *path, const char *pathlist, int mode)
{
	const char *comma = strchr(pathlist, ',');
	dragondos_path_id new_path;
	int image_mode;
	size_t image_length;
	error_code ec;

	if (comma == NULL)
	{
		return EOS_BPNAM;
	}

	new_path = calloc(1, sizeof(*new_path));
	if (new_path == NULL)
	{
		return EOS_OM;
	}

	image_length = (size_t)(comma - pathlist);
	new_path->imgfile = malloc(image_length + 1);
	new_path->filename = malloc(strlen(comma));
	if (new_path->imgfile == NULL || new_path->filename == NULL)
	{
		_dd_free_path(new_path);
		return EOS_OM;
	}
	memcpy(new_path->imgfile, pathlist, image_length);
	new_path->imgfile[image_length] = '\0';
	strcpy(new_path->filename, comma + 1);
	new_path->israw = new_path->filename[0] == '\0' || strcmp(new_path->filename, "@") == 0;
	new_path->mode = mode;

	image_mode = FAM_READ | (mode & (FAM_WRITE | DISKIMAGE_IGNORE_WRITE_PROTECT));
	ec = _diskimage_open(&new_path->disk, new_path->imgfile, image_mode);
	if (ec != 0)
	{
		_dd_free_path(new_path);
		return ec;
	}

	ec = _dd_load_geometry(new_path);
	if (ec != 0)
	{
		_diskimage_close(new_path->disk);
		_dd_free_path(new_path);
		return ec;
	}

	*path = new_path;
	return 0;
}

static error_code close_image(dragondos_path_id path, error_code ec)
{
	error_code close_ec = _diskimage_close(path->disk);

	_dd_free_path(path);
	return ec != 0 ? ec : close_ec;
}

error_code _dragondos_identify(const char *pathname, int *found)
{
	dragondos_path_id path;
	u_char *bitmap;

	*found = 0;

	path = calloc(1, sizeof(*path));
	if (path == NULL)
	{
		return EOS_OM;
	}

	if (_diskimage_open(&path->disk, pathname, FAM_READ) == 0)
	{
		if (_dd_load_geometry(path) == 0 && path->tracks > 20 && _dd_track_sector(path, 0, &bitmap) == 0)
		{
			u_int backup = path->directory_lsn - DD_BACKUP_TRACKS_BACK * path->sectors_per_track;
			u_int i;

			*found = 1;
			for (i = 0; i < DRAGONDOS_TRACK_SECTORS; i++)
			{
				if ((bitmap[(path->directory_lsn + i) / 8] >> ((path->directory_lsn + i) % 8)) & 1 ||
				    (bitmap[(backup + i) / 8] >> ((backup + i) % 8)) & 1)
				{
					*found = 0;
				}
			}
		}
		_diskimage_close(path->disk);
	}

	free(path);
	return 0;
}

error_code _dragondos_open(dragondos_path_id *path, const char *pathlist, int mode)
{
	dragondos_path_id new_path;
	u_char name[11];
	error_code ec = _dd_open_image(&new_path, pathlist, mode);

	if (ec != 0)
	{
		return ec;
	}

	if (!new_path->israw)
	{
		ec = _dd_parse_name(new_path->filename, name);
		if (ec == 0)
		{
			ec = _dd_find_file(new_path, name, &new_path->entry_index);
		}
		if (ec == 0)
		{
			ec = _dd_file_size(new_path, &new_path->filesize);
		}
		if (ec != 0)
		{
			return close_image(new_path, ec);
		}
	}

	*path = new_path;
	return 0;
}

error_code _dragondos_create(dragondos_path_id *path, const char *pathlist, int mode)
{
	dragondos_path_id new_path;
	dragondos_dir_entry *entry;
	u_char name[11];
	u_int existing;
	error_code ec = _dd_open_image(&new_path, pathlist, mode | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}
	if (new_path->israw)
	{
		return close_image(new_path, EOS_BPNAM);
	}

	ec = _dd_parse_name(new_path->filename, name);
	if (ec != 0)
	{
		return close_image(new_path, ec);
	}

	ec = _dd_find_file(new_path, name, &existing);
	if (ec == 0)
	{
		if (mode & FAM_NOCREATE)
		{
			return close_image(new_path, EOS_FAE);
		}

		ec = close_image(new_path, 0);
		if (ec == 0)
		{
			ec = _dragondos_kill(pathlist);
		}
		if (ec != 0)
		{
			return ec;
		}
		return _dragondos_create(path, pathlist, mode);
	}
	if (ec != EOS_PNNF)
	{
		return close_image(new_path, ec);
	}

	ec = _dd_find_free_entry(new_path, 0, &new_path->entry_index);
	if (ec == 0)
	{
		ec = _dd_entry(new_path, new_path->entry_index, &entry);
	}
	if (ec != 0)
	{
		return close_image(new_path, ec);
	}

	memset(entry, 0, sizeof(*entry));
	memcpy(entry->u.header.filename, name, 11);
	_dd_entry_dirty(new_path, new_path->entry_index);

	ec = _dd_flush(new_path);
	if (ec != 0)
	{
		return close_image(new_path, ec);
	}

	*path = new_path;
	return 0;
}

error_code _dragondos_close(dragondos_path_id path)
{
	error_code ec = 0;

	if (path->spare_count != 0)
	{
		ec = _dd_release(path, path->spare_lsn, path->spare_count);
		path->spare_count = 0;
	}
	if (ec == 0)
	{
		ec = _dd_flush(path);
	}
	return close_image(path, ec);
}

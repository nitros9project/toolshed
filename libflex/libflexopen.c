/********************************************************************
 * libflexopen.c - Open, create and close paths
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "libflexint.h"


static void free_path(flex_path_id path)
{
	free(path->imgfile);
	free(path->filename);
	free(path);
}

/* Open the image of a pathlist and read its geometry.  The path is raw if
   the pathlist names no file. */
error_code _fx_open_image(flex_path_id *path, const char *pathlist, int mode)
{
	const char *comma = strchr(pathlist, ',');
	flex_path_id new_path;
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
		free_path(new_path);
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
		free_path(new_path);
		return ec;
	}

	ec = _fx_load_geometry(new_path);
	if (ec != 0)
	{
		_diskimage_close(new_path->disk);
		free_path(new_path);
		return ec;
	}

	_flex_rewinddir(new_path);
	*path = new_path;
	return 0;
}

error_code _fx_close_image(flex_path_id path, error_code ec)
{
	error_code close_ec = _diskimage_close(path->disk);

	free_path(path);
	return ec != 0 ? ec : close_ec;
}

/* Whether a SIR and the image around it look like a FLEX disk. */
static int believable(flex_path_id path)
{
	diskimage_geometry geometry;
	u_char directory[FLEX_SECTOR_SIZE];
	flex_sir sir;
	u_int capacity;

	if (_diskimage_query_geometry(path->disk, &geometry) != 0)
	{
		return 0;
	}
	_fx_decode_sir(path->sir, &sir);

	if (path->linear)
	{
		capacity = path->track0_slots + (path->max_track - 1) * path->max_sector;
		if (geometry.total_sectors < capacity)
		{
			return 0;
		}
	}
	else if (geometry.cylinders <= path->max_track ||
		 geometry.sectors_per_track * geometry.sides < path->max_sector)
	{
		return 0;
	}

	if (sir.month > 12 || sir.day > 31)
	{
		return 0;
	}

	if (sir.free_track == 0 && sir.free_sector == 0)
	{
		if (sir.last_free_track != 0 || sir.last_free_sector != 0)
		{
			return 0;
		}
	}
	else if (!_fx_valid_address(path, sir.free_track, sir.free_sector) ||
		 !_fx_valid_address(path, sir.last_free_track, sir.last_free_sector) ||
		 sir.free_sectors == 0)
	{
		return 0;
	}
	if (sir.free_sectors > _fx_total_sectors(path))
	{
		return 0;
	}

	if (_fx_read_sector(path, 0, FLEX_DIR_SECTOR, directory) != 0)
	{
		return 0;
	}
	return (directory[0] == 0 && directory[1] == 0) || _fx_valid_address(path, directory[0], directory[1]);
}

error_code _flex_identify(const char *pathname, int *found)
{
	flex_path_id path;

	*found = 0;

	path = calloc(1, sizeof(*path));
	if (path == NULL)
	{
		return EOS_OM;
	}

	if (_diskimage_open(&path->disk, pathname, FAM_READ) == 0)
	{
		if (_fx_load_geometry(path) == 0)
		{
			*found = believable(path);
		}
		_diskimage_close(path->disk);
	}

	free(path);
	return 0;
}

static error_code load_file(flex_path_id path)
{
	u_char name[11];
	error_code ec = _fx_parse_name(path->filename, name);

	if (ec == 0)
	{
		ec = _fx_find_file(path, name, &path->entry_track, &path->entry_sector, &path->entry_slot, &path->entry);
	}
	if (ec == 0)
	{
		ec = _fx_set_data_start(path);
	}
	return ec;
}

error_code _flex_open(flex_path_id *path, const char *pathlist, int mode)
{
	flex_path_id new_path;
	error_code ec = _fx_open_image(&new_path, pathlist, mode);

	if (ec != 0)
	{
		return ec;
	}

	if (!new_path->israw)
	{
		/* The only directory is the whole disk's. */
		if (mode & FAM_DIR)
		{
			return _fx_close_image(new_path, EOS_FNA);
		}

		ec = load_file(new_path);
		if (ec != 0)
		{
			return _fx_close_image(new_path, ec);
		}
	}

	*path = new_path;
	return 0;
}

error_code _flex_create(flex_path_id *path, const char *pathlist, int mode)
{
	flex_path_id new_path;
	flex_dir_entry existing;
	u_char name[11];
	u_int track, sector, slot;
	error_code ec = _fx_open_image(&new_path, pathlist, mode | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}
	if (new_path->israw)
	{
		return _fx_close_image(new_path, EOS_BPNAM);
	}

	ec = _fx_parse_name(new_path->filename, name);
	if (ec != 0)
	{
		return _fx_close_image(new_path, ec);
	}

	ec = _fx_find_file(new_path, name, &track, &sector, &slot, &existing);
	if (ec == 0)
	{
		if (mode & FAM_NOCREATE)
		{
			return _fx_close_image(new_path, EOS_FAE);
		}
		if (existing.attributes & FX_ATTR_UNDELETABLE)
		{
			return _fx_close_image(new_path, EOS_FNA);
		}

		ec = _fx_close_image(new_path, 0);
		if (ec == 0)
		{
			ec = _flex_kill(pathlist);
		}
		if (ec != 0)
		{
			return ec;
		}
		return _flex_create(path, pathlist, mode);
	}
	if (ec != EOS_PNNF)
	{
		return _fx_close_image(new_path, ec);
	}

	ec = _fx_find_free_entry(new_path, &new_path->entry_track, &new_path->entry_sector, &new_path->entry_slot);
	if (ec != 0)
	{
		return _fx_close_image(new_path, ec);
	}

	memset(&new_path->entry, 0, sizeof(new_path->entry));
	memcpy(new_path->entry.filename, name, 8);
	memcpy(new_path->entry.file_extension, &name[8], 3);
	_fx_today(&new_path->entry.month);
	ec = _fx_write_entry(new_path, new_path->entry_track, new_path->entry_sector, new_path->entry_slot,
			     &new_path->entry);
	if (ec != 0)
	{
		return _fx_close_image(new_path, ec);
	}

	ec = _fx_set_data_start(new_path);
	if (ec != 0)
	{
		return _fx_close_image(new_path, ec);
	}
	*path = new_path;
	return 0;
}

error_code _flex_close(flex_path_id path)
{
	error_code ec = 0;
	error_code map_ec = 0;

	if (!path->israw && (path->mode & FAM_WRITE))
	{
		ec = _fx_flush_buffer(path);

		/* A map that can't be written still leaves the file and the free
		   chain whole, so the file can be deleted. */
		if (ec == 0 && path->making_random && path->entry.random)
		{
			map_ec = _fx_write_random_map(path);
		}

		/* FLEX deletes a file left with no sectors when it is closed. */
		if (ec == 0 && _flex_entry_sectors(&path->entry) == 0)
		{
			path->entry.filename[0] = 0xFF;
			path->entry_dirty = 1;
		}
		if (ec == 0 && path->entry_dirty)
		{
			ec = _fx_write_entry(path, path->entry_track, path->entry_sector, path->entry_slot, &path->entry);
			path->entry_dirty = 0;
		}
	}
	if (ec == 0)
	{
		ec = _fx_flush(path);
	}
	return _fx_close_image(path, ec != 0 ? ec : map_ec);
}

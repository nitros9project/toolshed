/********************************************************************
 * libflexdir.c - Filenames and the directory
 ********************************************************************/
#include <ctype.h>
#include <string.h>

#include "libflexint.h"


static int name_character(int c)
{
	return isalnum(c) || c == '-' || c == '_';
}

/* Make the 11 bytes of a directory entry's name and extension. */
error_code _fx_parse_name(const char *name, u_char *filename_and_extension)
{
	const char *p = name;
	int i;

	memset(filename_and_extension, 0, 11);

	/* A drive number, as in "1.NAME.TXT" */
	if (isdigit((unsigned char)p[0]) && p[1] == '.')
	{
		p += 2;
	}

	if (!isalpha((unsigned char)*p))
	{
		return EOS_BPNAM;
	}
	/* Characters past the eighth are ignored, as host filenames are often
	   longer. */
	for (i = 0; name_character((unsigned char)*p); i++, p++)
	{
		if (i < 8)
		{
			filename_and_extension[i] = (u_char)toupper((unsigned char)*p);
		}
	}

	if (*p == '.')
	{
		p++;
		for (i = 0; name_character((unsigned char)*p); i++, p++)
		{
			if (i == 3)
			{
				return EOS_BPNAM;
			}
			filename_and_extension[8 + i] = (u_char)toupper((unsigned char)*p);
		}
	}

	return *p == '\0' ? 0 : EOS_BPNAM;
}

int _flex_entry_in_use(const flex_dir_entry *entry)
{
	return entry->filename[0] != FLEX_ENTRY_UNUSED && !(entry->filename[0] & FLEX_ENTRY_DELETED);
}

u_int _flex_entry_sectors(const flex_dir_entry *entry)
{
	return _fx_get2(entry->sectors);
}

static int same_name(const flex_dir_entry *entry, const u_char *name)
{
	int i;

	for (i = 0; i < 11; i++)
	{
		u_char have = i < 8 ? entry->filename[i] : entry->file_extension[i - 8];

		if (toupper(have) != name[i])
		{
			return 0;
		}
	}
	return 1;
}

error_code _fx_read_entry(flex_path_id path, u_int track, u_int sector, u_int slot, flex_dir_entry *entry)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	error_code ec = _fx_read_sector(path, track, sector, buffer);

	if (ec == 0)
	{
		memcpy(entry, &buffer[FLEX_DIR_HEADER + slot * FX_ENTRY_SIZE], FX_ENTRY_SIZE);
	}
	return ec;
}

error_code _fx_write_entry(flex_path_id path, u_int track, u_int sector, u_int slot, const flex_dir_entry *entry)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	error_code ec = _fx_read_sector(path, track, sector, buffer);

	if (ec == 0)
	{
		memcpy(&buffer[FLEX_DIR_HEADER + slot * FX_ENTRY_SIZE], entry, FX_ENTRY_SIZE);
		ec = _fx_write_sector(path, track, sector, buffer);
	}
	return ec;
}

/* Visit each directory sector in turn.  The visitor returns 1 to stop.  At
   the end, *last_track and *last_sector are those of the last sector seen. */
typedef int (*dir_visitor)(const u_char *sector, u_int track, u_int number, void *context);

static error_code walk_directory(flex_path_id path, dir_visitor visit, void *context,
				 u_int *last_track, u_int *last_sector, int *stopped)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int track = 0, sector = FLEX_DIR_SECTOR;
	u_int limit = _fx_total_sectors(path) + 255;
	u_int seen;

	*stopped = 0;
	for (seen = 0; seen < limit; seen++)
	{
		error_code ec = _fx_read_sector(path, track, sector, buffer);

		if (ec != 0)
		{
			return ec;
		}
		*last_track = track;
		*last_sector = sector;
		if (visit(buffer, track, sector, context))
		{
			*stopped = 1;
			return 0;
		}
		if (buffer[0] == 0 && buffer[1] == 0)
		{
			return 0;
		}
		track = buffer[0];
		sector = buffer[1];
	}
	return EOS_SECT;
}

typedef struct
{
	const u_char *name;
	int want_free;
	u_int track;
	u_int sector;
	u_int slot;
	flex_dir_entry entry;
} find_context;

static int find_visitor(const u_char *sector, u_int track, u_int number, void *context)
{
	find_context *find = context;
	u_int slot;

	for (slot = 0; slot < FLEX_DIR_ENTRIES; slot++)
	{
		const flex_dir_entry *entry = (const flex_dir_entry *)&sector[FLEX_DIR_HEADER + slot * FX_ENTRY_SIZE];
		int match = find->want_free ? !_flex_entry_in_use(entry) :
					      _flex_entry_in_use(entry) && same_name(entry, find->name);

		if (match)
		{
			find->track = track;
			find->sector = number;
			find->slot = slot;
			memcpy(&find->entry, entry, sizeof(find->entry));
			return 1;
		}
	}
	return 0;
}

error_code _fx_find_file(flex_path_id path, const u_char *name, u_int *track, u_int *sector, u_int *slot,
			 flex_dir_entry *entry)
{
	find_context find;
	u_int last_track = 0, last_sector = 0;
	int stopped;
	error_code ec;

	memset(&find, 0, sizeof(find));
	find.name = name;
	ec = walk_directory(path, find_visitor, &find, &last_track, &last_sector, &stopped);
	if (ec != 0)
	{
		return ec;
	}
	if (!stopped)
	{
		return EOS_PNNF;
	}
	*track = find.track;
	*sector = find.sector;
	*slot = find.slot;
	if (entry != NULL)
	{
		*entry = find.entry;
	}
	return 0;
}

/* Find an entry that is unused or deleted, adding a sector from the free
   chain to the directory if there is none. */
error_code _fx_find_free_entry(flex_path_id path, u_int *track, u_int *sector, u_int *slot)
{
	find_context find;
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int last_track = 0, last_sector = 0, new_track, new_sector;
	int stopped;
	error_code ec;

	memset(&find, 0, sizeof(find));
	find.want_free = 1;
	ec = walk_directory(path, find_visitor, &find, &last_track, &last_sector, &stopped);
	if (ec != 0)
	{
		return ec;
	}
	if (stopped)
	{
		*track = find.track;
		*sector = find.sector;
		*slot = find.slot;
		return 0;
	}

	ec = _fx_allocate(path, &new_track, &new_sector);
	if (ec != 0)
	{
		return ec;
	}
	memset(buffer, 0, sizeof(buffer));
	ec = _fx_write_sector(path, new_track, new_sector, buffer);
	if (ec == 0)
	{
		ec = _fx_read_sector(path, last_track, last_sector, buffer);
	}
	if (ec == 0)
	{
		buffer[0] = (u_char)new_track;
		buffer[1] = (u_char)new_sector;
		ec = _fx_write_sector(path, last_track, last_sector, buffer);
	}
	if (ec == 0)
	{
		ec = _fx_flush(path);
	}
	if (ec != 0)
	{
		return ec;
	}

	*track = new_track;
	*sector = new_sector;
	*slot = 0;
	return 0;
}

error_code _flex_rewinddir(flex_path_id path)
{
	path->dir_track = 0;
	path->dir_sector = FLEX_DIR_SECTOR;
	path->dir_slot = 0;
	path->dir_sectors_seen = 0;
	return 0;
}

error_code _flex_readdir(flex_path_id path, flex_dir_entry *entry)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	error_code ec;

	if (path->dir_sector == 0)
	{
		return EOS_EOF;
	}
	if (path->dir_sectors_seen > _fx_total_sectors(path) + 255)
	{
		path->dir_sector = 0;
		return EOS_SECT;
	}

	ec = _fx_read_sector(path, path->dir_track, path->dir_sector, buffer);
	if (ec != 0)
	{
		path->dir_sector = 0;
		return ec;
	}

	memcpy(entry, &buffer[FLEX_DIR_HEADER + path->dir_slot * FX_ENTRY_SIZE], FX_ENTRY_SIZE);
	if (++path->dir_slot == FLEX_DIR_ENTRIES)
	{
		path->dir_slot = 0;
		path->dir_sectors_seen++;
		path->dir_track = buffer[0];
		path->dir_sector = buffer[1];
		if (path->dir_track == 0 && path->dir_sector == 0)
		{
			path->dir_sector = 0;
		}
	}
	return 0;
}

error_code _flex_ncpy_name(const flex_dir_entry *entry, char *name, size_t length)
{
	char text[13];
	int n = 0;
	int i;

	for (i = 0; i < 8 && entry->filename[i] != '\0'; i++)
	{
		text[n++] = (char)(i == 0 && (entry->filename[0] & FLEX_ENTRY_DELETED) ? '?' : entry->filename[i]);
	}
	if (entry->file_extension[0] != '\0')
	{
		text[n++] = '.';
		for (i = 0; i < 3 && entry->file_extension[i] != '\0'; i++)
		{
			text[n++] = (char)entry->file_extension[i];
		}
	}
	text[n] = '\0';

	if (length == 0)
	{
		return EOS_IA;
	}
	if ((size_t)n >= length)
	{
		n = (int)length - 1;
	}
	memcpy(name, text, (size_t)n);
	name[n] = '\0';
	return 0;
}

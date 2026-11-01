/********************************************************************
 * libdragondosbitmap.c - The allocation bitmap
 *
 * _dd_allocate follows the routine at $CD31 of the Dragon DOS 1.0 ROM.
 ********************************************************************/
#include "libdragondosint.h"


/* Get a byte of the bitmap, without the bits of sectors past the end of the
   disk.  On a valid disk there are none, so this changes nothing but that
   a single sided disk doesn't need the bitmap's second sector. */
static error_code bitmap_byte(dragondos_path_id path, u_int base, u_int offset, u_char *value)
{
	u_int first = base + offset * 8;
	u_char *sector;
	error_code ec;

	*value = 0;
	if (first >= path->total_sectors || offset >= DD_BITMAP_BYTES)
	{
		return 0;
	}

	ec = _dd_track_sector(path, base / DD_BITMAP_SECTOR_LSNS, &sector);
	if (ec != 0)
	{
		return ec;
	}

	*value = sector[offset];
	if (first + 8 > path->total_sectors)
	{
		*value &= (u_char)((1 << (path->total_sectors - first)) - 1);
	}
	return 0;
}

error_code _dd_sector_free(dragondos_path_id path, u_int lsn, int *is_free)
{
	u_int base = lsn - lsn % DD_BITMAP_SECTOR_LSNS;
	u_char value;
	error_code ec = bitmap_byte(path, base, (lsn - base) / 8, &value);

	if (ec == 0)
	{
		*is_free = (value >> (lsn % 8)) & 1;
	}
	return ec;
}

static error_code set_bitmap(dragondos_path_id path, u_int lsn, int is_free)
{
	u_int index = lsn / DD_BITMAP_SECTOR_LSNS;
	u_int offset = lsn % DD_BITMAP_SECTOR_LSNS;
	u_char *sector;
	error_code ec = _dd_track_sector(path, index, &sector);

	if (ec != 0)
	{
		return ec;
	}

	if (is_free)
	{
		sector[offset / 8] |= (u_char)(1 << (offset % 8));
	}
	else
	{
		sector[offset / 8] &= (u_char)~(1 << (offset % 8));
	}
	_dd_track_dirty(path, index);
	return 0;
}

/* Allocate the free sectors from lsn up, to the most an extent can hold.
   A run stops at the end of the bitmap sector it began in. */
static error_code take_run(dragondos_path_id path, u_int lsn, u_int *count)
{
	u_int taken = 0;
	int is_free = 1;

	while (is_free && taken < DD_MAX_EXTENT)
	{
		error_code ec = set_bitmap(path, lsn + taken, 0);

		if (ec != 0)
		{
			return ec;
		}
		taken++;

		is_free = 0;
		if ((lsn + taken) % DD_BITMAP_SECTOR_LSNS != 0)
		{
			ec = _dd_sector_free(path, lsn + taken, &is_free);
			if (ec != 0)
			{
				return ec;
			}
		}
	}

	*count = taken;
	return 0;
}

/* Scan one sector of the bitmap for its longest run of bytes which begins
   with any nonzero byte and carries on through $FF bytes.  run is that
   length in bytes.  lsn is where to allocate: a run of more than one byte
   gives the free sectors joining its first byte to the next, and a run of
   one byte gives the free sectors reaching that byte's highest set bit. */
static error_code scan_bitmap(dragondos_path_id path, u_int base, u_int *run, u_int *lsn)
{
	u_int best = 0, best_offset = 0;
	u_int offset = 0;
	u_char value;
	int bit;
	u_int position;

	*run = 0;
	*lsn = base + DD_BITMAP_SECTOR_LSNS;

	while (offset < DD_BITMAP_BYTES)
	{
		u_int start = offset, length = 1;
		error_code ec = bitmap_byte(path, base, offset, &value);

		if (ec != 0)
		{
			return ec;
		}
		offset++;
		if (value == 0)
		{
			continue;
		}

		for (;;)
		{
			ec = bitmap_byte(path, base, offset, &value);
			if (ec != 0)
			{
				return ec;
			}
			if (value != 0xFF)
			{
				break;
			}
			length++;
			offset++;
		}

		if (length > best)
		{
			best = length;
			best_offset = start;
		}
	}

	if (best == 0)
	{
		return 0;
	}

	{
		error_code ec = bitmap_byte(path, base, best_offset, &value);

		if (ec != 0)
		{
			return ec;
		}
	}

	bit = 7;
	position = base + best_offset * 8 + 8;
	if (best == 1)
	{
		while (!((value >> bit) & 1))
		{
			bit--;
			position--;
		}
	}
	while (bit >= 0 && ((value >> bit) & 1))
	{
		bit--;
		position--;
	}

	*run = best;
	*lsn = position;
	return 0;
}

/* Allocate a run of sectors for a file.  hint is the sector that would
   continue the file, or DD_NO_HINT for a file with no sectors yet.

   If the hint is not free, look from the directory track towards track 0
   for the first track whose first sector is free, as is the first sector
   of the next track out.  Failing that, take the lowest track found with
   its first sector free.  Failing that, look from the directory track
   towards the end of the disk for a track with its first sector free. A
   hint past the first bitmap sector only looks towards the end of the disk,
   beginning at LSN 1440.

   When no track has its first sector free, look for the longest run in
   the second then the first sector of the bitmap. */
error_code _dd_allocate(dragondos_path_id path, u_int hint, u_int *lsn, u_int *count)
{
	u_int position = path->directory_lsn;
	u_int run, second_run, second_lsn;
	int is_free;
	int look_down = 1;
	error_code ec;

	if (hint != DD_NO_HINT)
	{
		ec = _dd_sector_free(path, hint, &is_free);
		if (ec != 0)
		{
			return ec;
		}
		if (is_free)
		{
			position = hint;
			goto take;
		}
		if (hint >= DD_BITMAP_SECTOR_LSNS)
		{
			position = DD_BITMAP_SECTOR_LSNS - DRAGONDOS_TRACK_SECTORS;
			look_down = 0;
		}
	}

	if (look_down)
	{
		int found = 0;
		u_int candidate = 0;

		while (position >= DRAGONDOS_TRACK_SECTORS)
		{
			position -= DRAGONDOS_TRACK_SECTORS;
			ec = _dd_sector_free(path, position, &is_free);
			if (ec != 0)
			{
				return ec;
			}
			if (!is_free)
			{
				found = 0;
				continue;
			}
			if (found)
			{
				goto take;
			}
			found = 1;
			candidate = position;
		}
		if (found)
		{
			position = candidate;
			goto take;
		}
		position = path->directory_lsn;
	}

	for (position += DRAGONDOS_TRACK_SECTORS; position <= DD_MAX_LSN; position += DRAGONDOS_TRACK_SECTORS)
	{
		ec = _dd_sector_free(path, position, &is_free);
		if (ec != 0)
		{
			return ec;
		}
		if (is_free)
		{
			goto take;
		}
	}

	ec = scan_bitmap(path, DD_BITMAP_SECTOR_LSNS, &second_run, &second_lsn);
	if (ec != 0)
	{
		return ec;
	}
	position = second_lsn;
	if (second_run < 2)
	{
		ec = scan_bitmap(path, 0, &run, &position);
		if (ec != 0)
		{
			return ec;
		}
		if (run < 2 && run <= second_run)
		{
			position = second_lsn;
			ec = _dd_sector_free(path, position, &is_free);
			if (ec != 0)
			{
				return ec;
			}
			if (!is_free)
			{
				return EOS_DF;
			}
		}
	}

take:
	*lsn = position;
	return take_run(path, position, count);
}

error_code _dd_release(dragondos_path_id path, u_int lsn, u_int count)
{
	u_int i;

	if (count != 0 && lsn + count > DD_MAX_LSN)
	{
		return EOS_WT;
	}

	for (i = 0; i < count; i++)
	{
		error_code ec = set_bitmap(path, lsn + i, 1);

		if (ec != 0)
		{
			return ec;
		}
	}
	return 0;
}

error_code _dragondos_gs_free(dragondos_path_id path, u_int *sectors)
{
	u_int lsn, total = 0;

	for (lsn = 0; lsn < path->total_sectors; lsn += 8)
	{
		u_char value;
		error_code ec = bitmap_byte(path, lsn - lsn % DD_BITMAP_SECTOR_LSNS, (lsn % DD_BITMAP_SECTOR_LSNS) / 8, &value);

		if (ec != 0)
		{
			return ec;
		}
		for (; value != 0; value >>= 1)
		{
			total += value & 1;
		}
	}

	*sectors = total;
	return 0;
}

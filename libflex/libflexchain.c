/********************************************************************
 * libflexchain.c - Sector chains and the free chain
 *
 * Free sectors are taken from the start of the free chain, and returned
 * to its end, as FLEX does.  The chain ends at the last free sector the
 * SIR names, as some disks have a link onwards from it.  The SIR is changed
 * in memory, and written by _fx_flush.
 ********************************************************************/
#include <string.h>

#include "libflexint.h"


error_code _fx_follow(flex_path_id path, u_int track, u_int sector, u_int steps, u_int *next_track, u_int *next_sector)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int i;

	for (i = 0; i < steps; i++)
	{
		error_code ec = _fx_read_sector(path, track, sector, buffer);

		if (ec != 0)
		{
			return ec;
		}
		track = buffer[0];
		sector = buffer[1];
		if (!_fx_valid_address(path, track, sector))
		{
			return EOS_SECT;
		}
	}
	*next_track = track;
	*next_sector = sector;
	return 0;
}

error_code _fx_allocate(flex_path_id path, u_int *track, u_int *sector)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int first_track = path->sir[FX_SIR_FREE];
	u_int first_sector = path->sir[FX_SIR_FREE + 1];
	u_int count = _fx_get2(&path->sir[FX_SIR_FREE_COUNT]);
	error_code ec;

	if (first_track == 0 && first_sector == 0)
	{
		return EOS_DF;
	}

	ec = _fx_read_sector(path, first_track, first_sector, buffer);
	if (ec != 0)
	{
		return ec;
	}

	/* The chain ends at the last sector the SIR names, or a link of 0,0,
	   whatever the count. */
	if ((buffer[0] == 0 && buffer[1] == 0) ||
	    (first_track == path->sir[FX_SIR_LAST_FREE] && first_sector == path->sir[FX_SIR_LAST_FREE + 1]))
	{
		_fx_set_free_chain(path, 0, 0, 0, 0, 0);
	}
	else
	{
		if (!_fx_valid_address(path, buffer[0], buffer[1]))
		{
			return EOS_SECT;
		}
		_fx_set_free_chain(path, buffer[0], buffer[1], path->sir[FX_SIR_LAST_FREE],
				   path->sir[FX_SIR_LAST_FREE + 1], count > 0 ? count - 1 : 0);
	}

	*track = first_track;
	*sector = first_sector;
	return 0;
}

error_code _fx_release(flex_path_id path, u_int first_track, u_int first_sector,
		       u_int last_track, u_int last_sector, u_int count)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int free_track = path->sir[FX_SIR_FREE];
	u_int free_sector = path->sir[FX_SIR_FREE + 1];
	u_int free_count = _fx_get2(&path->sir[FX_SIR_FREE_COUNT]);
	error_code ec;

	if (count == 0)
	{
		return 0;
	}

	/* The chain ends where the free chain will. */
	ec = _fx_read_sector(path, last_track, last_sector, buffer);
	if (ec == 0 && (buffer[0] != 0 || buffer[1] != 0))
	{
		buffer[0] = 0;
		buffer[1] = 0;
		ec = _fx_write_sector(path, last_track, last_sector, buffer);
	}
	if (ec != 0)
	{
		return ec;
	}

	if (free_track == 0 && free_sector == 0)
	{
		_fx_set_free_chain(path, first_track, first_sector, last_track, last_sector, count);
		return 0;
	}

	ec = _fx_read_sector(path, path->sir[FX_SIR_LAST_FREE], path->sir[FX_SIR_LAST_FREE + 1], buffer);
	if (ec == 0)
	{
		buffer[0] = (u_char)first_track;
		buffer[1] = (u_char)first_sector;
		ec = _fx_write_sector(path, path->sir[FX_SIR_LAST_FREE], path->sir[FX_SIR_LAST_FREE + 1], buffer);
	}
	if (ec != 0)
	{
		return ec;
	}

	_fx_set_free_chain(path, free_track, free_sector, last_track, last_sector, free_count + count);
	return 0;
}

error_code _flex_count_free_chain(flex_path_id path, u_int *sectors)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int track = path->sir[FX_SIR_FREE];
	u_int sector = path->sir[FX_SIR_FREE + 1];
	u_int limit = _fx_total_sectors(path) + 255;
	u_int count = 0;

	*sectors = 0;
	if (track == 0 && sector == 0)
	{
		return 0;
	}

	while (count <= limit)
	{
		error_code ec = _fx_read_sector(path, track, sector, buffer);

		if (ec != 0)
		{
			*sectors = count;
			return ec;
		}
		count++;
		if (track == path->sir[FX_SIR_LAST_FREE] && sector == path->sir[FX_SIR_LAST_FREE + 1])
		{
			*sectors = count;
			return 0;
		}
		if (buffer[0] == 0 && buffer[1] == 0)
		{
			/* It ends before the last sector the SIR names. */
			*sectors = count;
			return EOS_SECT;
		}
		track = buffer[0];
		sector = buffer[1];
	}

	*sectors = count;
	return EOS_SECT;
}

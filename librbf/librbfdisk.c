/********************************************************************
 * librbfdisk.c - Logical sectors of an OS-9 disk image
 *
 * An LSN is turned into an address on the disk, rather than a place in the
 * image's stream, so that a missing sector fails by itself and does not move
 * the sectors after it.
 ********************************************************************/
#include <string.h>

#include "cocotypes.h"
#include "os9path.h"
#include "cocopath.h"

#define RAW_SECTORS_PER_TRACK	18
#define RAW_SECTOR_SIZE		256
#define MAX_SECTOR_SIZE		1024


/* One sector of the image, as the image stores it. */
typedef struct
{
	u_int cylinder;
	u_int side;
	u_int sector;
	u_int size;
	u_int offset;	/* where the transfer begins in the sector */
} sector_address;


/*
 * locate()
 *
 * Find the sector holding the byte 'offset' bytes past the start of 'lsn'.
 *
 * A raw image keeps the unused slots of a short track 0 and has no tracks or
 * sides of its own, so it is addressed by slot whatever the disk's layout. Its
 * sectors are always 256 bytes, so a larger LSN spans several.
 */
static error_code locate(os9_path_id path, long long lsn, u_int offset, sector_address *address)
{
	if (path->raw_image)
	{
		long long slot = lsn >= path->t0s ? lsn + path->spt - path->t0s : lsn;

		slot = slot * (path->bps / RAW_SECTOR_SIZE) + offset / RAW_SECTOR_SIZE;
		address->cylinder = (u_int) (slot / RAW_SECTORS_PER_TRACK);
		address->side = 0;
		address->sector = (u_int) (slot % RAW_SECTORS_PER_TRACK) + 1;
		address->size = RAW_SECTOR_SIZE;
		address->offset = offset % RAW_SECTOR_SIZE;
	}
	else
	{
		long long track = 0;
		long long index = lsn;

		if (lsn >= path->t0s)
		{
			if (path->spt == 0)
			{
				return EOS_SECT;
			}
			track = (lsn - path->t0s) / path->spt + 1;
			index = (lsn - path->t0s) % path->spt;
		}

		address->cylinder = (u_int) (track / path->sides);
		address->side = (u_int) (track % path->sides);
		address->sector = (u_int) index + path->first_sector;
		address->size = path->bps;
		address->offset = offset;
	}

	return 0;
}


/* Bad data is still the data on the disk, so it is used with a warning. */
static error_code read_address(os9_path_id path, const sector_address *address, u_char *sector)
{
	u_int size = MAX_SECTOR_SIZE;
	error_code ec = _diskimage_read_sector(path->disk, address->cylinder, address->side, address->sector, sector, &size);

	if (ec == EOS_CRC && path->quiet)
	{
		ec = 0;
	}
	if (ec == EOS_CRC)
	{
		fprintf(stderr, "Warning: bad CRC in cylinder %u side %u sector %u of %s\n",
			address->cylinder, address->side, address->sector, path->imgfile);
		ec = 0;
	}
	if (ec == EOS_IA || (ec == 0 && size != address->size))
	{
		return EOS_SECT;
	}

	return ec;
}


static error_code get_bytes(os9_path_id path, int lsn, u_int offset, void *buffer, u_int size, u_int *done)
{
	u_char sector[MAX_SECTOR_SIZE];
	u_char *out = buffer;

	*done = 0;

	while (*done < size)
	{
		sector_address address;
		u_int position = offset + *done;
		u_int count;
		error_code ec = locate(path, (long long) lsn + position / path->bps, position % path->bps, &address);

		if (ec == 0)
		{
			ec = read_address(path, &address, sector);
		}
		if (ec != 0)
		{
			return ec;
		}

		count = address.size - address.offset;
		if (count > size - *done)
		{
			count = size - *done;
		}
		memcpy(out + *done, sector + address.offset, count);
		*done += count;
	}

	return 0;
}


static error_code put_bytes(os9_path_id path, int lsn, u_int offset, const void *buffer, u_int size, u_int *done)
{
	u_char sector[MAX_SECTOR_SIZE];
	const u_char *in = buffer;

	*done = 0;

	while (*done < size)
	{
		sector_address address;
		u_int position = offset + *done;
		u_int count;
		error_code ec = locate(path, (long long) lsn + position / path->bps, position % path->bps, &address);

		if (ec != 0)
		{
			return ec;
		}

		if (address.size > MAX_SECTOR_SIZE)
		{
			return EOS_SECT;
		}

		count = address.size - address.offset;
		if (count > size - *done)
		{
			count = size - *done;
		}

		/* Keep the rest of a sector only partly written.  A raw image
		   grows, and the rest of a new sector is $FF. */
		if (count < address.size)
		{
			ec = read_address(path, &address, sector);
			if (ec == EOS_SECT && path->raw_image)
			{
				memset(sector, 0xFF, address.size);
				ec = 0;
			}
			if (ec != 0)
			{
				return ec;
			}
		}

		memcpy(sector + address.offset, in + *done, count);
		ec = _diskimage_write_sector(path->disk, address.cylinder, address.side, address.sector, sector, address.size);
		if (ec != 0)
		{
			return ec == EOS_IA ? EOS_SECT : ec;
		}
		*done += count;
	}

	return 0;
}


/*
 * _os9_lsn_get()
 *
 * Read from a run of contiguous LSNs, starting 'offset' bytes past 'lsn'.
 */

error_code _os9_lsn_get(os9_path_id path, int lsn, u_int offset, void *buffer, u_int size)
{
	u_int done;

	return get_bytes(path, lsn, offset, buffer, size, &done);
}


/*
 * _os9_lsn_put()
 *
 * Write to a run of contiguous LSNs, starting 'offset' bytes past 'lsn'.
 */

error_code _os9_lsn_put(os9_path_id path, int lsn, u_int offset, const void *buffer, u_int size)
{
	u_int done;

	return put_bytes(path, lsn, offset, buffer, size, &done);
}


/*
 * _os9_lsn_read()
 *
 * As _os9_lsn_get, returning the number of bytes read.
 */

u_int _os9_lsn_read(os9_path_id path, int lsn, u_int offset, void *buffer, u_int size)
{
	u_int done;

	get_bytes(path, lsn, offset, buffer, size, &done);

	return done;
}


/*
 * _os9_lsn_write()
 *
 * As _os9_lsn_put, returning the number of bytes written.
 */

u_int _os9_lsn_write(os9_path_id path, int lsn, u_int offset, const void *buffer, u_int size)
{
	u_int done;

	put_bytes(path, lsn, offset, buffer, size, &done);

	return done;
}

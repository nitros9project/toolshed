/********************************************************************
 * libflexboot.c - Make a disk boot FLEX on the Color Computer
 *
 * The Color Computer starts FLEX from Disk BASIC, with RUN "FLEX".  A boot
 * disk is laid out as FHL's PUTLDR lays it out: it is a Disk BASIC disk
 * too, with a directory in track 17 sector 3, its allocation table in
 * sector 2, and one file, FLEX.BAS, in granule 32.  That is track 16
 * sectors 1 to 9.  FLEX.BAS is in sector 1, and the loader it runs is in
 * sector 9.  These sectors are taken out of FLEX's free chain.  The loader
 * finds FLEX.SYS from the track and sector at its offset 5, where LINK
 * puts them.
 *
 * FLEX.BAS and the loader are ToolShed's own.  The loader's source is
 * flexboot.asm, next to this file.
 ********************************************************************/
#include <string.h>

#include "libflexint.h"

#define FX_BOOT_DIR_TRACK	17
#define FX_BOOT_FAT_SECTOR	2
#define FX_BOOT_DIR_SECTOR	3
#define FX_BOOT_BASIC_SECTOR	1
#define FX_BOOT_GRANULE		32
#define FX_BOOT_GRANULES	68	/* in a 35 track Disk BASIC table */
#define FX_GRANULE_USED		0xC0	/* the last granule of a file, with no sectors */
#define FX_DECB_ENTRY_SIZE	32
#define FX_DECB_BASIC		0x00	/* file type */
#define FX_DECB_ASCII		0xFF	/* ASCII flag */

/* flexboot.asm, assembled */
static const u_char loader[] = {
	0x20, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0xEF, 0x8C, 0xFA, 0xEC, 0x8C, 0xF1, 0xFD, 0x06, 0x00, 0x10,
	0x8E, 0x07, 0x00, 0x8D, 0x3D, 0x81, 0x02, 0x27, 0x10, 0x81, 0x16, 0x26,
	0xF6, 0x8D, 0x33, 0xA7, 0x8C, 0xDD, 0x8D, 0x2E, 0xA7, 0x8C, 0xD9, 0x20,
	0xEA, 0x8D, 0x27, 0xA7, 0x8C, 0xD3, 0x8D, 0x22, 0xA7, 0x8C, 0xCF, 0x8D,
	0x1D, 0x1F, 0x89, 0x5D, 0x27, 0xD9, 0xAE, 0x8C, 0xC4, 0x8D, 0x13, 0x34,
	0x01, 0x1A, 0x50, 0xB7, 0xFF, 0xDF, 0xA7, 0x80, 0xB7, 0xFF, 0xDE, 0x35,
	0x01, 0x5A, 0x26, 0xED, 0x20, 0xC1, 0x10, 0x8C, 0x07, 0x00, 0x26, 0x27,
	0x34, 0x14, 0xFC, 0x06, 0x00, 0x27, 0x23, 0xFD, 0x00, 0xEC, 0x86, 0x02,
	0xB7, 0x00, 0xEA, 0x7F, 0x00, 0xEB, 0x8E, 0x06, 0x00, 0xBF, 0x00, 0xEE,
	0xAD, 0x9F, 0xC0, 0x04, 0x7D, 0x00, 0xF0, 0x26, 0x12, 0x35, 0x14, 0x10,
	0x8E, 0x06, 0x04, 0xA6, 0xA0, 0x39, 0x1A, 0x50, 0xB7, 0xFF, 0xDF, 0x6E,
	0x9D, 0xFF, 0x74, 0x10, 0xEE, 0x8D, 0xFF, 0x73, 0x39
};

/* As a Disk BASIC program, which DSKI$ reads into two strings.  Together they
   are 255 bytes, from where the loader runs. */
static const char flex_bas[] =
	"10 ' BOOT FLEX, FROM TOOLSHED FLEX GEN\r"
	"20 CLEAR 1000\r"
	"30 DSKI$ 0,16,9,A$,B$\r"
	"40 L$=A$+LEFT$(B$,127):V=VARPTR(L$)\r"
	"50 EXEC PEEK(V+2)*256+PEEK(V+3)\r"
	"60 PRINT \"FLEX DID NOT LOAD\"\r";

/* The sectors a boot disk takes from FLEX */
static const u_char boot_sectors[][2] = {
	{FLEX_BOOT_TRACK, 1}, {FLEX_BOOT_TRACK, 2}, {FLEX_BOOT_TRACK, 3},
	{FLEX_BOOT_TRACK, 4}, {FLEX_BOOT_TRACK, 5}, {FLEX_BOOT_TRACK, 6},
	{FLEX_BOOT_TRACK, 7}, {FLEX_BOOT_TRACK, 8}, {FLEX_BOOT_TRACK, FLEX_BOOT_LOADER_SECTOR},
	{FX_BOOT_DIR_TRACK, FX_BOOT_FAT_SECTOR}, {FX_BOOT_DIR_TRACK, FX_BOOT_DIR_SECTOR}
};
#define BOOT_SECTORS	(sizeof(boot_sectors) / sizeof(boot_sectors[0]))


/* Sectors on each side of a track past 0 */
static u_int side_sectors(flex_path_id path)
{
	int two_sides = path->linear ? path->max_sector > FLEX_DD_SECTORS : path->sides == 2;

	return two_sides ? path->max_sector / 2 : path->max_sector;
}

static int is_boot_sector(u_int track, u_int sector)
{
	u_int i;

	for (i = 0; i < BOOT_SECTORS; i++)
	{
		if (boot_sectors[i][0] == track && boot_sectors[i][1] == sector)
		{
			return 1;
		}
	}
	return 0;
}

/* Whether the Disk BASIC directory names FLEX.BAS. */
static error_code has_boot_area(flex_path_id path, int *found)
{
	u_char sector[FLEX_SECTOR_SIZE];
	error_code ec = _fx_read_sector(path, FX_BOOT_DIR_TRACK, FX_BOOT_DIR_SECTOR, sector);

	*found = ec == 0 && memcmp(sector, "FLEX    BAS", 11) == 0 && sector[13] == FX_BOOT_GRANULE;
	return ec == EOS_CRC ? 0 : ec;
}

/* Take the boot sectors out of the free chain, wherever they are in it.
   *taken is how many were there.  With only_count, nothing is changed. */
static error_code take_boot_sectors(flex_path_id path, u_int *taken, int only_count)
{
	u_char current[FLEX_SECTOR_SIZE];
	u_char previous[FLEX_SECTOR_SIZE];
	u_int track = path->sir[FX_SIR_FREE];
	u_int sector = path->sir[FX_SIR_FREE + 1];
	u_int last_track = path->sir[FX_SIR_LAST_FREE];
	u_int last_sector = path->sir[FX_SIR_LAST_FREE + 1];
	u_int count = _fx_get2(&path->sir[FX_SIR_FREE_COUNT]);
	u_int previous_track = 0, previous_sector = 0;
	u_int limit = _fx_total_sectors(path) + 255;
	u_int seen;
	int have_previous = 0;
	error_code ec;

	*taken = 0;
	for (seen = 0; !(track == 0 && sector == 0) && seen < limit; seen++)
	{
		int is_last = track == last_track && sector == last_sector;
		u_int next_track, next_sector;

		ec = _fx_read_sector(path, track, sector, current);
		if (ec != 0)
		{
			return ec;
		}
		next_track = is_last ? 0 : current[0];
		next_sector = is_last ? 0 : current[1];

		if (is_boot_sector(track, sector) && only_count)
		{
			(*taken)++;
		}
		else if (is_boot_sector(track, sector))
		{
			(*taken)++;
			if (count > 0)
			{
				count--;
			}
			if (have_previous)
			{
				previous[0] = (u_char)next_track;
				previous[1] = (u_char)next_sector;
				ec = _fx_write_sector(path, previous_track, previous_sector, previous);
				if (ec != 0)
				{
					return ec;
				}
			}
			else
			{
				path->sir[FX_SIR_FREE] = (u_char)next_track;
				path->sir[FX_SIR_FREE + 1] = (u_char)next_sector;
			}
			if (is_last)
			{
				last_track = have_previous ? previous_track : 0;
				last_sector = have_previous ? previous_sector : 0;
			}
		}
		else
		{
			memcpy(previous, current, sizeof(previous));
			previous_track = track;
			previous_sector = sector;
			have_previous = 1;
		}

		if (is_last)
		{
			break;
		}
		track = next_track;
		sector = next_sector;
	}
	if (seen == limit)
	{
		return EOS_SECT;
	}
	if (only_count)
	{
		return 0;
	}

	_fx_set_free_chain(path, path->sir[FX_SIR_FREE], path->sir[FX_SIR_FREE + 1], last_track, last_sector, count);
	if (path->sir[FX_SIR_FREE] == 0 && path->sir[FX_SIR_FREE + 1] == 0)
	{
		_fx_set_free_chain(path, 0, 0, 0, 0, 0);
	}
	return 0;
}

static error_code write_filled(flex_path_id path, u_int track, u_int sector, const void *data, u_int size)
{
	u_char buffer[FLEX_SECTOR_SIZE];

	memset(buffer, 0xFF, sizeof(buffer));
	memcpy(buffer, data, size);
	return _fx_write_sector(path, track, sector, buffer);
}

static error_code write_boot_area(flex_path_id path)
{
	u_char buffer[FLEX_SECTOR_SIZE];
	u_int basic_size = (u_int)strlen(flex_bas);
	u_int i;
	error_code ec;

	ec = write_filled(path, FLEX_BOOT_TRACK, FX_BOOT_BASIC_SECTOR, flex_bas, basic_size);
	if (ec == 0)
	{
		ec = write_filled(path, FLEX_BOOT_TRACK, FLEX_BOOT_LOADER_SECTOR, loader, sizeof(loader));
	}
	if (ec != 0)
	{
		return ec;
	}

	/* Every granule is used but 32, which holds one sector of FLEX.BAS. */
	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < FX_BOOT_GRANULES; i++)
	{
		buffer[i] = i == FX_BOOT_GRANULE ? FX_GRANULE_USED + 1 : FX_GRANULE_USED;
	}
	ec = _fx_write_sector(path, FX_BOOT_DIR_TRACK, FX_BOOT_FAT_SECTOR, buffer);
	if (ec != 0)
	{
		return ec;
	}

	memset(buffer, 0xFF, sizeof(buffer));
	memset(buffer, 0, FX_DECB_ENTRY_SIZE);
	memcpy(buffer, "FLEX    BAS", 11);
	buffer[11] = FX_DECB_BASIC;
	buffer[12] = FX_DECB_ASCII;
	buffer[13] = FX_BOOT_GRANULE;
	_fx_put2(&buffer[14], basic_size);
	return _fx_write_sector(path, FX_BOOT_DIR_TRACK, FX_BOOT_DIR_SECTOR, buffer);
}

error_code _flex_putldr(const char *pathlist)
{
	flex_path_id path = NULL;
	u_int per_side, taken;
	int found;
	error_code ec = _fx_open_image(&path, pathlist, FAM_READ | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}
	if (!path->israw)
	{
		return _fx_close_image(path, EOS_BPNAM);
	}

	/* Disk BASIC reads only double density, 18 sectors a side.  As in
	   PUTLDR, the last track must be past the directory's. */
	per_side = side_sectors(path);
	if (path->max_track <= FX_BOOT_DIR_TRACK || (per_side != FLEX_DD_SECTORS && per_side != FLEX_DD_SECTORS - 1))
	{
		return _fx_close_image(path, EOS_IA);
	}

	/* A disk not set up before needs every boot sector free, and is left
	   as it was if they aren't. */
	ec = has_boot_area(path, &found);
	if (ec == 0 && !found)
	{
		ec = take_boot_sectors(path, &taken, 1);
		if (ec == 0 && taken != BOOT_SECTORS)
		{
			ec = EOS_FNA;
		}
	}
	if (ec == 0)
	{
		ec = take_boot_sectors(path, &taken, 0);
	}
	if (ec == 0)
	{
		ec = write_boot_area(path);
	}
	if (ec == 0)
	{
		ec = _fx_flush(path);
	}
	return _fx_close_image(path, ec);
}

error_code _flex_link(const char *pathlist)
{
	flex_path_id path = NULL, disk = NULL;
	u_char sector[FLEX_SECTOR_SIZE];
	u_int per_side, track, number, i, sectors;
	u_int start_track, start_sector;
	int found;
	error_code ec = _flex_open(&path, pathlist, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}
	if (path->israw)
	{
		_flex_close(path);
		return EOS_BPNAM;
	}

	/* The loader reads only side 0 of double density tracks. */
	per_side = side_sectors(path);
	start_track = track = path->entry.start_track;
	start_sector = number = path->entry.start_sector;
	sectors = _flex_entry_sectors(&path->entry);
	if (sectors == 0)
	{
		ec = EOS_IA;
	}
	for (i = 0; ec == 0 && i < sectors; i++)
	{
		if (track == 0 || number > per_side)
		{
			ec = EOS_IA;
			break;
		}
		if (i + 1 < sectors)
		{
			ec = _fx_follow(path, track, number, 1, &track, &number);
		}
	}
	_flex_close(path);
	if (ec != 0)
	{
		return ec;
	}

	/* The loader is written through a path to the whole disk. */
	ec = _fx_open_image(&disk, pathlist, FAM_READ | FAM_WRITE);
	if (ec != 0)
	{
		return ec;
	}
	ec = has_boot_area(disk, &found);
	if (ec == 0 && !found)
	{
		ec = EOS_FNA;
	}
	if (ec == 0)
	{
		ec = _fx_read_sector(disk, FLEX_BOOT_TRACK, FLEX_BOOT_LOADER_SECTOR, sector);
	}
	if (ec == 0)
	{
		sector[FLEX_BOOT_LINK_OFFSET] = (u_char)start_track;
		sector[FLEX_BOOT_LINK_OFFSET + 1] = (u_char)start_sector;
		ec = _fx_write_sector(disk, FLEX_BOOT_TRACK, FLEX_BOOT_LOADER_SECTOR, sector);
	}
	return _fx_close_image(disk, ec);
}

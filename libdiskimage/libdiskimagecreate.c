/********************************************************************
 * libdiskimagecreate.c - Create new disk images
 *
 * DMK and SDF tracks are written as double density, with sector numbers
 * starting at 1 and placed on the track according to the interleave.  The
 * track layout matches the images in unittest/testdata, which were made by
 * MAME, including the $00 that starts each track.
 ********************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libdiskimageint.h"

#define MFM_GAP_BYTE		0x4E
#define MFM_GAP1		32
#define MFM_GAP2		22
#define MFM_GAP3		24
#define MFM_GAP3_MIN		10
#define MFM_ID_SYNC_ZEROS	8
#define MFM_DATA_SYNC_ZEROS	12
#define MFM_ID_FIELD		7	/* mark, 4 ID bytes, CRC */

/* Everything recorded for a sector besides its data and the gap after it. */
#define MFM_SECTOR_OVERHEAD	(MFM_ID_SYNC_ZEROS + 3 + MFM_ID_FIELD + MFM_GAP2 + MFM_DATA_SYNC_ZEROS + 3 + 1 + 2)

#define DMK_MAX_CYLINDERS	255
#define DMK_MAX_SECTORS		64
#define SDF_MAX_CYLINDERS	80
#define SDF_MAX_SECTORS		31
#define JVC_MAX_SECTORS		255
#define JVC_HEADER_MAX		3
#define JVC_DEFAULT_SPT		18
#define JVC_DEFAULT_SIDES	1
#define JVC_DEFAULT_SIZE_CODE	1


error_code di_file_write(FILE *fd, const void *buffer, size_t size)
{
	return fwrite(buffer, 1, size, fd) == size ? 0 : EOS_WRITE;
}


/* Return the gap to leave after each sector on a track, or -1 if the
   sectors do not fit. */
static int mfm_gap3(u_int sectors, u_int sector_size)
{
	int gap = (int)((DI_MFM_TRACK_SIZE - MFM_GAP1) / sectors) - (int)(MFM_SECTOR_OVERHEAD + sector_size);

	if (gap < MFM_GAP3_MIN)
	{
		return -1;
	}

	return gap > MFM_GAP3 ? MFM_GAP3 : gap;
}


static u_char *put_bytes(u_char *p, u_char byte, int count)
{
	memset(p, byte, count);
	return p + count;
}


/* Find the sector number at each position on a track.  Successive numbers
   are interleave positions apart, or at the next free position after that. */
static void sector_order(u_int sectors, u_int interleave, u_char *order)
{
	u_int position = 0;
	u_int number;

	memset(order, 0, sectors);
	for (number = 1; number <= sectors; number++)
	{
		while (order[position] != 0)
		{
			position = (position + 1) % sectors;
		}
		order[position] = (u_char)number;
		position = (position + interleave) % sectors;
	}
}


/* Record a formatted track.  Returns the number of sectors, and for each
   the position in raw of its ID address mark and first data byte. */
int di_mfm_track(const di_layout *layout, u_int cylinder, u_int side, u_char *raw, int *id_pos, int *data_pos)
{
	u_int sectors = (cylinder == 0 && side == 0) ? layout->sectors_track0 : layout->sectors_per_track;
	int gap3 = mfm_gap3(sectors, layout->sector_size);
	u_char field[1 + DI_MAX_SECTOR_SIZE];
	u_char order[DI_MAX_TRACK_IDS];
	u_char *p = raw;
	unsigned short crc;
	u_int i;

	sector_order(sectors, layout->interleave, order);
	memset(raw, MFM_GAP_BYTE, DI_MFM_TRACK_SIZE);
	raw[0] = 0x00;
	p += MFM_GAP1;

	field[0] = 0xFB;
	memset(&field[1], layout->fill, layout->sector_size);

	for (i = 0; i < sectors; i++)
	{
		u_char id[5];

		id[0] = 0xFE;
		id[1] = (u_char)cylinder;
		id[2] = (u_char)side;
		id[3] = order[i];
		id[4] = (u_char)layout->size_code;

		p = put_bytes(p, 0x00, MFM_ID_SYNC_ZEROS);
		p = put_bytes(p, 0xA1, 3);
		id_pos[i] = (int)(p - raw);
		memcpy(p, id, sizeof(id));
		p += sizeof(id);
		crc = di_crc16(DI_CRC_INIT_DD, id, sizeof(id));
		*p++ = (u_char)(crc >> 8);
		*p++ = (u_char)crc;

		p = put_bytes(p, MFM_GAP_BYTE, MFM_GAP2);
		p = put_bytes(p, 0x00, MFM_DATA_SYNC_ZEROS);
		p = put_bytes(p, 0xA1, 3);
		*p++ = field[0];
		data_pos[i] = (int)(p - raw);
		memcpy(p, &field[1], layout->sector_size);
		p += layout->sector_size;
		crc = di_crc16(DI_CRC_INIT_DD, field, 1 + layout->sector_size);
		*p++ = (u_char)(crc >> 8);
		*p++ = (u_char)crc;

		p += gap3;
	}

	return (int)sectors;
}


/* Write the sectors of a raw or JVC image, after its header if any. */
static error_code write_linear(FILE *fd, const u_char *header, u_int header_size, long long sectors, const di_layout *layout)
{
	u_char sector[DI_MAX_SECTOR_SIZE];
	long long i;
	error_code ec = header_size ? di_file_write(fd, header, header_size) : 0;

	memset(sector, layout->fill, layout->sector_size);
	for (i = 0; i < sectors && ec == 0; i++)
	{
		ec = di_file_write(fd, sector, layout->sector_size);
	}

	return ec;
}


/* Record a short track 0 in LSN0, the only place a raw image can hold it.
   Only the fields needed to find it again are set. */
void di_raw_stamp_lsn0(u_char *sector, const di_layout *layout, long long total)
{
	lsn0_sect *lsn0 = (lsn0_sect *)sector;

	_int3((u_int)total, lsn0->dd_tot);
	_int1(layout->sectors_per_track, lsn0->dd_tks);
	_int2(layout->sectors_per_track, lsn0->dd_spt);
	_int2(layout->sectors_per_track, lsn0->dd_opt.m6809.pd_sct);
	_int2(layout->sectors_track0, lsn0->dd_opt.m6809.pd_t0s);
	memset(lsn0->dd_sync, 0, sizeof(lsn0->dd_sync));
}


static error_code create_raw(FILE *fd, const di_layout *layout, long long total)
{
	u_char sector[DI_RAW_SECTOR_SIZE];
	long long slots = (long long)layout->cylinders * layout->sides * layout->sectors_per_track;
	error_code ec = 0;

	/* Track 0 keeps all of its slots in the file, even when short. */
	if (layout->sectors_track0 != layout->sectors_per_track)
	{
		memset(sector, layout->fill, sizeof(sector));
		di_raw_stamp_lsn0(sector, layout, total);
		ec = di_file_write(fd, sector, sizeof(sector));
		slots--;
	}

	return ec != 0 ? ec : write_linear(fd, NULL, 0, slots, layout);
}


static error_code create_jvc(FILE *fd, const di_layout *layout, long long sectors)
{
	u_char header[JVC_HEADER_MAX];
	u_int header_size = 0;

	/* The header is only as long as it must be to differ from the defaults. */
	header[0] = (u_char)layout->sectors_per_track;
	header[1] = (u_char)layout->sides;
	header[2] = (u_char)layout->size_code;
	if (layout->size_code != JVC_DEFAULT_SIZE_CODE)
	{
		header_size = 3;
	}
	else if (layout->sides != JVC_DEFAULT_SIDES)
	{
		header_size = 2;
	}
	else if (layout->sectors_per_track != JVC_DEFAULT_SPT)
	{
		header_size = 1;
	}

	return write_linear(fd, header, header_size, sectors, layout);
}


/* Check what a format can represent. */
static error_code check_layout(diskimage_format format, const di_layout *layout, long long total)
{
	switch (format)
	{
	case DISKIMAGE_RAW:
		if (layout->sector_size != DI_RAW_SECTOR_SIZE ||
		    layout->sectors_per_track != DI_RAW_SECTORS_PER_TRACK ||
		    layout->sectors_track0 > layout->sectors_per_track ||
		    total > DI_RAW_MAX_SECTORS)
		{
			return EOS_IA;
		}
		break;

	case DISKIMAGE_JVC:
		/* Sector data must be a multiple of 256 bytes for a reader to
		   find the end of the header. */
		if (layout->sectors_per_track > JVC_MAX_SECTORS ||
		    layout->sectors_track0 != layout->sectors_per_track ||
		    total * layout->sector_size % 256 != 0)
		{
			return EOS_IA;
		}
		break;

	case DISKIMAGE_DMK:
	case DISKIMAGE_SDF:
	{
		u_int max_cylinders = format == DISKIMAGE_DMK ? DMK_MAX_CYLINDERS : SDF_MAX_CYLINDERS;
		u_int max_sectors = format == DISKIMAGE_DMK ? DMK_MAX_SECTORS : SDF_MAX_SECTORS;

		if (layout->cylinders > max_cylinders ||
		    (layout->interleave > 1 && layout->interleave >= layout->sectors_per_track) ||
		    layout->sectors_per_track > max_sectors || layout->sectors_track0 > max_sectors ||
		    mfm_gap3(layout->sectors_per_track, layout->sector_size) < 0 ||
		    mfm_gap3(layout->sectors_track0, layout->sector_size) < 0)
		{
			return EOS_IA;
		}
		break;
	}

	default:
		return EOS_IA;
	}

	return 0;
}


error_code _diskimage_create(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
			     u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
			     u_int interleave, u_char fill)
{
	di_layout layout;
	struct stat st;
	long long total;
	FILE *fd;
	error_code ec;

	if (pathname == NULL || (sides != 1 && sides != 2) || tracks == 0 || sectors_per_track == 0)
	{
		return EOS_IA;
	}

	layout.cylinders = tracks;
	layout.sides = sides;
	layout.sector_size = sector_size;
	layout.sectors_per_track = sectors_per_track;
	layout.sectors_track0 = sectors_track0 ? sectors_track0 : sectors_per_track;
	layout.interleave = interleave ? interleave : 1;
	layout.fill = fill;
	for (layout.size_code = 0; layout.size_code < 4; layout.size_code++)
	{
		if (sector_size == 128U << layout.size_code)
		{
			break;
		}
	}
	if (layout.size_code == 4)
	{
		return EOS_IA;
	}

	total = (long long)tracks * sides * sectors_per_track - sectors_per_track + layout.sectors_track0;
	ec = check_layout(format, &layout, total);
	if (ec != 0)
	{
		return ec;
	}

	if (stat(pathname, &st) == 0)
	{
		return EOS_FAE;
	}

	fd = fopen(pathname, "wb");
	if (fd == NULL)
	{
		return UnixToCoCoError(errno);
	}

	switch (format)
	{
	case DISKIMAGE_RAW:
		ec = create_raw(fd, &layout, total);
		break;
	case DISKIMAGE_JVC:
		ec = create_jvc(fd, &layout, total);
		break;
	case DISKIMAGE_DMK:
		ec = di_dmk_create(fd, &layout);
		break;
	default:
		ec = di_sdf_create(fd, &layout);
		break;
	}

	if (fclose(fd) != 0 && ec == 0)
	{
		ec = EOS_WRITE;
	}
	if (ec != 0)
	{
		remove(pathname);
	}

	return ec;
}

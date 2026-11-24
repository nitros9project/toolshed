/********************************************************************
 * libdiskimagecreate.c - Create new disk images
 *
 * DMK and SDF tracks are written as double density, with sector numbers
 * starting at 1 and placed on the track according to the interleave.  The
 * track layout matches the images in unittest/testdata, which were made by
 * MAME, including the $00 that starts each track.
 *
 * A track can instead start with an index mark, the layout of a disk
 * formatted by Dragon DOS as MAME records it.  The sectors are recorded the
 * same way, only later on the track.
 *
 * Track 0, or every track, can instead be single density, as FLEX needs.
 * Its bytes are recorded twice each, as DMK and SDF images hold single
 * density, and the track is half as long in the 6250 bytes of a double
 * density one.
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
#define MFM_GAP4A		54	/* with the $00 that starts the track */
#define MFM_INDEX_SYNC_ZEROS	9
#define MFM_INDEX_FIELD		4	/* 3 $C2 and the mark */
#define MFM_ID_SYNC_ZEROS	8
#define MFM_DATA_SYNC_ZEROS	12
#define MFM_ID_FIELD		7	/* mark, 4 ID bytes, CRC */

/* Everything recorded for a sector besides its data and the gap after it. */
#define MFM_SECTOR_OVERHEAD	(MFM_ID_SYNC_ZEROS + 3 + MFM_ID_FIELD + MFM_GAP2 + MFM_DATA_SYNC_ZEROS + 3 + 1 + 2)

#define FM_GAP_BYTE		0xFF
#define FM_TRACK_SIZE		(DI_MFM_TRACK_SIZE / 2)	/* bytes in a single density track */
#define FM_GAP1			16	/* IBM 5.25" gap 1, as a real Dragon Beta disk has */
#define FM_GAP2			11
#define FM_GAP3			10	/* what is left over becomes gap 4 */
#define FM_GAP3_MIN		8
#define FM_GAP4A		40
#define FM_SYNC_ZEROS		6
#define FM_INDEX_START		(FM_GAP4A + FM_SYNC_ZEROS + 1)	/* the gap, zeros and $FC index mark */
#define FM_SECTOR_OVERHEAD	(FM_SYNC_ZEROS + MFM_ID_FIELD + FM_GAP2 + FM_SYNC_ZEROS + 1 + 2)

/* The space left after the last sector is gap 4, and it is what absorbs the
   difference between the drive that wrote the disk and the one reading it.
   Gap 3 is kept at its IBM length so that gap 4 gets the remainder rather than
   the other way round: a track with no gap 4 is safe in an image but not on
   media.

   A 300 rpm track holds 3,125 bytes, the revolution being 200 ms, and OS-9 for
   the 6809 uses 256-byte sectors (dd_lsnsize is 1), so these are the layouts
   that arise:

	format      leader  gap 3  per sector  gap 4  margin
	10 x 256    16      10     299         119    3.8 %
	 9 x 256    16      10     299         418    13.4 %

   Under 2 % of a revolution is not reliable on physical media: a decent drive
   and diskette hold within 2.5 % of their speed, and the drive that wrote the
   disk may not be the one reading it.  Ten sectors is what FLEX and a Dragon
   Beta disk use, and it leaves 3.8 %. */

#define DMK_MAX_CYLINDERS	255
#define DMK_MAX_SECTORS		64
#define SDF_MAX_CYLINDERS	80
#define SDF_MAX_SECTORS		31
#define JVC_MAX_SECTORS		255
#define JVC_HEADER_MAX		4
#define JVC_DEFAULT_SPT		18
#define JVC_DEFAULT_SIDES	1
#define JVC_DEFAULT_SIZE_CODE	1
#define JVC_DEFAULT_FIRST_SECTOR	1
#define VDK_MAX_CYLINDERS	255
#define VDK_HEADER_SIZE		12
#define VDK_VERSION		0x10
#define VDK_SOURCE_ID		'S'	/* ToolShed */
#define VDK_SOURCE_VERSION	1


error_code di_file_write(FILE *fd, const void *buffer, size_t size)
{
	return fwrite(buffer, 1, size, fd) == size ? 0 : EOS_WRITE;
}


/* Return the gap to leave after each sector on a track, or -1 if the
   sectors do not fit. */
static int mfm_gap3(u_int sectors, const di_layout *layout)
{
	int start = MFM_GAP1 + (layout->index_mark ? MFM_GAP4A + MFM_INDEX_SYNC_ZEROS + MFM_INDEX_FIELD : 0);
	int gap = (int)((DI_MFM_TRACK_SIZE - start) / sectors) - (int)(MFM_SECTOR_OVERHEAD + layout->sector_size);

	if (gap < MFM_GAP3_MIN)
	{
		return -1;
	}

	return gap > MFM_GAP3 ? MFM_GAP3 : gap;
}


/* As mfm_gap3, for a single density track. */
static int fm_gap3(u_int sectors, const di_layout *layout)
{
	int start = FM_GAP1 + (layout->index_mark ? FM_INDEX_START : 0);
	int gap = (int)((FM_TRACK_SIZE - start) / sectors) - (int)(FM_SECTOR_OVERHEAD + layout->sector_size);

	if (gap < FM_GAP3_MIN)
	{
		return -1;
	}

	return gap > FM_GAP3 ? FM_GAP3 : gap;
}


static u_char *put_bytes(u_char *p, u_char byte, int count)
{
	memset(p, byte, count);
	return p + count;
}


/* Record single density bytes, each twice. */
static u_char *put_fm(u_char *p, const u_char *bytes, int count)
{
	int i;

	for (i = 0; i < count; i++)
	{
		*p++ = bytes[i];
		*p++ = bytes[i];
	}
	return p;
}


static u_char *put_fm_bytes(u_char *p, u_char byte, int count)
{
	return put_bytes(p, byte, count * 2);
}


/* Whether a track is recorded in single density. */
static int fm_track(const di_layout *layout, u_int cylinder, u_int side)
{
	if (layout->fm_all || (layout->fm_track0 && cylinder == 0))
	{
		return 1;
	}

	return layout->fm_track0_side0 && cylinder == 0 && side == 0;
}


/* The sectors on one side of a track. */
static u_int track_sectors(const di_layout *layout, u_int cylinder, u_int side)
{
	if (cylinder == 0 && (side == 0 || layout->fm_track0))
	{
		return layout->sectors_track0;
	}
	return layout->sectors_per_track;
}


/* The number of a track's first sector, less one. */
static int track_id_base(const di_layout *layout, u_int cylinder, u_int side)
{
	int base = (int)layout->first_sector[side] - 1;

	return (layout->span_sides && side == 1) ? base + (int)track_sectors(layout, cylinder, 0) : base;
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


/* Record a single density track, each byte twice, in DI_MFM_TRACK_SIZE bytes. */
static int fm_format_track(const di_layout *layout, u_int cylinder, u_int side, u_char *raw, int *id_pos, int *data_pos)
{
	u_int sectors = track_sectors(layout, cylinder, side);
	int base = track_id_base(layout, cylinder, side);
	int gap3 = fm_gap3(sectors, layout);
	u_char field[1 + DI_MAX_SECTOR_SIZE];
	u_char order[DI_MAX_TRACK_IDS];
	u_char *p = raw;
	u_char crc_bytes[2];
	unsigned short crc;
	u_int i;

	sector_order(sectors, layout->interleave, order);
	memset(raw, FM_GAP_BYTE, DI_MFM_TRACK_SIZE);
	if (layout->index_mark)
	{
		u_char mark = 0xFC;

		p = put_fm_bytes(p, FM_GAP_BYTE, FM_GAP4A);
		p = put_fm_bytes(p, 0x00, FM_SYNC_ZEROS);
		p = put_fm(p, &mark, 1);
	}
	p = put_fm_bytes(p, FM_GAP_BYTE, FM_GAP1);

	field[0] = 0xFB;
	memset(&field[1], layout->fill, layout->sector_size);

	for (i = 0; i < sectors; i++)
	{
		u_char id[5];

		id[0] = 0xFE;
		id[1] = (u_char)cylinder;
		id[2] = (u_char)side;
		id[3] = (u_char)(base + order[i]);
		id[4] = (u_char)layout->size_code;

		p = put_fm_bytes(p, 0x00, FM_SYNC_ZEROS);
		id_pos[i] = (int)(p - raw);
		p = put_fm(p, id, sizeof(id));
		crc = di_crc16(DI_CRC_INIT_SD, id, sizeof(id));
		crc_bytes[0] = (u_char)(crc >> 8);
		crc_bytes[1] = (u_char)crc;
		p = put_fm(p, crc_bytes, 2);

		p = put_fm_bytes(p, FM_GAP_BYTE, FM_GAP2);
		p = put_fm_bytes(p, 0x00, FM_SYNC_ZEROS);
		p = put_fm(p, field, 1);
		data_pos[i] = (int)(p - raw);
		p = put_fm(p, &field[1], (int)layout->sector_size);
		crc = di_crc16(DI_CRC_INIT_SD, field, 1 + layout->sector_size);
		crc_bytes[0] = (u_char)(crc >> 8);
		crc_bytes[1] = (u_char)crc;
		p = put_fm(p, crc_bytes, 2);

		p += gap3 * 2;
	}

	return (int)sectors;
}


/* Record a formatted track.  Returns the number of sectors, and for each
   the position in raw of its ID address mark and first data byte.  A
   single density track records each byte twice, and sets *double_density
   to 0. */
int di_format_track(const di_layout *layout, u_int cylinder, u_int side, u_char *raw, int *id_pos, int *data_pos,
		    int *double_density)
{
	u_int sectors = track_sectors(layout, cylinder, side);
	int base = track_id_base(layout, cylinder, side);
	int gap3 = mfm_gap3(sectors, layout);
	u_char field[1 + DI_MAX_SECTOR_SIZE];
	u_char order[DI_MAX_TRACK_IDS];
	u_char *p = raw;
	unsigned short crc;
	u_int i;

	*double_density = !fm_track(layout, cylinder, side);
	if (!*double_density)
	{
		return fm_format_track(layout, cylinder, side, raw, id_pos, data_pos);
	}

	sector_order(sectors, layout->interleave, order);
	memset(raw, MFM_GAP_BYTE, DI_MFM_TRACK_SIZE);
	raw[0] = 0x00;
	if (layout->index_mark)
	{
		p += MFM_GAP4A;
		p = put_bytes(p, 0x00, MFM_INDEX_SYNC_ZEROS);
		p = put_bytes(p, 0xC2, 3);
		*p++ = 0xFC;
	}
	p += MFM_GAP1;

	field[0] = 0xFB;
	memset(&field[1], layout->fill, layout->sector_size);

	for (i = 0; i < sectors; i++)
	{
		u_char id[5];

		id[0] = 0xFE;
		id[1] = (u_char)cylinder;
		id[2] = (u_char)side;
		id[3] = (u_char)(base + order[i]);
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
	header[3] = (u_char)layout->first_sector[0];
	if (layout->first_sector[0] != JVC_DEFAULT_FIRST_SECTOR)
	{
		header_size = 4;
	}
	else if (layout->size_code != JVC_DEFAULT_SIZE_CODE)
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


static error_code create_vdk(FILE *fd, const di_layout *layout, long long sectors)
{
	u_char header[VDK_HEADER_SIZE] = {'d', 'k', VDK_HEADER_SIZE, 0, VDK_VERSION, VDK_VERSION,
					  VDK_SOURCE_ID, VDK_SOURCE_VERSION};

	header[8] = (u_char)layout->cylinders;
	header[9] = (u_char)layout->sides;

	return write_linear(fd, header, sizeof(header), sectors, layout);
}


/* Check what a format can represent. */
static error_code check_layout(diskimage_format format, const di_layout *layout, long long total)
{
	u_int side, cylinder;

	/* A number for each side describes a double sided disk.  A disk with one
	   side has nowhere for the second number to apply, so it would not be the
	   disk that was asked for. */
	if (layout->sides == 1 && layout->first_sector[0] != layout->first_sector[1])
	{
		return EOS_SECT;
	}

	/* A sector number is one byte, so a track's numbers must not run past 255.
	   Track 0 can be short, so every cylinder is checked, not just the first. */
	for (side = 0; side < 2; side++)
	{
		for (cylinder = 0; cylinder < layout->cylinders; cylinder++)
		{
			int base = track_id_base(layout, cylinder, side);

			if (base + (int)track_sectors(layout, cylinder, side) > 255)
			{
				return EOS_SECT;
			}
		}
	}

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
		/* The header holds one first sector number, so a disk asked for a number
		   per side would express numbering the request did not ask for: not the
		   disk that was requested. */
		if (layout->first_sector[0] != layout->first_sector[1])
		{
			return EOS_SECT;
		}

		/* Sector data must be a multiple of 256 bytes for a reader to
		   find the end of the header. */
		if (layout->sectors_per_track > JVC_MAX_SECTORS ||
		    layout->sectors_track0 != layout->sectors_per_track ||
		    total * layout->sector_size % 256 != 0)
		{
			return EOS_IA;
		}
		break;

	case DISKIMAGE_VDK:
		/* The header cannot record any other shape, so readers assume this. */
		if (layout->cylinders > VDK_MAX_CYLINDERS ||
		    layout->sector_size != DI_RAW_SECTOR_SIZE ||
		    layout->sectors_per_track != DI_RAW_SECTORS_PER_TRACK ||
		    layout->sectors_track0 != layout->sectors_per_track)
		{
			return EOS_IA;
		}
		break;

	case DISKIMAGE_DMK:
	case DISKIMAGE_SDF:
	{
		u_int max_cylinders = format == DISKIMAGE_DMK ? DMK_MAX_CYLINDERS : SDF_MAX_CYLINDERS;
		u_int max_sectors = format == DISKIMAGE_DMK ? DMK_MAX_SECTORS : SDF_MAX_SECTORS;
		int track0_gap = fm_track(layout, 0, 0) ? fm_gap3(layout->sectors_track0, layout) :
						       mfm_gap3(layout->sectors_track0, layout);
		int track_gap = layout->fm_all ? fm_gap3(layout->sectors_per_track, layout) :
						 mfm_gap3(layout->sectors_per_track, layout);
		u_int track0_interleave_limit = (layout->fm_track0 || layout->fm_track0_side0) ?
			layout->sectors_track0 : layout->sectors_per_track;

		if (layout->cylinders > max_cylinders ||
		    (layout->interleave > 1 && layout->interleave >= layout->sectors_per_track) ||
		    (layout->interleave > 1 && layout->interleave >= track0_interleave_limit) ||
		    layout->sectors_per_track > max_sectors || layout->sectors_track0 > max_sectors ||
		    track_gap < 0 || track0_gap < 0 ||
		    (layout->span_sides && layout->sides == 2 &&
		     (layout->sectors_per_track * 2 > 255 || layout->sectors_track0 * 2 > 255)))
		{
			return EOS_IA;
		}
		break;
	}

	default:
		return EOS_IA;
	}

	/* Only DMK and SDF images record density. */
	if ((layout->fm_track0 || layout->fm_track0_side0 || layout->fm_all) &&
	    format != DISKIMAGE_DMK && format != DISKIMAGE_SDF)
	{
		return EOS_IA;
	}

	return 0;
}


static error_code create_image(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
			       u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
			       u_int interleave, const u_int first_sector[2], u_char fill, u_int flags)
{
	di_layout layout;
	struct stat st;
	long long total;
	FILE *fd;
	error_code ec;

	if (pathname == NULL || (sides != 1 && sides != 2) || tracks == 0 || sectors_per_track == 0 ||
	    (flags & ~(DISKIMAGE_CREATE_INDEX_MARK | DISKIMAGE_CREATE_FM_TRACK0 | DISKIMAGE_CREATE_FM_TRACK0_SIDE0 |
		       DISKIMAGE_CREATE_SPAN_SIDES | DISKIMAGE_CREATE_SINGLE_DENSITY)) != 0)
	{
		return EOS_IA;
	}

	layout.cylinders = tracks;
	layout.sides = sides;
	layout.sector_size = sector_size;
	layout.sectors_per_track = sectors_per_track;
	layout.sectors_track0 = sectors_track0 ? sectors_track0 : sectors_per_track;
	layout.interleave = interleave ? interleave : 1;
	layout.first_sector[0] = first_sector[0];
	layout.first_sector[1] = first_sector[1];
	layout.fill = fill;
	layout.index_mark = (flags & DISKIMAGE_CREATE_INDEX_MARK) != 0;
	layout.fm_track0 = (flags & DISKIMAGE_CREATE_FM_TRACK0) != 0;
	layout.fm_track0_side0 = (flags & DISKIMAGE_CREATE_FM_TRACK0_SIDE0) != 0;
	layout.span_sides = (flags & DISKIMAGE_CREATE_SPAN_SIDES) != 0;
	layout.fm_all = (flags & DISKIMAGE_CREATE_SINGLE_DENSITY) != 0;
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
	if (layout.fm_track0 && sides == 2)
	{
		total += (long long)layout.sectors_track0 - sectors_per_track;
	}
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
	case DISKIMAGE_VDK:
		ec = create_vdk(fd, &layout, total);
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


error_code _diskimage_create(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
			     u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
			     u_int interleave, u_char fill)
{
	static const u_int one_based[2] = { 1, 1 };

	return create_image(pathname, format, sides, tracks, sector_size, sectors_per_track,
			    sectors_track0, interleave, one_based, fill, 0);
}


error_code _diskimage_create_index_mark(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
					u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
					u_int interleave, u_char fill)
{
	static const u_int one_based[2] = { 1, 1 };

	return create_image(pathname, format, sides, tracks, sector_size, sectors_per_track,
			    sectors_track0, interleave, one_based, fill, DISKIMAGE_CREATE_INDEX_MARK);
}


error_code _diskimage_create_ex(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
				u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
				u_int interleave, const u_int first_sector[2], u_char fill, u_int flags)
{
	return create_image(pathname, format, sides, tracks, sector_size, sectors_per_track,
			    sectors_track0, interleave, first_sector, fill, flags);
}

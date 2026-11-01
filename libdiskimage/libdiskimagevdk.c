/********************************************************************
 * libdiskimagevdk.c - VDK image parsing
 *
 * The header gives tracks and sides, but not the sectors per track or
 * sector size.  Sectors are always 256 bytes, and the sectors per track
 * come from the file size, which must hold whole tracks.  That is
 * normally 18, which is all MAME's reader assumes.
 *
 * Of the flags, only write protect is honored; PC-Dragon's DOSCART.C also
 * defines lock bits.  The compression byte is not interpreted, as a
 * compressed file fails the size check and so is not taken for a VDK image.
 ********************************************************************/
#include <string.h>

#include "libdiskimageint.h"

#define VDK_MIN_HEADER_SIZE	12
#define VDK_SECTOR_SIZE		256
#define VDK_MAX_SECTORS		255


/* Return the sectors per track if this is a VDK image, or 0 if not. */
static u_int sectors_per_track(const u_char *header, long long file_size)
{
	u_int header_size = header[2] | (header[3] << 8);
	u_int tracks = header[8];
	u_int sides = header[9];
	long long track_bytes = (long long)tracks * sides * VDK_SECTOR_SIZE;
	long long data_size = file_size - header_size;
	long long sectors;

	if (memcmp(header, "dk", 2) != 0 || header_size < VDK_MIN_HEADER_SIZE)
	{
		return 0;
	}
	if (tracks == 0 || (sides != 1 && sides != 2) || data_size <= 0 || data_size % track_bytes != 0)
	{
		return 0;
	}

	sectors = data_size / track_bytes;

	return sectors <= VDK_MAX_SECTORS ? (u_int)sectors : 0;
}


int di_vdk_probe(const u_char *header, long long file_size)
{
	return sectors_per_track(header, file_size) != 0;
}


void di_vdk_load(diskimage_id disk, const u_char *header, long long file_size)
{
	disk->linear = 1;
	disk->cylinders = header[8];
	disk->sides = header[9];
	disk->write_protected = (header[DI_VDK_FLAGS_OFFSET] & DI_VDK_FLAG_WRITE_PROTECT) != 0;
	disk->sector_size = VDK_SECTOR_SIZE;
	disk->sectors_per_track = sectors_per_track(header, file_size);
	disk->first_sector = 1;
	disk->data_start = header[2] | (header[3] << 8);
	disk->stream_size = file_size - disk->data_start;
	disk->total_sectors = (u_int)(disk->stream_size / VDK_SECTOR_SIZE);
}

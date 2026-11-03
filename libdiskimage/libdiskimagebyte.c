/********************************************************************
 * libdiskimagebyte.c - Locations in the host image's container
 ********************************************************************/
#include <string.h>

#include "libdiskimageint.h"


/* Prefer an ID over overlapping data: an ID can be embedded in another
   sector's data on a damaged or deliberately unusual track. */
static void consider(long long offset, diskimage_byte_info *info, const diskimage_byte_region *region)
{
	if (offset >= region->start && offset < region->end)
	{
		if (info->region.kind == DISKIMAGE_BYTE_GAP || region->kind == DISKIMAGE_BYTE_ID)
			info->region = *region;
	}
	else if (region->end <= offset &&
		 (!info->has_previous || region->end > info->previous.end))
	{
		info->previous = *region;
		info->has_previous = 1;
	}
	else if (region->start > offset &&
		 (!info->has_next || region->start < info->next.start))
	{
		info->next = *region;
		info->has_next = 1;
	}
}


static void linear_byte(diskimage_id disk, long long offset, diskimage_byte_info *info)
{
	diskimage_byte_region *region = &info->region;
	u_int attribute = !disk->linear; /* only JVC with attributes uses a table here */
	u_int stride = disk->sector_size + attribute;
	long long slot = (offset - disk->data_start) / stride;
	long long position = (offset - disk->data_start) % stride;
	long long track = slot / disk->sectors_per_track;
	long long slots = disk->format == DISKIMAGE_RAW ?
		info->file_size / stride : (long long)disk->cylinders * disk->sides * disk->sectors_per_track;

	region->start = disk->data_start + slot * stride;
	region->end = region->start + stride;
	region->step = 1;
	if (slot >= slots)
	{
		region->kind = DISKIMAGE_BYTE_GAP;
		region->start = disk->data_start + slots * stride;
		region->end = info->file_size;
		return;
	}
	if (disk->raw_missing && slot >= disk->raw_track0 && slot < DI_RAW_SECTORS_PER_TRACK)
	{
		region->kind = DISKIMAGE_BYTE_UNUSED;
		region->start = (long long)disk->raw_track0 * stride;
		region->end = (long long)DI_RAW_SECTORS_PER_TRACK * stride;
		return;
	}
	region->sector.cylinder = (u_int)(track / disk->sides);
	region->sector.side = (u_int)(track % disk->sides);
	region->sector.sector = disk->first_sector + (u_int)(slot % disk->sectors_per_track);
	region->sector.size = disk->sector_size;
	region->logical = 1;
	if (attribute)
	{
		long long index = di_table_find(disk, region->sector.cylinder, region->sector.side, region->sector.sector);

		region->logical = index >= 0;
		if (index >= 0)
			region->sector.flags = disk->table[index].flags & DI_FLAG_PUBLIC;
		if (position == 0)
		{
			region->kind = DISKIMAGE_BYTE_ATTRIBUTE;
			region->end = region->start + 1;
			return;
		}
		region->start++;
	}
	region->kind = DISKIMAGE_BYTE_DATA;
}


error_code _diskimage_query_byte(diskimage_id disk, long long offset, diskimage_byte_info *info)
{
	u_int i;
	long long gap_start, gap_end;

	if (disk == NULL || info == NULL || offset < 0)
		return EOS_IA;
	memset(info, 0, sizeof(*info));
	if (fseeko(disk->fd, 0, SEEK_END) != 0 ||
	    (info->file_size = (long long)ftello(disk->fd)) < 0)
		return EOS_READ;
	if (offset >= info->file_size)
		return EOS_EOF;
	info->region.end = info->file_size;
	info->region.step = 1;
	if (offset < disk->data_start)
	{
		info->region.kind = DISKIMAGE_BYTE_HEADER;
		info->region.end = disk->data_start;
		return 0;
	}
	if (disk->linear || disk->format == DISKIMAGE_JVC)
	{
		linear_byte(disk, offset, info);
		return 0;
	}
	gap_start = disk->data_start;
	gap_end = info->file_size;
	if (disk->track_size > 0)
	{
		long long track = (offset - disk->data_start) / disk->track_size;
		long long start = disk->data_start + track * disk->track_size;

		gap_start = start + disk->track_header_size;
		gap_end = start + disk->track_size;

		if (offset < start + disk->track_header_size)
		{
			info->region.kind = DISKIMAGE_BYTE_TRACK_HEADER;
			info->region.start = start;
			info->region.end = start + disk->track_header_size;
			info->region.sector.cylinder = (u_int)(track / disk->sides);
			info->region.sector.side = (u_int)(track % disk->sides);
			return 0;
		}
	}
	for (i = 0; i < disk->id_count; i++)
	{
		const di_track_id *id = &disk->ids[i];
		const di_sector *sector = &id->sector;
		diskimage_byte_region region = {0};
		long long index = di_table_find(disk, sector->cylinder, sector->side, sector->id);

		region.sector.cylinder = sector->cylinder;
		region.sector.side = sector->side;
		region.sector.sector = sector->id;
		region.sector.size = sector->size;
		region.sector.flags = sector->flags & DI_FLAG_PUBLIC;
		region.logical = id->valid && index >= 0 && disk->table[index].data_off == sector->data_off;
		if (region.logical)
			region.sector.flags = disk->table[index].flags & DI_FLAG_PUBLIC;
		region.kind = DISKIMAGE_BYTE_ID;
		region.start = id->id_start;
		region.end = id->id_end;
		region.step = (u_int)((id->id_end - id->id_start) / 7);
		consider(offset, info, &region);
		if (sector->data_off == 0)
			continue;
		region.step = sector->step;
		region.kind = DISKIMAGE_BYTE_DATA_MARK;
		region.start = sector->data_off - sector->step;
		region.end = sector->data_off;
		consider(offset, info, &region);
		region.kind = DISKIMAGE_BYTE_DATA;
		region.start = region.end;
		region.end += (long long)sector->size * sector->step;
		consider(offset, info, &region);
		region.kind = DISKIMAGE_BYTE_CRC;
		region.start = region.end;
		region.end += 2 * sector->step;
		consider(offset, info, &region);
	}
	if (info->region.kind == DISKIMAGE_BYTE_GAP)
	{
		info->region.start = info->has_previous ? info->previous.end : disk->data_start;
		info->region.end = info->has_next ? info->next.start : info->file_size;
		if (info->region.start < gap_start)
			info->region.start = gap_start;
		if (info->region.end > gap_end)
			info->region.end = gap_end;
	}
	return 0;
}

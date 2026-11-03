/* Explain filesystem and container bytes in a Dragon DOS image. */
#include <util.h>
#include <dragondospath.h>
#include <limits.h>
#include "dgndos.h"

static char const *const helpMessage[] = {
	"Syntax: reveal <disk> <lsn>[:<offset>]\n",
	"        reveal -b <disk> <byte-offset>\n",
	"Usage:  Explain a location in a Dragon DOS disk image.\n",
	"        LSNs start at zero; sector byte offsets are 0 through 255.\n",
	"Options: -b  Absolute image byte offset, including container fields\n",
	NULL
};

static const char *container_field(diskimage_byte_kind kind)
{
	switch (kind)
	{
	case DISKIMAGE_BYTE_HEADER: return "image header";
	case DISKIMAGE_BYTE_TRACK_HEADER: return "track header / sector pointer table";
	case DISKIMAGE_BYTE_ID: return "ID field";
	case DISKIMAGE_BYTE_DATA_MARK: return "data address mark";
	case DISKIMAGE_BYTE_DATA: return "data field";
	case DISKIMAGE_BYTE_CRC: return "data CRC";
	case DISKIMAGE_BYTE_ATTRIBUTE: return "attribute byte";
	case DISKIMAGE_BYTE_UNUSED: return "unused slot on short track zero";
	default: return "inter-sector data";
	}
}


static void describe_container_byte(long long byteoff, const diskimage_byte_info *info)
{
	const diskimage_byte_region *r = &info->region;

	printf("Byte %lld is ", byteoff);
	if (r->kind == DISKIMAGE_BYTE_GAP)
	{
		printf("in inter-sector data");
		if (info->has_previous)
		{
			const diskimage_byte_region *p = &info->previous;
			printf(", %lld bytes after the last byte of sector %u's %s (cylinder %u, side %u)",
				byteoff - (p->end - 1), p->sector.sector, container_field(p->kind),
				p->sector.cylinder, p->sector.side);
		}
		if (info->has_next)
		{
			const diskimage_byte_region *n = &info->next;
			printf(", and %lld bytes before sector %u's %s (cylinder %u, side %u)",
				n->start - byteoff, n->sector.sector, container_field(n->kind),
				n->sector.cylinder, n->sector.side);
		}
		printf(".\n");
		return;
	}
	if (r->kind == DISKIMAGE_BYTE_HEADER || r->kind == DISKIMAGE_BYTE_UNUSED)
		printf("in the %s (byte %lld of %lld).\n", container_field(r->kind),
			byteoff - r->start, r->end - r->start);
	else if (r->kind == DISKIMAGE_BYTE_TRACK_HEADER)
		printf("in the %s for cylinder %u, side %u (byte %lld of %lld).\n",
			container_field(r->kind), r->sector.cylinder, r->sector.side,
			byteoff - r->start, r->end - r->start);
	else
	{
		printf("byte %lld of sector %u's %s (cylinder %u, side %u).\n",
			(byteoff - r->start) / r->step, r->sector.sector, container_field(r->kind),
			r->sector.cylinder, r->sector.side);
		if (r->step == 2)
			printf("This is copy %lld of a duplicated single-density byte.\n", (byteoff - r->start) % 2 + 1);
		if (!r->logical)
			printf("This sector is not a logical sector used by filesystem reads.\n");
	}
}




/* Public directory reads use the backup directory when necessary. */
static int read_entry(dragondos_path_id path, unsigned int index, dragondos_dir_entry *entry)
{
	_dragondos_seekdir(path, index);
	return _dragondos_readdir(path, entry);
}

static void describe_directory(const u_char *buffer, unsigned int sector, unsigned int offset)
{
	const dragondos_dir_entry *entry;
	unsigned int byte = offset % 25, start, extent;
	const char *field;
	char name[13];

	if (offset >= 250)
	{
		printf("This is reserved space after the directory entries.\n");
		return;
	}
	entry = (const dragondos_dir_entry *)(buffer + offset / 25 * 25);
	start = entry->flags & DRAGONDOS_FLAG_CONTINUATION ? 1 : 12;
	if (byte == 0) field = "flags (deleted, continued, end, protected, continuation)";
	else if (byte == 24) field = entry->flags & DRAGONDOS_FLAG_CONTINUED ?
		"next directory entry index" : "last sector byte count (0 means 256)";
	else if (byte < start) field = byte < 9 ? "filename" : "extension";
	else if ((entry->flags & DRAGONDOS_FLAG_CONTINUATION) && byte >= 22) field = "reserved space";
	else field = (byte - start) % 3 < 2 ? "extent starting LSN" : "extent sector count";
	_dragondos_ncpy_name(entry, name, sizeof(name));
	printf("This is byte %u of directory entry %u (%s): %s", byte,
		(sector - 2) * 10 + offset / 25,
		entry->flags & DRAGONDOS_FLAG_END ? "unused/end of directory" :
		entry->flags & DRAGONDOS_FLAG_DELETED ? "deleted/free" :
		entry->flags & DRAGONDOS_FLAG_CONTINUATION ? "continuation" : name, field);
	if (byte >= start && byte < (start == 1 ? 22 : 24))
	{
		extent = (byte - start) / 3;
		printf(" (extent %u, byte %u)", extent + 1, (byte - start) % 3);
	}
	printf(".\n");
}

static void describe_sector(dragondos_path_id path, unsigned int lsn, unsigned int offset)
{
	u_char buffer[256];
	unsigned int track = lsn / path->sectors_per_track;
	unsigned int side = lsn % path->sectors_per_track / 18;
	unsigned int sector = lsn % 18;
	unsigned int index;
	int incomplete = 0;
	error_code ec;

	printf("Examining LSN %u, byte %u (track %u, side %u, sector %u) of '%s'...\n",
		lsn, offset, track, side, sector + 1, path->imgfile);
	if (lsn >= path->total_sectors)
	{
		printf("This is past the logical end of the filesystem (%u sectors).\n", path->total_sectors);
		return;
	}
	ec = _dragondos_gs_sector(path, lsn, buffer);
	if (ec != 0 && ec != EOS_CRC)
	{
		printf("This sector is missing or unreadable (error %d).\n", ec);
		return;
	}
	if (ec == EOS_CRC) printf("Warning: this sector has a bad CRC; describing the recorded bytes.\n");
	if ((track == 20 || track == 16) && side == 0)
	{
		printf("This is the %s directory track.\n", track == 16 ? "backup" : "primary");
		if (sector < 2)
		{
			if (offset < 180)
			{
				unsigned int first = sector * 1440 + offset * 8, bit;
				printf("This is allocation bitmap byte %u, covering LSNs %u through %u ($%02X).\n",
					sector * 180 + offset, first, first + 7, buffer[offset]);
				for (bit = 0; bit < 8; bit++)
					printf("  LSN %u: %s\n", first + bit, first + bit >= path->total_sectors ?
						"outside filesystem" : buffer[offset] & (1 << bit) ? "UNALLOCATED (free)" : "ALLOCATED");
			}
			else if (sector == 0 && offset >= 252)
			{
				const char *fields[] = { "track count", "sectors per track (including both sides)",
					"track count complement", "sectors per track complement" };
				printf("This is the %s: %u ($%02X).\n", fields[offset - 252], buffer[offset], buffer[offset]);
			}
			else printf("This is reserved space after the allocation bitmap.\n");
		}
		else describe_directory(buffer, sector, offset);
		return;
	}
	for (index = 0; index < DRAGONDOS_MAX_DIR_ENTRIES; index++)
	{
		dragondos_dir_entry header, entry;
		u_char visited[DRAGONDOS_MAX_DIR_ENTRIES] = {0};
		unsigned int current = index, position = 0, match = 0, match_extent = 0;
		int found = 0, valid = 0;
		char name[13];

		ec = read_entry(path, index, &header);
		if (ec == EOS_EOF) break;
		if (ec != 0) { incomplete = 1; continue; }
		if (header.flags & (DRAGONDOS_FLAG_DELETED | DRAGONDOS_FLAG_CONTINUATION)) continue;
		_dragondos_ncpy_name(&header, name, sizeof(name));
		while (current < DRAGONDOS_MAX_DIR_ENTRIES && !visited[current])
		{
			dragondos_extent *extents;
			unsigned int count, i;
			visited[current] = 1;
			if (read_entry(path, current, &entry) != 0) break;
			if (entry.flags & DRAGONDOS_FLAG_DELETED) break;
			if (current != index && !(entry.flags & DRAGONDOS_FLAG_CONTINUATION)) break;
			extents = entry.flags & DRAGONDOS_FLAG_CONTINUATION ? entry.u.continuation.extent : entry.u.header.extent;
			count = entry.flags & DRAGONDOS_FLAG_CONTINUATION ? 7 : 4;
			for (i = 0; i < count; i++)
			{
				unsigned int first = int2(extents[i].lsn);
				if (extents[i].count && lsn >= first && lsn - first < extents[i].count)
				{
					found = 1;
					match = position + (lsn - first) * 256 + offset;
					match_extent = i + 1;
				}
				position += extents[i].count * 256;
			}
			if (!(entry.flags & DRAGONDOS_FLAG_CONTINUED)) { valid = 1; break; }
			current = entry.last_or_next;
		}
		if (!valid) incomplete = 1;
		if (found)
		{
			printf("This is file byte %u of \"%s\", in extent %u.\n", match, name, match_extent);
			if (!valid) printf("The directory continuation chain is corrupt or unreadable; the file size cannot be verified.\n");
			else
			{
				unsigned int size = position;
				if (size && entry.last_or_next) size -= 256 - entry.last_or_next;
				if (match >= size) printf("This is unused allocation space past the end of the file.\n");
			}
			return;
		}
	}
	/* The library caches the authoritative bitmap, using its backup if needed. */
	index = lsn / 1440;
	if (!path->track_loaded[index])
	{
		u_int free_sectors;
		_dragondos_gs_free(path, &free_sectors);
	}
	if (path->track_loaded[index])
		printf("The bitmap marks this sector %s; no live file owner was identified.\n",
			path->track[index][(lsn % 1440) / 8] & (1 << (lsn % 8)) ? "UNALLOCATED (free space)" : "ALLOCATED");
	else printf("No live file owner was identified; the allocation bitmap is unreadable.\n");
	if (incomplete) printf("Some directory entries or continuation chains were unreadable or corrupt.\n");
}

int dgndosreveal(int argc, char **argv)
{
	const char *image = NULL, *target = NULL;
	char *end, *pathlist;
	long long value;
	unsigned int lsn = 0, offset = 0;
	int byte_mode = 0, i;
	error_code ec;
	dragondos_path_id path;
	diskimage_byte_info info;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-b") == 0) byte_mode = 1;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0)
		{ show_help(helpMessage); return 0; }
		else if (argv[i][0] == '-') return EOS_IA;
		else if (image == NULL) image = argv[i];
		else if (target == NULL) target = argv[i];
		else return EOS_IA;
	}
	if (!image || !target) { show_help(helpMessage); return DGNDOS_USAGE_ERROR; }
	errno = 0;
	value = strtoll(target, &end, 0);
	if (errno || end == target || value < 0) return EOS_IA;
	if (!byte_mode)
	{
		if (value > UINT_MAX) return EOS_IA;
		lsn = (unsigned int)value;
		if (*end == ':')
		{
			const char *start = end + 1;
			errno = 0;
			value = strtoll(start, &end, 0);
			if (errno || end == start || value < 0 || value > 255) return EOS_IA;
			offset = (unsigned int)value;
		}
	}
	if (*end) return EOS_IA;
	/* Inspect container fields even when filesystem metadata is damaged. */
	if (byte_mode)
	{
		diskimage_id disk;
		ec = _diskimage_open(&disk, image, FAM_READ);
		if (ec != 0) return ec;
		ec = _diskimage_query_byte(disk, value, &info);
		_diskimage_close(disk);
		if (ec == EOS_EOF)
		{
			printf("Byte %lld is past the physical end of '%s' (%lld bytes).\n", value, image, info.file_size);
			return 0;
		}
		if (ec != 0) return ec;
		describe_container_byte(value, &info);
		if (info.region.kind != DISKIMAGE_BYTE_DATA || !info.region.logical) return 0;
	}
	pathlist = malloc(strlen(image) + 2);
	if (pathlist == NULL) return EOS_OM;
	ec = dgndos_disk_pathlist(image, pathlist, strlen(image) + 2);
	if (ec == 0) ec = _dragondos_open(&path, pathlist, FAM_READ);
	free(pathlist);
	if (ec != 0) return ec;
	if (byte_mode)
	{
		diskimage_geometry geometry;
		const diskimage_byte_region *r = &info.region;
		_diskimage_query_geometry(path->disk, &geometry);
		if (geometry.format == DISKIMAGE_RAW)
		{
			if (value / 256 > UINT_MAX) { _dragondos_close(path); return EOS_IA; }
			lsn = (unsigned int)(value / 256);
			offset = (unsigned int)(value % 256);
		}
		else
		{
			if (r->sector.size != 256 || r->sector.sector < 1 || r->sector.sector > 18 ||
				r->sector.side >= path->sectors_per_track / 18)
			{
				printf("This sector data is outside the Dragon DOS geometry.\n");
				_dragondos_close(path);
				return 0;
			}
			lsn = r->sector.cylinder * path->sectors_per_track + r->sector.side * 18 + r->sector.sector - 1;
			offset = (unsigned int)((value - r->start) / r->step);
		}
	}
	describe_sector(path, lsn, offset);
	_dragondos_close(path);
	return 0;
}

/* Explain filesystem and container bytes in a Disk BASIC image. */
#include <util.h>
#include <decbpath.h>
#include <limits.h>

static char const *const helpMessage[] = {
	"Syntax: reveal <disk> <lsn>[:<offset>]\n",
	"        reveal -b <disk> <byte-offset>\n",
	"Usage:  Explain a location in a Disk BASIC disk image.\n",
	"        LSNs start at zero; sectors contain 256 bytes.\n",
	"        Disk paths may include a drive: image,:drive[+offset].\n",
	"Options: -b  Absolute image byte offset (including container fields)\n",
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



static void describe_sector(decb_path_id path, unsigned int lsn, unsigned int offset)
{
	unsigned int track = lsn / 18, sector = lsn % 18 + 1;
	unsigned int granule, within, number;
	u_char buffer[256];
	int ended = 0, incomplete = 0;
	error_code ec;

	printf("Examining LSN %u, byte %u (track %u, sector %u) of '%s'...\n",
		lsn, offset, track, sector, path->imgfile);
	ec = _decb_gs_sector(path, track, sector, (char *)buffer);
	if (ec != 0)
	{
		printf("This sector is missing, unreadable, or past the physical end (error %d).\n", ec);
		return;
	}
	if (track == 17)
	{
		if (sector == 2)
		{
			unsigned int value = buffer[offset];
			if (offset >= (unsigned int)path->granule_count)
				printf("This is reserved space after the FAT.\n");
			else
			{
				printf("This is the FAT entry for granule %u ($%02X): ", offset, value);
				if (value == 0xFF) printf("UNALLOCATED (free space).\n");
				else if (value >= 0xC0 && value <= 0xC9)
					printf("last granule, with %u sectors used.\n", value - 0xC0);
				else if (value < (unsigned int)path->granule_count)
					printf("next granule is %u.\n", value);
				else printf("invalid granule link.\n");
			}
		}
		else if (sector >= 3 && sector <= 11)
		{
			decb_dir_entry *entry = (decb_dir_entry *)(buffer + offset / 32 * 32);
			u_char name[13];
			unsigned int byte = offset % 32;
			const char *field = byte < 8 ? "filename" : byte < 11 ? "extension" :
				byte == 11 ? "file type" : byte == 12 ? "ASCII/binary flag" :
				byte == 13 ? "first granule" : byte < 16 ? "last sector byte count" :
				byte == 16 ? "Color TDECB file status" : byte < 19 ? "logical record size" :
				byte == 19 ? "record terminator" : "reserved space";
			_decb_ncpy_name(*entry, name, sizeof(name));
			printf("This is byte %u of directory entry %u (%s): %s.\n", byte,
				(sector - 3) * 8 + offset / 32 + 1,
				entry->filename[0] == 0 ? "deleted/free" : entry->filename[0] == 0xFF ? "unused" : (char *)name, field);
		}
		else printf("This is reserved space on the directory track.\n");
		return;
	}
	granule = (track - (track > 17)) * 2 + (sector > 9);
	within = ((sector - 1) % 9) * 256 + offset;
	if (granule >= (unsigned int)path->granule_count)
	{
		printf("This is outside the configured %d-granule filesystem.\n", path->granule_count);
		return;
	}
	if (path->fat_ec != 0)
	{
		printf("Granule %u cannot be classified: the FAT is unreadable (error %d).\n", granule, path->fat_ec);
		return;
	}
	for (number = 3; number <= 11 && !ended; number++)
	{
		unsigned int i;
		if (_decb_gs_sector(path, 17, number, (char *)buffer) != 0)
		{
			incomplete = 1;
			continue;
		}
		for (i = 0; i < 8; i++)
		{
			decb_dir_entry *entry = (decb_dir_entry *)(buffer + i * 32);
			u_char visited[256] = {0}, name[13];
			unsigned int current = entry->first_granule, position = 0;
			if (entry->filename[0] == 0xFF) { ended = 1; break; }
			if (entry->filename[0] == 0) continue;
			while (current < (unsigned int)path->granule_count && !visited[current])
			{
				unsigned int next = path->FAT[current];
				visited[current] = 1;
				/* Match the same physical slots as _decb_gs_granule,
				 * including HDB-DOS images with alternate sectors. */
				unsigned int first = current * 9 + (current > 33 ? 18 : 0);
				unsigned int step = path->hdbdos_offset > 0 ? 2 : 1;
				if (lsn >= first && lsn - first < 9 * step && (lsn - first) % step == 0)
				{
					unsigned int used = 2304;
					within = (lsn - first) / step * 256 + offset;
					_decb_ncpy_name(*entry, name, sizeof(name));
					if (next >= 0xC0 && next <= 0xC9)
						used = next == 0xC0 ? 0 : (next - 0xC1) * 256 + int2(entry->last_sector_size);
					printf("This is byte %u of granule %u, owned by file \"%s\" (file byte %u).\n",
						within, current, name, position + within);
					if (within >= used) printf("This is unused allocation space past the end of the file.\n");
					if (next == 0xFF || (next >= (unsigned int)path->granule_count && !(next >= 0xC0 && next <= 0xC9)))
						printf("The file's granule chain is corrupt.\n");
					return;
				}
				current = next;
				position += 2304;
			}
		}
	}
	printf("Granule %u is %s and has no identified live file owner.%s\n", granule,
		path->FAT[granule] == 0xFF ? "UNALLOCATED (free space)" : "ALLOCATED",
		incomplete ? " Some directory sectors were unreadable." : "");
}

int decbreveal(int argc, char **argv)
{
	char *image = NULL, *target = NULL, *pathlist, *end;
	int i, byte_mode = 0;
	long long value;
	unsigned int lsn = 0, offset = 0;
	decb_path_id path;
	error_code ec;
	diskimage_byte_info info;
	diskimage_geometry geometry;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-b") == 0) byte_mode = 1;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0)
		{ show_help(helpMessage); return 0; }
		else if (image == NULL) image = argv[i];
		else if (target == NULL) target = argv[i];
		else return EOS_IA;
	}
	if (image == NULL || target == NULL) { show_help(helpMessage); return EOS_IA; }
	errno = 0;
	value = strtoll(target, &end, 0);
	if (errno || end == target || value < 0) return EOS_IA;
	if (!byte_mode)
	{
		if (value > INT_MAX / 18) return EOS_IA;
		lsn = (unsigned int)value;
		if (*end == ':')
		{
			char *start = end + 1;
			errno = 0;
			value = strtoll(start, &end, 0);
			if (errno || end == start || value < 0 || value > 255) return EOS_IA;
			offset = (unsigned int)value;
		}
	}
	if (*end != '\0') return EOS_IA;
	pathlist = malloc(strlen(image) + 2);
	if (pathlist == NULL) return EOS_OM;
	strcpy(pathlist, image);
	if (strchr(image, ',') == NULL) strcat(pathlist, ",");
	ec = _decb_open(&path, pathlist, FAM_READ);
	free(pathlist);
	if (ec != 0) return ec;
	_diskimage_query_geometry(path->disk, &geometry);
	if (path->granule_count < 1 || path->granule_count > 156)
	{ _decb_close(path); return EOS_IA; }
	if (byte_mode)
	{
		ec = _diskimage_query_byte(path->disk, value, &info);
		if (ec == EOS_EOF)
			printf("Byte %lld is past the physical end of '%s' (%lld bytes).\n", value, path->imgfile, info.file_size);
		else if (ec == 0)
		{
			describe_container_byte(value, &info);
			if (info.region.kind == DISKIMAGE_BYTE_DATA && info.region.logical)
			{
				long long slot = geometry.format == DISKIMAGE_RAW ? value / 256 :
					(long long)info.region.sector.cylinder * 18 + info.region.sector.sector - 1;
				slot -= path->disk_offset / 256;
				if (slot < 0 || slot > INT_MAX / 18 ||
					(geometry.format != DISKIMAGE_RAW && (info.region.sector.side != 0 ||
					info.region.sector.size != 256 || info.region.sector.sector < 1 || info.region.sector.sector > 18)))
					printf("This sector data is outside the selected Disk BASIC geometry.\n");
				else describe_sector(path, (unsigned int)slot, geometry.format == DISKIMAGE_RAW ?
					(unsigned int)(value % 256) : (unsigned int)((value - info.region.start) / info.region.step));
			}
		}
		if (ec == EOS_EOF) ec = 0;
	}
	else describe_sector(path, lsn, offset);
	_decb_close(path);
	return ec;
}

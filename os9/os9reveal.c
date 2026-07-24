/********************************************************************
 * reveal.c - os9 disk archaeology utility
 *
 * Given an LSN (and optionally a byte offset within that sector), or
 * an absolute byte offset into the image, "reveal" walks the LSN0
 * header, the allocation bitmap, and the live directory tree of an
 * OS-9 disk image and explains -- in plain English -- exactly what
 * lives at that location: a field of LSN0, a bitmap byte and the
 * cluster/LSN range it tracks, a file descriptor and which field of
 * it, a segment/extent entry, a directory entry (and which byte of
 * its name or LSN pointer), or a byte inside a file's data.
 *
 * Assumptions carried over from the rest of the os9 library:
 *   - os9_path_id already carries bps/spc/bitmap_bytes/cs and a
 *     parsed copy of LSN0 (path->lsn0) once _os9_open() succeeds.
 *   - read_lsn(path, lsn, buffer) reads exactly path->bps bytes of
 *     a single raw LSN into buffer, regardless of interpretation.
 *   - fd_stats/os9_dir_entry/lsn0_sect map byte-for-byte onto their
 *     on-disk sector layouts (as os9id.c already relies on).
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cocopath.h>


/* ------------------------------------------------------------------
 * types
 * ------------------------------------------------------------------ */

typedef struct
{
	unsigned int	target_lsn;	/* LSN being asked about */
	unsigned int	target_offset;	/* byte offset within that LSN */
} reveal_target;

struct field_desc
{
	size_t		offset;
	size_t		size;
	const char	*label;
};


/* ------------------------------------------------------------------
 * globals / help
 * ------------------------------------------------------------------ */

static char os9pathlist[256];

static char const *const helpMessage[] = {
	"Syntax: reveal {[<opts>]} <disk> <lsn>[:<offset>]\n",
	"        reveal -b <disk> <byte-offset>\n",
	"Usage:  Explain exactly what lives at a given LSN (and, optionally,\n",
	"        byte offset within that sector) on an os9 disk image -- a\n",
	"        piece of LSN0, a bitmap byte and the LSNs it tracks, a file\n",
	"        descriptor field, a directory entry, or file data -- by\n",
	"        name and structure, not just a raw hex dump.\n",
	"Options:\n",
	"  -b    Treat <offset> as an absolute byte offset from the start\n",
	"        of the disk image, rather than an LSN[:offset] pair.\n",
	NULL
};


/* ------------------------------------------------------------------
 * small helpers
 * ------------------------------------------------------------------ */

static char ord_buf[16];

static const char *ordinal(unsigned int n)
{
	const char *suffix = "th";

	if ((n % 100) < 11 || (n % 100) > 13)
	{
		switch (n % 10)
		{
		case 1:
			suffix = "st";
			break;
		case 2:
			suffix = "nd";
			break;
		case 3:
			suffix = "rd";
			break;
		default:
			suffix = "th";
			break;
		}
	}

	snprintf(ord_buf, sizeof(ord_buf), "%u%s", n, suffix);
	return (ord_buf);
}


/* Decode a fixed-length os9 name field (high bit terminates the
 * final character) into a plain C string.  Mirrors the pattern
 * os9id.c already uses for the LSN0 disk name. */
static u_char *decode_os9_name(const u_char *raw, size_t len)
{
	char *tmp = malloc(len + 1);

	memcpy(tmp, raw, len);
	tmp[len] = '\0';

	return (OS9StringToCString((u_char *) tmp));
}

static u_char *decode_entry_name(os9_dir_entry *e)
{
	return (decode_os9_name(e->name, D_NAMELEN));
}


/* ------------------------------------------------------------------
 * LSN0 field table
 * ------------------------------------------------------------------ */

static const struct field_desc lsn0_fields[] = {
	{ offsetof(lsn0_sect, dd_tot),     sizeof(((lsn0_sect *) 0)->dd_tot),     "total sector count (dd_tot)" },
	{ offsetof(lsn0_sect, dd_tks),     sizeof(((lsn0_sect *) 0)->dd_tks),     "track size (dd_tks)" },
	{ offsetof(lsn0_sect, dd_map),     sizeof(((lsn0_sect *) 0)->dd_map),     "bitmap byte count (dd_map)" },
	{ offsetof(lsn0_sect, dd_bit),     sizeof(((lsn0_sect *) 0)->dd_bit),     "sectors-per-cluster (dd_bit)" },
	{ offsetof(lsn0_sect, dd_dir),     sizeof(((lsn0_sect *) 0)->dd_dir),     "root directory LSN (dd_dir)" },
	{ offsetof(lsn0_sect, dd_own),     sizeof(((lsn0_sect *) 0)->dd_own),     "disk owner id (dd_own)" },
	{ offsetof(lsn0_sect, dd_att),     sizeof(((lsn0_sect *) 0)->dd_att),     "disk attributes (dd_att)" },
	{ offsetof(lsn0_sect, dd_dsk),     sizeof(((lsn0_sect *) 0)->dd_dsk),     "disk id (dd_dsk)" },
	{ offsetof(lsn0_sect, dd_fmt),     sizeof(((lsn0_sect *) 0)->dd_fmt),     "disk format flags (dd_fmt)" },
	{ offsetof(lsn0_sect, dd_spt),     sizeof(((lsn0_sect *) 0)->dd_spt),     "sectors-per-track (dd_spt)" },
	{ offsetof(lsn0_sect, dd_res),     sizeof(((lsn0_sect *) 0)->dd_res),     "reserved area (dd_res)" },
	{ offsetof(lsn0_sect, dd_bt),      sizeof(((lsn0_sect *) 0)->dd_bt),      "bootstrap LSN (dd_bt)" },
	{ offsetof(lsn0_sect, dd_bsz),     sizeof(((lsn0_sect *) 0)->dd_bsz),     "bootfile size (dd_bsz)" },
	{ offsetof(lsn0_sect, dd_dat),     sizeof(((lsn0_sect *) 0)->dd_dat),     "disk creation date (dd_dat)" },
	{ offsetof(lsn0_sect, dd_nam),     sizeof(((lsn0_sect *) 0)->dd_nam),     "disk name (dd_nam)" },
	{ offsetof(lsn0_sect, dd_opt),     sizeof(((lsn0_sect *) 0)->dd_opt),     "path descriptor options (dd_opt)" },
	{ offsetof(lsn0_sect, dd_res2),    sizeof(((lsn0_sect *) 0)->dd_res2),    "reserved (dd_res2)" },
	{ offsetof(lsn0_sect, dd_sync),    sizeof(((lsn0_sect *) 0)->dd_sync),    "OS-9/68K sync bytes (dd_sync, CRUZ)" },
	{ offsetof(lsn0_sect, dd_maplsn),  sizeof(((lsn0_sect *) 0)->dd_maplsn),  "bitmap sector LSN (dd_maplsn)" },
	{ offsetof(lsn0_sect, dd_lsnsize), sizeof(((lsn0_sect *) 0)->dd_lsnsize), "LSN size multiplier (dd_lsnsize)" },
	{ offsetof(lsn0_sect, dd_versid),  sizeof(((lsn0_sect *) 0)->dd_versid),  "LSN0 version id (dd_versid)" },
};

#define	N_LSN0_FIELDS	(sizeof(lsn0_fields) / sizeof(lsn0_fields[0]))

static const size_t DD_NAM_OFFSET = offsetof(lsn0_sect, dd_nam);
static const size_t DD_NAM_SIZE   = sizeof(((lsn0_sect *) 0)->dd_nam);


static void describe_lsn0_offset(lsn0_sect *l0, unsigned int offset)
{
	size_t i;
	u_char *diskname = decode_os9_name(l0->dd_nam, DD_NAM_SIZE);

	if (offset >= DD_NAM_OFFSET && offset < DD_NAM_OFFSET + DD_NAM_SIZE)
	{
		unsigned int charIndex = offset - (unsigned int) DD_NAM_OFFSET + 1;

		printf("This is byte %u of LSN0 -- specifically, this is the\n"
		       "%s character of the disk name (currently \"%s\").\n",
		       offset, ordinal(charIndex), diskname);
		return;
	}

	for (i = 0; i < N_LSN0_FIELDS; i++)
	{
		const struct field_desc *f = &lsn0_fields[i];

		if (offset >= f->offset && offset < f->offset + f->size)
		{
			printf("This is byte %u of LSN0, the header sector of disk\n"
			       "\"%s\" -- within the %s field (byte %u of %lu of it).\n",
			       offset, diskname, f->label,
			       offset - (unsigned int) f->offset + 1,
			       (unsigned long) f->size);
			return;
		}
	}

	printf("This is byte %u of LSN0, in an unnamed or reserved area.\n",
	       offset);
}


/* ------------------------------------------------------------------
 * file descriptor field table
 * ------------------------------------------------------------------ */

static const struct field_desc fd_fields[] = {
	{ offsetof(fd_stats, fd_att),   sizeof(((fd_stats *) 0)->fd_att),   "attributes field (fd_att)" },
	{ offsetof(fd_stats, fd_own),   sizeof(((fd_stats *) 0)->fd_own),   "owner id (fd_own)" },
	{ offsetof(fd_stats, fd_dat),   sizeof(((fd_stats *) 0)->fd_dat),   "last-modified date (fd_dat)" },
	{ offsetof(fd_stats, fd_lnk),   sizeof(((fd_stats *) 0)->fd_lnk),   "link count (fd_lnk)" },
	{ offsetof(fd_stats, fd_siz),   sizeof(((fd_stats *) 0)->fd_siz),   "file size in bytes (fd_siz)" },
	{ offsetof(fd_stats, fd_creat), sizeof(((fd_stats *) 0)->fd_creat), "creation date (fd_creat)" },
};

#define	N_FD_FIELDS	(sizeof(fd_fields) / sizeof(fd_fields[0]))


static void describe_fd_offset(const char *pathname, unsigned int offset)
{
	size_t i;
	size_t segArea = offsetof(fd_stats, fd_seg);

	if (offset == 0)
	{
		printf("This is the start of the file descriptor for file\n"
		       "\"%s\".\n", pathname);
		return;
	}

	if (offset >= segArea)
	{
		unsigned int segOffset = offset - (unsigned int) segArea;
		unsigned int segIndex  = segOffset / sizeof(fd_seg);
		unsigned int fieldOff  = segOffset % sizeof(fd_seg);

		if (segIndex >= NUM_SEGS)
		{
			printf("This is byte %u of the file descriptor for file\n"
			       "\"%s\", past its last possible segment/extent entry.\n",
			       offset, pathname);
			return;
		}

		if (fieldOff < 3)
			printf("This is byte %u of the file descriptor for file\n"
			       "\"%s\" -- the LSN field of its %s segment (extent) entry.\n",
			       offset, pathname, ordinal(segIndex + 1));
		else
			printf("This is byte %u of the file descriptor for file\n"
			       "\"%s\" -- the sector-count field of its %s segment\n"
			       "(extent) entry.\n",
			       offset, pathname, ordinal(segIndex + 1));
		return;
	}

	for (i = 0; i < N_FD_FIELDS; i++)
	{
		const struct field_desc *f = &fd_fields[i];

		if (offset >= f->offset && offset < f->offset + f->size)
		{
			printf("This is byte %u of the file descriptor for file\n"
			       "\"%s\", within its %s.\n",
			       offset, pathname, f->label);
			return;
		}
	}

	printf("This is byte %u of the file descriptor for file \"%s\".\n",
	       offset, pathname);
}


/* ------------------------------------------------------------------
 * bitmap
 * ------------------------------------------------------------------ */

static void describe_bitmap_offset(os9_path_id path, reveal_target *tgt)
{
	unsigned int byteInBitmap =
		(tgt->target_lsn - 1) * path->bps + tgt->target_offset;
	unsigned int clusterStart = byteInBitmap * 8;
	unsigned int lsnStart = clusterStart * path->spc;
	unsigned int lsnEnd   = lsnStart + (8 * path->spc) - 1;

	printf("This is byte %u of the allocation bitmap (LSN %u, %u bytes\n"
	       "into the bitmap area). Each bit marks one %u-sector cluster\n"
	       "as free or allocated, so this byte's 8 bits cover clusters\n"
	       "%u through %u -- that is, LSNs %u through %u.\n",
	       byteInBitmap, tgt->target_lsn,
	       (tgt->target_lsn - 1) * path->bps + tgt->target_offset,
	       path->spc, clusterStart, clusterStart + 7, lsnStart, lsnEnd);
}


/* ------------------------------------------------------------------
 * recursive directory-tree walk
 * ------------------------------------------------------------------ */

static int reveal_examine_fd(os9_path_id path, unsigned int fd_lsn,
			      const char *pathname, reveal_target *tgt)
{
	fd_stats	fd;
	int		i;
	unsigned int	prevBytes = 0;
	int		is_dir;

	if (fd_lsn == tgt->target_lsn)
	{
		describe_fd_offset(pathname, tgt->target_offset);
		return (1);
	}

	if (read_lsn(path, fd_lsn, &fd) != 0)
		return (0);

	is_dir = (fd.fd_att & FAP_DIR) ? 1 : 0;

	/* does the target LSN fall inside one of this FD's extents? */
	for (i = 0; i < NUM_SEGS; i++)
	{
		unsigned int seg_lsn = int3(fd.fd_seg[i].lsn);
		unsigned int seg_num = int2(fd.fd_seg[i].num);

		if (seg_num == 0)
			break;

		if (tgt->target_lsn >= seg_lsn &&
		    tgt->target_lsn < seg_lsn + seg_num)
		{
			unsigned int byteOffset = prevBytes +
				(tgt->target_lsn - seg_lsn) * path->bps +
				tgt->target_offset;

			if (is_dir)
			{
				unsigned int entriesPerSector =
					path->bps / sizeof(os9_dir_entry);
				unsigned int entryIndex =
					byteOffset / sizeof(os9_dir_entry);
				unsigned int entryByte =
					byteOffset % sizeof(os9_dir_entry);
				unsigned int localIndex =
					entryIndex % entriesPerSector;
				os9_dir_entry entbuf[64];
				u_char *entname = NULL;

				if (read_lsn(path, tgt->target_lsn, entbuf) == 0 &&
				    entbuf[localIndex].name[0] != 0)
					entname = decode_entry_name(&entbuf[localIndex]);

				printf("This is byte %u of the %s extent (segment)\n"
				       "of directory file \"%s\" --\n",
				       byteOffset, ordinal(i + 1), pathname);

				if (entryByte < D_NAMELEN)
					printf("it lands on the %s directory entry%s%s%s,\n"
					       "the %s character of that entry's filename.\n",
					       ordinal(entryIndex + 1),
					       entname ? " (\"" : "",
					       entname ? entname : (u_char *)"",
					       entname ? "\")" : "",
					       ordinal(entryByte + 1));
				else
					printf("it lands on the %s directory entry%s%s%s,\n"
					       "byte %u of the LSN pointer for that entry.\n",
					       ordinal(entryIndex + 1),
					       entname ? " (\"" : "",
					       entname ? entname : (u_char *)"",
					       entname ? "\")" : "",
					       entryByte - D_NAMELEN + 1);
			}
			else
			{
				printf("This is byte %u of file \"%s\" (the %s\n"
				       "byte overall), found in the %s segment/extent\n"
				       "of this file, which begins at LSN %u.\n",
				       byteOffset, pathname,
				       ordinal(byteOffset + 1),
				       ordinal(i + 1), seg_lsn);
			}

			return (1);
		}

		prevBytes += seg_num * path->bps;
	}

	/* not in this FD's own sectors -- if it's a directory, recurse */
	if (is_dir)
	{
		unsigned int fsize = int4(fd.fd_siz);
		unsigned int fileBytes = 0;

		for (i = 0; i < NUM_SEGS; i++)
		{
			unsigned int seg_lsn = int3(fd.fd_seg[i].lsn);
			unsigned int seg_num = int2(fd.fd_seg[i].num);
			unsigned int s;

			if (seg_num == 0)
				break;

			for (s = 0; s < seg_num && fileBytes < fsize; s++)
			{
				os9_dir_entry entbuf[64];
				int nEntries = path->bps / sizeof(os9_dir_entry);
				int e;

				if (read_lsn(path, seg_lsn + s, entbuf) != 0)
				{
					fileBytes += path->bps;
					continue;
				}

				for (e = 0; e < nEntries; e++)
				{
					char	childpath[512];
					u_char	*childname;
					unsigned int childlsn;

					if (entbuf[e].name[0] == 0)
						continue;

					childname = decode_entry_name(&entbuf[e]);
					childlsn = int3(entbuf[e].lsn);

					if (strcmp((char *)childname, ".") == 0 ||
					    strcmp((char *)childname, "..") == 0)
					{
						free(childname);
						continue;
					}

					if (strcmp(pathname, "/") == 0)
						snprintf(childpath, sizeof(childpath),
							 "/%s", childname);
					else
						snprintf(childpath, sizeof(childpath),
							 "%s/%s", pathname, childname);

					free(childname);

					if (reveal_examine_fd(path, childlsn,
							       childpath, tgt))
						return (1);
				}

				fileBytes += path->bps;
			}
		}
	}

	return (0);
}


/* ------------------------------------------------------------------
 * top-level dispatch
 * ------------------------------------------------------------------ */

static void reveal(os9_path_id path, reveal_target *tgt)
{
	lsn0_sect	*l0 = path->lsn0;
	unsigned int	bitmapSectors;
	unsigned int	rootDirLsn;

	printf("\nExamining LSN %u", tgt->target_lsn);
	if (tgt->target_offset != 0)
		printf(", byte %u", tgt->target_offset);
	printf(" of '%s'...\n\n", path->imgfile);

	if (tgt->target_lsn == 0)
	{
		describe_lsn0_offset(l0, tgt->target_offset);
		return;
	}

	bitmapSectors = (path->bitmap_bytes + path->bps - 1) / path->bps;

	if (tgt->target_lsn >= 1 && tgt->target_lsn <= bitmapSectors)
	{
		describe_bitmap_offset(path, tgt);
		return;
	}

	rootDirLsn = int3(l0->dd_dir);

	if (!reveal_examine_fd(path, rootDirLsn, "/", tgt))
	{
		printf("LSN %u doesn't currently map to any known filesystem\n"
		       "structure (LSN0, the bitmap, or a live file/directory).\n"
		       "It's most likely unallocated free space, or it falls in a\n"
		       "reserved/system area of the disk.\n",
		       tgt->target_lsn);
	}
}


/* ------------------------------------------------------------------
 * argument parsing / entry point
 * ------------------------------------------------------------------ */

static void parse_target(const char *arg, int byte_mode, os9_path_id path,
			  reveal_target *tgt)
{
	if (byte_mode)
	{
		unsigned long byteoff = strtoul(arg, NULL, 0);

		tgt->target_lsn    = (unsigned int) (byteoff / path->bps);
		tgt->target_offset = (unsigned int) (byteoff % path->bps);
	}
	else
	{
		char *copy = strdup(arg);
		char *colon = strchr(copy, ':');

		if (colon != NULL)
		{
			*colon = '\0';
			tgt->target_lsn    = (unsigned int) strtoul(copy, NULL, 0);
			tgt->target_offset = (unsigned int) strtoul(colon + 1, NULL, 0);
		}
		else
		{
			tgt->target_lsn    = (unsigned int) strtoul(copy, NULL, 0);
			tgt->target_offset = 0;
		}

		free(copy);
	}
}


int os9reveal(int argc, char *argv[])
{
	error_code	ec = 0;
	int		i;
	int		byte_mode = 0;
	char		*imgpath = NULL;
	char		*targetspec = NULL;
	os9_path_id	path;
	reveal_target	tgt;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			char *p;

			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);

				case 'b':
					byte_mode = 1;
					break;

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (0);
				}
			}
		}
		else if (imgpath == NULL)
		{
			imgpath = argv[i];
		}
		else if (targetspec == NULL)
		{
			targetspec = argv[i];
		}
	}

	if (imgpath == NULL || targetspec == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	strcpy(os9pathlist, imgpath);
	strcat(os9pathlist, ",@");

	ec = _os9_open(&path, os9pathlist, FAM_READ);
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n",
			argv[0], ec, os9pathlist);
		return (ec);
	}

	parse_target(targetspec, byte_mode, path, &tgt);

	reveal(path, &tgt);

	_os9_close(path);

	return (0);
}

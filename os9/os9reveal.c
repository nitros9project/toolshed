/********************************************************************
 * reveal.c - os9 disk archaeology utility
 *
 * Given an LSN (and optionally a byte offset within that sector), or
 * an absolute byte offset into the image, "reveal" walks the LSN0
 * header, the allocation bitmap, and the live directory tree of an
 * OS-9 disk image and explains -- in plain English -- exactly what
 * lives at that location: a field of LSN0, a bitmap byte and the
 * cluster/LSN range it tracks, a file descriptor and which field of
 * it, a segment entry, a directory entry (and which byte of
 * its name or LSN pointer), or a byte inside a file's data.
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <rbfutil.h>


/* ------------------------------------------------------------------
 * types
 * ------------------------------------------------------------------ */

/* full definition is further down, near get_boottrack_lsn(); a
 * pointer is all reveal_target needs to carry it that far */
// struct personality;

typedef struct
{
	unsigned int	target_lsn;	/* LSN being asked about */
	unsigned int	target_offset;	/* byte offset within that LSN */
	struct personality *hwtype;
	unsigned int	max_valid_lsn;
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
	"        byte offset within that sector) on an os9 disk image.\n",
	"Options:\n",
	"  -b    Treat <offset> as an absolute byte offset from the start\n",
	"        of the disk image, rather than an LSN[:offset] pair.\n",
	"  -d    Dragon disk\n",
	NULL
};

static long get_image_byte_size(os9_path_id path)
{
	struct stat st;

	if (path->fd == NULL)
		return (-1);

	if (fstat(fileno(path->fd), &st) != 0)
		return (-1);

	return ((long) st.st_size);
}


/* ------------------------------------------------------------------
 * small helpers
 * ------------------------------------------------------------------ */

/* Format a number with thousands separators (e.g. 164291 -> "164,291").
 * Rotates through a small pool of buffers for the same reason ordinal()
 * does -- so multiple calls in one printf() don't clobber each other. */
#define	NUM_NUM_BUFS	8
static char num_bufs[NUM_NUM_BUFS][32];
static int  num_buf_idx = 0;

static const char *format_num(unsigned long n)
{
	char	raw[24];
	char	*buf = num_bufs[num_buf_idx];
	int	rawlen, i, j;

	num_buf_idx = (num_buf_idx + 1) % NUM_NUM_BUFS;

	snprintf(raw, sizeof(raw), "%lu", n);
	rawlen = strlen(raw);

	for (i = 0, j = 0; i < rawlen; i++)
	{
		if (i > 0 && (rawlen - i) % 3 == 0)
			buf[j++] = ',';
		buf[j++] = raw[i];
	}
	buf[j] = '\0';

	return (buf);
}

#define	ORD_NUM_BUFS	8
static char ord_bufs[ORD_NUM_BUFS][24];
static int  ord_buf_idx = 0;

static const char *ordinal(unsigned int n)
{
	const char *suffix = "th";
	char *buf = ord_bufs[ord_buf_idx];

	ord_buf_idx = (ord_buf_idx + 1) % ORD_NUM_BUFS;

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

	snprintf(buf, 24, "%s%s", format_num(n), suffix);
	return (buf);
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

		printf("This is byte %s of LSN0 -- specifically, this is the %s character of the disk name (currently \"%s\").\n",
		       format_num(offset), ordinal(charIndex), diskname);
		return;
	}

	for (i = 0; i < N_LSN0_FIELDS; i++)
	{
		const struct field_desc *f = &lsn0_fields[i];

		if (offset >= f->offset && offset < f->offset + f->size)
		{
			printf("This is byte %s of LSN0, the header sector of disk \"%s\" -- within the %s field (byte %u of %lu of it).\n",
			       format_num(offset), diskname, f->label,
			       offset - (unsigned int) f->offset + 1,
			       (unsigned long) f->size);
			return;
		}
	}

	printf("This is byte %s of LSN0, in an unnamed or reserved area.\n",
	       format_num(offset));
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
		printf("This is the start of the file descriptor for file \"%s\".\n",
		       pathname);
		return;
	}

	if (offset >= segArea)
	{
		unsigned int segOffset = offset - (unsigned int) segArea;
		unsigned int segIndex  = segOffset / sizeof(fd_seg);
		unsigned int fieldOff  = segOffset % sizeof(fd_seg);

		if (segIndex >= NUM_SEGS)
		{
			printf("This is byte %s of the file descriptor for file \"%s\", past its last possible segment entry.\n",
			       format_num(offset), pathname);
			return;
		}

		if (fieldOff < 3)
			printf("This is byte %s of the file descriptor for file \"%s\" -- the LSN field of its %s segment entry.\n",
			       format_num(offset), pathname, ordinal(segIndex + 1));
		else
			printf("This is byte %s of the file descriptor for file \"%s\" -- the sector-count field of its %s segment entry.\n",
			       format_num(offset), pathname, ordinal(segIndex + 1));
		return;
	}

	for (i = 0; i < N_FD_FIELDS; i++)
	{
		const struct field_desc *f = &fd_fields[i];

		if (offset >= f->offset && offset < f->offset + f->size)
		{
			printf("This is byte %s of the file descriptor for file \"%s\", within its %s.\n",
			       format_num(offset), pathname, f->label);
			return;
		}
	}

	printf("This is byte %s of the file descriptor for file \"%s\".\n",
	       format_num(offset), pathname);
}


/* ------------------------------------------------------------------
 * boot track LSN calculation
 *
 * Pasted in from a newer os9gen.c, and wired up below (see the -d
 * option and the ALLOCATED-but-unmapped-LSN case in reveal()).
 * ------------------------------------------------------------------ */

static struct personality coco   = { 18 * 34 };
static struct personality dragon = { 2 };

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

	printf("This is byte %s of the allocation bitmap (LSN %s, %s bytes into the bitmap area). Each bit marks one %u-sector cluster as free or allocated, so this byte's 8 bits cover clusters %s through %s -- that is, LSNs %s through %s.\n",
	       format_num(byteInBitmap), format_num(tgt->target_lsn),
	       format_num(byteInBitmap), path->spc,
	       format_num(clusterStart), format_num(clusterStart + 7),
	       format_num(lsnStart), format_num(lsnEnd));
}


/* ------------------------------------------------------------------
 * recursive directory-tree walk
 * ------------------------------------------------------------------ */

static int reveal_examine_fd(os9_path_id path, unsigned int fd_lsn,
			      const char *pathname, reveal_target *tgt,
			      unsigned int depth)
{
	fd_stats	fd;
	int		i;
	unsigned int	prevBytes = 0;
	int		is_dir;

	/* A corrupted image can have a directory entry that points back
	 * at itself or an ancestor (accidentally or via disk damage --
	 * no malice required). Without a limit that's unbounded
	 * recursion and a stack-overflow crash rather than a clean
	 * error, since we don't track visited LSNs. A generous cap
	 * catches that without affecting any real directory tree, which
	 * won't nest anywhere close to this deep. */
#define	REVEAL_MAX_DEPTH	256
	if (depth > REVEAL_MAX_DEPTH)
	{
		fprintf(stderr,
			"reveal: directory nesting exceeds %d levels at\n"
			"\"%s\" -- stopping (the image may have a corrupt\n"
			"or cyclic directory structure).\n",
			REVEAL_MAX_DEPTH, pathname);
		return (0);
	}

	/* A corrupt directory entry or segment can claim an LSN that's
	 * beyond the physical file or the declared format size. The
	 * actual target LSN is already known to be in-bounds (reveal()
	 * checked that before calling us at all), so this can only ever
	 * reject a bogus reference, never the real target. */
	if (fd_lsn > tgt->max_valid_lsn)
		return (0);

	if (fd_lsn == tgt->target_lsn)
	{
		describe_fd_offset(pathname, tgt->target_offset);
		return (1);
	}

	/* read_lsn() returns the number of bytes read on success, not an
	 * error_code -- 0 (or negative) means the read failed. */
	if (read_lsn(path, fd_lsn, &fd) <= 0)
		return (0);

	is_dir = (fd.fd_att & FAP_DIR) ? 1 : 0;

	/* does the target LSN fall inside one of this FD's segements? */
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
				unsigned int localIndex;
				os9_dir_entry *entbuf;
				u_char *entname = NULL;

				/* A corrupted bps smaller than one directory
				 * entry would make entriesPerSector 0 and the
				 * modulo below a crash; bail cleanly instead. */
				if (entriesPerSector == 0)
				{
					printf("This is byte %s of the %s segment of directory file \"%s\", but this disk's sector size (%u bytes) looks too small to hold a directory entry -- the image may be corrupt.\n",
					       format_num(byteOffset), ordinal(i + 1),
					       pathname, path->bps);
					return (1);
				}

				localIndex = entryIndex % entriesPerSector;

				/* entbuf is sized to path->bps rather than a
				 * fixed array -- bps comes from this image's
				 * own (possibly corrupt) LSN0, and a fixed
				 * stack buffer sized for the common 256-byte
				 * sector would overflow on a bad/garbage bps
				 * larger than that. */
				entbuf = malloc(path->bps);

				if (entbuf != NULL)
				{
					/* read_lsn() returns bytes read, not
					 * an error_code -- > 0 means success. */
					if (read_lsn(path, tgt->target_lsn, entbuf) > 0 &&
					    entbuf[localIndex].name[0] != 0)
						entname = decode_entry_name(&entbuf[localIndex]);
				}

				if (entryByte < D_NAMELEN)
					printf("This is byte %s of the %s segment of directory file \"%s\" -- it lands on the %s directory entry%s%s%s, the %s character of that entry's filename.\n",
					       format_num(byteOffset), ordinal(i + 1), pathname,
					       ordinal(entryIndex + 1),
					       entname ? " (\"" : "",
					       entname ? entname : (u_char *)"",
					       entname ? "\")" : "",
					       ordinal(entryByte + 1));
				else
					printf("This is byte %s of the %s segment of directory file \"%s\" -- it lands on the %s directory entry%s%s%s, byte %u of the LSN pointer for that entry.\n",
					       format_num(byteOffset), ordinal(i + 1), pathname,
					       ordinal(entryIndex + 1),
					       entname ? " (\"" : "",
					       entname ? entname : (u_char *)"",
					       entname ? "\")" : "",
					       entryByte - D_NAMELEN + 1);

				if (entname != NULL)
					free(entname);
				if (entbuf != NULL)
					free(entbuf);
			}
			else
			{
				printf("This is byte %s of file \"%s\" (the %s byte overall), found in the %s segment of this file, which begins at LSN %s.\n",
				       format_num(byteOffset), pathname,
				       ordinal(byteOffset + 1),
				       ordinal(i + 1), format_num(seg_lsn));
			}

			return (1);
		}

		prevBytes += seg_num * path->bps;
	}

	/* not in this FD's own sectors -- if it's a directory, recurse.
	 * A directory's last allocated sector is often only partially
	 * used -- OS-9 allocates directories in whole clusters, and the
	 * unused tail of that final sector is just leftover disk content
	 * (frequently stale bytes from whatever file previously owned
	 * that sector), not zeroed. So we bound the scan by fd_siz -- the
	 * directory's actual byte length -- rather than reading every
	 * entry slot in every allocated sector; otherwise we walk off
	 * the end of the real entries into that stale tail and start
	 * "recursing" into garbage LSNs as if they were real children. */
	if (is_dir)
	{
		unsigned int fsize = int4(fd.fd_siz);
		unsigned int totalEntries = fsize / sizeof(os9_dir_entry);
		unsigned int entriesSeen = 0;
		int nEntries = path->bps / sizeof(os9_dir_entry);

		/* entbuf is sized to path->bps rather than a fixed array --
		 * bps comes from this image's own (possibly corrupt) LSN0,
		 * and a fixed stack buffer sized for the common 256-byte
		 * sector would overflow on a bad/garbage bps larger than
		 * that. Allocated once and reused for every sector read. */
		os9_dir_entry *entbuf = (nEntries > 0) ? malloc(path->bps) : NULL;

		if (entbuf == NULL)
			return (0);

		for (i = 0; i < NUM_SEGS && entriesSeen < totalEntries; i++)
		{
			unsigned int seg_lsn = int3(fd.fd_seg[i].lsn);
			unsigned int seg_num = int2(fd.fd_seg[i].num);
			unsigned int s;

			if (seg_num == 0)
				break;

			for (s = 0; s < seg_num && entriesSeen < totalEntries; s++)
			{
				int e;

				/* read_lsn() returns bytes read, not an
				 * error_code -- <= 0 means the read failed. */
				if (read_lsn(path, seg_lsn + s, entbuf) <= 0)
				{
					/* can't verify this sector's entries;
					 * skip it but keep the entry count in
					 * sync so we don't overrun elsewhere */
					entriesSeen += nEntries;
					continue;
				}

				for (e = 0; e < nEntries && entriesSeen < totalEntries;
				     e++, entriesSeen++)
				{
					char	*childpath;
					size_t	pathlen;
					u_char	*childname;
					unsigned int childlsn;
					int	found;

					if (entbuf[e].name[0] == 0)
						continue;

					childname = decode_entry_name(&entbuf[e]);
					childlsn = int3(entbuf[e].lsn);

					if (strcmp((char *) childname, ".") == 0 ||
					    strcmp((char *) childname, "..") == 0)
					{
						free(childname);
						continue;
					}

					/* pathname + '/' + childname + '\0' --
					 * sized exactly, no fixed path limit */
					pathlen = strlen(pathname) +
						strlen((char *) childname) + 2;
					childpath = malloc(pathlen);

					if (childpath == NULL)
					{
						free(childname);
						continue;
					}

					if (strcmp(pathname, "/") == 0)
						snprintf(childpath, pathlen,
							 "/%s", childname);
					else
						snprintf(childpath, pathlen,
							 "%s/%s", pathname, childname);

					free(childname);

					/* skip a clearly bogus child reference
					 * (out past both the physical file and
					 * the declared format size) rather than
					 * wasting a read attempt on it */
					if (childlsn > tgt->max_valid_lsn)
					{
						free(childpath);
						continue;
					}

					found = reveal_examine_fd(path, childlsn,
								   childpath, tgt,
								   depth + 1);
					free(childpath);

					if (found)
					{
						free(entbuf);
						return (1);
					}
				}
			}
		}

		free(entbuf);
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
	unsigned int	logicalTotalSectors;
	unsigned int	physSectors = 0;
	unsigned int	bound;
	long		imageBytes;
	unsigned long	targetByteOffset;

	printf("\nExamining LSN %s", format_num(tgt->target_lsn));
	if (tgt->target_offset != 0)
		printf(", byte %s", format_num(tgt->target_offset));
	printf(" of '%s'...\n\n", path->imgfile);

	logicalTotalSectors = int3(l0->dd_tot);
	imageBytes = get_image_byte_size(path);
	targetByteOffset = (unsigned long) tgt->target_lsn * path->bps +
		tgt->target_offset;

	/* logical end of the formatted filesystem -- LSN0 itself says
	 * this disk doesn't contain this LSN at all (dd_tot). This is
	 * the authoritative bound regardless of the container file's
	 * physical size. */
	if (logicalTotalSectors > 0 && tgt->target_lsn >= logicalTotalSectors)
	{
		printf("LSN %s is past the logical end of the formatted filesystem -- LSN0 declares only %s total sectors (dd_tot). This is most likely trailing padding, space past a smaller format than the container file, or a corrupt dd_tot value.\n",
		       format_num(tgt->target_lsn), format_num(logicalTotalSectors));
		return;
	}

	/* physical end of the container file. A host image is allowed
	 * to be shorter than the sectors LSN0 declares -- that's normal
	 * for a dynamically-growing/sparse image, and just means those
	 * trailing sectors haven't been referred to (allocated) yet. It
	 * only becomes a real problem if the bitmap disagrees -- i.e.
	 * something in this filesystem claims a sector that the file
	 * doesn't actually contain. */
	if (imageBytes >= 0 && targetByteOffset >= (unsigned long) imageBytes)
	{
		int allocated = _os9_ckbit(path->bitmap, tgt->target_lsn);

		printf("LSN %s is past the physical end of the image file '%s' (which is %s bytes / %s sectors long).",
		       format_num(tgt->target_lsn), path->imgfile,
		       format_num((unsigned long) imageBytes),
		       format_num((unsigned long) (path->bps ?
						    (unsigned long) imageBytes / path->bps : 0)));

		if (allocated)
			printf(" The bitmap marks it ALLOCATED, though -- something in this filesystem references a sector the container file doesn't actually contain. That's inconsistent, and likely means the image is truncated or corrupt.\n");
		else
			printf(" The bitmap marks it UNALLOCATED, so this is simply space that hasn't been grown into yet -- expected for a dynamically-growing image, not a problem.\n");

		return;
	}

	/* Bound used deeper in the walk (reveal_examine_fd) to skip
	 * obviously-corrupt LSNs -- e.g. a directory entry or segment
	 * pointing somewhere that can't exist -- without wasting a read
	 * attempt on them. 0xFFFFFFFF means "no bound known". */
	if (imageBytes >= 0 && path->bps > 0)
		physSectors = (unsigned int) ((unsigned long) imageBytes / path->bps);

	bound = 0xFFFFFFFFu;
	if (physSectors > 0 && physSectors < bound)
		bound = physSectors;
	if (logicalTotalSectors > 0 && logicalTotalSectors < bound)
		bound = logicalTotalSectors;
	tgt->max_valid_lsn = (bound == 0xFFFFFFFFu) ? bound : bound - 1;

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

	if (!reveal_examine_fd(path, rootDirLsn, "/", tgt, 0))
	{
		int allocated = _os9_ckbit(path->bitmap, tgt->target_lsn);

		printf("LSN %s doesn't currently map to any known filesystem structure (LSN0, the bitmap, or a live file/directory).\n",
		       format_num(tgt->target_lsn));

		if (allocated)
		{
			int startlsn = 0;
			error_code btec = get_boottrack_lsn(*l0, tgt->hwtype, &startlsn, 0);
			int in_boot_track = 0;

			if (btec == 0)
			{
				/* Boot track is exactly one track long,
				 * starting at startlsn -- same pd_sct fetch
				 * get_boottrack_lsn() itself uses. */
				int is_osk = (memcmp(l0->dd_sync, "Cruz", 4) == 0);
				u_char *pd_sct = is_osk ? l0->dd_opt.m68k.pd_sct
							 : l0->dd_opt.m6809.pd_sct;
				unsigned int spt = int2(pd_sct);

				in_boot_track = (spt > 0 &&
					tgt->target_lsn >= (unsigned int) startlsn &&
					tgt->target_lsn < (unsigned int) startlsn + spt);
			}

			if (in_boot_track)
				printf("The bitmap marks it as ALLOCATED, though -- it falls within the boot track (starting at LSN %s), so it likely belongs to boot code rather than a file.\n",
				       format_num((unsigned int) startlsn));
			else
				printf("The bitmap marks it as ALLOCATED, though -- it likely belongs to a deleted file, a reserved area, or something this walk didn't reach.\n");
		}
		else
			printf("The bitmap marks it as UNALLOCATED -- it's free space.\n");
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
	struct personality *hwtype = &coco;

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

				case 'd':
					hwtype = &dragon;
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

	/* A corrupted LSN0 could yield a bytes-per-sector of 0, which
	 * would otherwise crash later on division/modulo (bitmap sector
	 * count, -b byte-offset math). Bail out cleanly instead. */
	if (path->bps == 0)
	{
		fprintf(stderr,
			"%s: '%s' reports 0 bytes per sector -- LSN0 looks\n"
			"corrupt or unreadable, refusing to proceed.\n",
			argv[0], os9pathlist);
		_os9_close(path);
		return (-1);
	}

	/* Likewise a missing LSN0/bitmap (e.g. _os9_open succeeded on a
	 * badly damaged image without fully populating these) would
	 * crash reveal() on the first dereference. */
	if (path->lsn0 == NULL || path->bitmap == NULL)
	{
		fprintf(stderr,
			"%s: '%s' didn't yield a usable LSN0/bitmap --\n"
			"refusing to proceed on a possibly corrupt image.\n",
			argv[0], os9pathlist);
		_os9_close(path);
		return (-1);
	}

	parse_target(targetspec, byte_mode, path, &tgt);
	tgt.hwtype = hwtype;

	reveal(path, &tgt);

	_os9_close(path);

	return (0);
}

/********************************************************************
 * librbfformat.c - RBF disk format
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>

#include "cocotypes.h"
#include "cocosys.h"

#include "os9path.h"
#include "nativepath.h"


#define DragonBootSize	16	/* Size of Dragon boot area in sectors */


/* Where the sectors of a new disk go: a native file in LSN order, or a disk
   image's own sectors. */
typedef struct
{
	native_path_id native;
	os9_path_id image;
	long long written;
	long long t0sBytes;
	u_int gapBytes;
	error_code ec;
} format_output;


/*
 * write_lsns()
 *
 * Write the next bytes of the disk in LSN order. Track 0 keeps all of its file
 * slots on a short track 0 disk, so fill the unused ones on reaching T0S.
 */

static void write_lsns(format_output *out, void *buffer, u_int size)
{
	char *buf_ptr = buffer;

	if (out->image != NULL)
	{
		if (out->ec == 0)
		{
			out->ec = _os9_lsn_put(out->image, 0, (u_int) out->written, buffer, size);
		}
		out->written += size;
		return;
	}

	while (size > 0)
	{
		u_int run = size;

		if (out->written < out->t0sBytes && run > out->t0sBytes - out->written)
		{
			run = out->t0sBytes - out->written;
		}

		if (out->written == out->t0sBytes && out->gapBytes > 0)
		{
			char *gap = (char *) malloc(out->gapBytes);

			if (gap != NULL)
			{
				u_int gapSize = out->gapBytes;

				memset(gap, 0xE5, out->gapBytes);
				_native_write(out->native, gap, &gapSize);
				free(gap);
			}
		}

		_native_write(out->native, buf_ptr, &run);
		buf_ptr += run;
		out->written += run;
		size -= run;
	}
}


/* Make a new image of the whole disk, and a path to reach its LSNs. */
static error_code open_new_image(struct _os9_path_id *image, char *pathname, diskimage_format format,
				 int tracks, int sectorsPerTrack, int sectorsTrack0, int heads,
				 int sectorSize, int density, int interleave)
{
	diskimage_geometry geometry;
	error_code ec;

	/* These formats only record double density here. */
	if (density == 0 && (format == DISKIMAGE_DMK || format == DISKIMAGE_SDF))
	{
		return EOS_IA;
	}

	ec = _diskimage_create(pathname, format, heads, tracks, sectorSize, sectorsPerTrack,
			       sectorsTrack0 == sectorsPerTrack ? 0 : sectorsTrack0, interleave, 0xE5);
	if (ec != 0)
	{
		return ec;
	}

	memset(image, 0, sizeof(*image));
	image->imgfile = pathname;
	image->mode = FAM_READ | FAM_WRITE;
	image->bps = sectorSize;
	image->spt = sectorsPerTrack;
	image->t0s = sectorsTrack0;
	image->sides = heads;
	image->first_sector = 1;

	ec = _diskimage_open(&image->disk, pathname, FAM_READ | FAM_WRITE);
	if (ec != 0)
	{
		return ec;
	}

	/* A JVC image with nothing to record needs no header, and opens as raw. */
	ec = _diskimage_query_geometry(image->disk, &geometry);
	image->raw_image = ec == 0 && geometry.format == DISKIMAGE_RAW;

	return ec;
}


error_code _os9_format(char *pathname, int os968k, int tracks,
		       int sectorsPerTrack, int sectorsTrack0,
			   int heads, int sectorSize,
		       int *clusterSize, char *diskName,
		       int sectorAllocationSize, int tpi, int density,
		       int formatEntire, int isDragon, int isHDD, int interleave,
		       unsigned int *totalSectors, unsigned int *totalBytes)
{
	return _os9_format_image(pathname, DISKIMAGE_RAW, os968k, tracks, sectorsPerTrack, sectorsTrack0,
				 heads, sectorSize, clusterSize, diskName, sectorAllocationSize, tpi,
				 density, formatEntire, isDragon, isHDD, interleave, totalSectors, totalBytes);
}


error_code _os9_format_image(char *pathname, diskimage_format format, int os968k, int tracks,
			     int sectorsPerTrack, int sectorsTrack0,
			     int heads, int sectorSize,
			     int *clusterSize, char *diskName,
			     int sectorAllocationSize, int tpi, int density,
			     int formatEntire, int isDragon, int isHDD, int interleave,
			     unsigned int *totalSectors, unsigned int *totalBytes)
{
	error_code ec = 0;
	format_output out;
	struct _os9_path_id image;
	lsn0_sect s0;
	unsigned int sectorsLeft;
	int b;
	unsigned int sectorsToAlloc = 0;
	unsigned int bitmapSectors, bitmapBytes;
	unsigned int rootSects;

	/* An LSN at or past T0S is offset by the sectors track 0 is missing, which
	 * needs at least LSN0 on track 0 and no more sectors than a full track.
	 * Check before the create below truncates any existing image.
	 */
	if (sectorsTrack0 < 1 || sectorsTrack0 > sectorsPerTrack)
	{
		fprintf(stderr, "Sectors on track 0 (%d) must be from 1 to sectors per track (%d)\n",
			sectorsTrack0, sectorsPerTrack);

		return (1);
	}

	/* 1. Open a path to the virtual disk. */

	memset(&out, 0, sizeof(out));
	out.t0sBytes = (long long) sectorsTrack0 * sectorSize;
	out.gapBytes = (sectorsPerTrack - sectorsTrack0) * sectorSize;

	if (format == DISKIMAGE_RAW)
	{
		ec = _native_create(&out.native, pathname, FAM_WRITE, FAP_READ | FAP_WRITE);

		if (ec != 0)
		{
			return (ec);
		}

		_native_seek(out.native, 0, SEEK_SET);
	}
	else
	{
		ec = open_new_image(&image, pathname, format, tracks, sectorsPerTrack, sectorsTrack0, heads,
				    sectorSize, density, interleave);

		if (ec != 0)
		{
			return (ec);
		}

		out.image = &image;
	}

	/**** Build LSN0 *****/
	memset(&s0, 0, sizeof(s0));

	*totalSectors = ((tracks * sectorsPerTrack * heads) - sectorsPerTrack) + sectorsTrack0;
	_int3(*totalSectors, s0.dd_tot);

	*totalBytes = *totalSectors * sectorSize;

	/* Determine appropriate cluster size */

	if (*clusterSize == 0)
	{
		*clusterSize = 1;	/* Set cluster size to 1 to start with */
		while ((*clusterSize * (65535 * 8)) < *totalSectors)	/* test if the right amount of reserved sectors has been reached */
		{
			*clusterSize *= 2;	/* Double Cluster Size for next test pass */
		}
	}

#define	MAX_SECTORS 0xFFFFFF

	if (int3(s0.dd_tot) > MAX_SECTORS)
	{
		fprintf(stderr, "Disk too large\n");

		return (1);
	}
	else if (int3(s0.dd_tot) < 4)
	{
		fprintf(stderr, "Too few sectors\n");

		return (1);
	}

	sectorsLeft = int3(s0.dd_tot);

	_int1(sectorsPerTrack, s0.dd_tks);

	bitmapBytes =
		int3(s0.dd_tot) / (8 * *clusterSize) +
		(int3(s0.dd_tot) % (8 * *clusterSize) != 0);
	_int2(bitmapBytes, s0.dd_map);

	_int2(*clusterSize, s0.dd_bit);

	// Compute bitmap sectors here
	bitmapSectors =
		bitmapBytes / sectorSize + (bitmapBytes % sectorSize != 0);

	/* Compute starting location of root directory */
	/* The dragon uses sectors 3..18 of the first track for storing the boot program */
	/* so we have to place the root directory beyond this */
	if (isDragon)
	{

		_int3(17 + bitmapSectors, s0.dd_dir);
	}
	else
	{
		_int3((int2(s0.dd_map) + sectorSize) / sectorSize +
		      (int2(s0.dd_map) % sectorSize != 0), s0.dd_dir);
	}

	_int2(0, s0.dd_own);
	_int1(0xFF, s0.dd_att);
	_int2(0x0180, s0.dd_dsk);	// Disk ID, is this random???

	/* Build DD.FMT byte */
	b = 0;
	switch (tpi)
	{
	case 48:
		b |= 0;
		break;

	case 96:
		b |= 4;
		break;
	}

	switch (heads)
	{
	case 1:
		b |= 0;
		break;

	case 2:
		b |= 1;
		break;
	}

	switch (density)
	{
	case 0:
		b |= 0;
		break;

	case 1:
		b |= 2;
		break;
	}

	_int1(b, s0.dd_fmt);	// B2 = tpi, B1 = density, B0 = SS or DS

	_int2(sectorsPerTrack, s0.dd_spt);
	_int2(0, s0.dd_res);
	_int3(0, s0.dd_bt);
	_int2(0, s0.dd_bsz);
	UnixToOS9Time(time(NULL), (char *) s0.dd_dat);

	{
		u_int i;
		char sizedDiskName[33];
		memset(sizedDiskName, 0, 33);
		strncpy(sizedDiskName, diskName, 32);

		for (i = 0; i < strlen(sizedDiskName) - 1; i++)
		{
			s0.dd_nam[i] = sizedDiskName[i];
		}
		s0.dd_nam[i] = sizedDiskName[i] | 128;
	}

	if (os968k == 1)
	{
		/*
		 * OS-9/68K (OSK) uses a different DD.OPT layout (rbf_opt prefix).
		 * In particular, there is a pad byte (pd_res1) after pd_dns, and
		 * some fields are 16-bit (e.g. pd_sas, pd_ssize).
		 */

		_int1(DT_os9, s0.dd_opt.m68k.pd_dtp);
		_int1(0, s0.dd_opt.m68k.pd_drv);
		_int1(3, s0.dd_opt.m68k.pd_stp);
		_int1(isHDD ? 0x80 : 0x21, s0.dd_opt.m68k.pd_typ);
		_int1(3, s0.dd_opt.m68k.pd_dns);
		_int1(0, s0.dd_opt.m68k.pd_res1);
		_int2(tracks, s0.dd_opt.m68k.pd_cyl);
		_int1(heads, s0.dd_opt.m68k.pd_sid);
		_int1(0, s0.dd_opt.m68k.pd_vfy);
		_int2(sectorsPerTrack, s0.dd_opt.m68k.pd_sct);
		_int2(sectorsTrack0, s0.dd_opt.m68k.pd_t0s);
		_int2(sectorAllocationSize, s0.dd_opt.m68k.pd_sas);
		_int1(interleave, s0.dd_opt.m68k.pd_ilv);

		_int2(sectorSize, s0.dd_opt.m68k.pd_ssize);
		_int1(7, s0.dd_opt.m68k.pd_trys);
		_int4(0x4372757A, s0.dd_sync); 	/* "Cruz" to mark as OSK) */
		_int4(1, s0.dd_maplsn);
		_int2(1, s0.dd_versid);
		_int2(sectorSize, s0.dd_lsnsize);
	}
	else
	{
		/* OS-9/6809-style packed DD.OPT layout (existing behavior) */
		_int1(DT_os9, s0.dd_opt.m6809.pd_dtp);
		_int1(1, s0.dd_opt.m6809.pd_drv);
		_int1(0, s0.dd_opt.m6809.pd_stp);
		_int1(isHDD ? 0x80 : 0x20, s0.dd_opt.m6809.pd_typ);
		_int1(1, s0.dd_opt.m6809.pd_dns);

		_int2(tracks, s0.dd_opt.m6809.pd_cyl);

		_int1(heads, s0.dd_opt.m6809.pd_sid);
		_int1(0, s0.dd_opt.m6809.pd_vfy);

		_int2(sectorsPerTrack, s0.dd_opt.m6809.pd_sct);
		_int2(sectorsTrack0, s0.dd_opt.m6809.pd_t0s);

		_int1(interleave, s0.dd_opt.m6809.pd_ilv);
		_int1(sectorAllocationSize, s0.dd_opt.m6809.pd_sas);
		/* put bytes per sector */
		_int1(sectorSize / 256, s0.dd_lsnsize);
	}



	/***** Write LSN0 *****/
	{
		u_int s0size = sizeof(s0);
		u_int padding = sectorSize - s0size;

		/* write LSN0 structure */
		write_lsns(&out, &s0, s0size);

		/* fill in rest of sector with zeros */
		while (padding--)
		{
			char nil = '\0';

			write_lsns(&out, &nil, 1);
		}
	}

	/***** Write Bitmap Sector(s) *****/
	{
		u_char *bitmap;
		u_int bitmapSize;

		bitmapSize = bitmapSectors * sectorSize;
		bitmap = (u_char *) malloc(bitmapSize);
		if (bitmap == NULL)
		{
			return (1);
		}

		/* Clear out bitmap sectors (assume all space available) */
		memset(bitmap, 0, bitmapSize);

		/* Allocate bits after s0.dd_dot sectors to end of bitmap */
		{
			int extraBitStart = int3(s0.dd_tot) / *clusterSize;

			_os9_allbit(bitmap, extraBitStart,
				    (bitmapSize * 8) - extraBitStart);
		}

		/* Allocate LSN0, Bitmap Sectors, Root FD and Root Dir */
		{
			int clusters;

			sectorsToAlloc++;	/* LSN0 */
			sectorsToAlloc += bitmapSectors;	/* Bitmap sectors */

			if (isDragon)
			{
				sectorsToAlloc += DragonBootSize;	/* Dragon boot area */
			}

			rootSects = 1;	/* Root FD Sector */
			rootSects += sectorAllocationSize;	/* Root dirent sectors */

			sectorsToAlloc += rootSects;	/* Total sectors so far -- will be a multiple of cluster size */

			/* Round up sectors to allocate to next highest multiple
			 * of cluster size.
			 * This gives the root directory the fullest possible
			 * number of sectors remaining in the cluster.
			 */
			{
				unsigned int sectorsToAllocOld =
					sectorsToAlloc;

				sectorsToAlloc =
					NextHighestMultiple(sectorsToAlloc,
							    *clusterSize);
				rootSects +=
					sectorsToAlloc - sectorsToAllocOld;
			}

			clusters = sectorsToAlloc / *clusterSize;
			_os9_allbit(bitmap, 0, clusters);

			sectorsLeft -= sectorsToAlloc;
		}

		write_lsns(&out, bitmap, bitmapSize);

		free(bitmap);
	}

	/* If this is a Dragon disk, write out Dragon boot area */
	if (isDragon)
	{
		char *DragonBoot;
		u_int BootSize;

		BootSize = DragonBootSize * sectorSize;

		DragonBoot = (char *) calloc(BootSize, 1);
		memset(DragonBoot, 0x55, BootSize);
		write_lsns(&out, DragonBoot, BootSize);

		free(DragonBoot);
	}

	/* Write Root Directory FD and Root Sectors */
	{
		char *allocedSectors;
		unsigned int totalSectors;
		u_int totalBytes;

		if (isDragon)
		{
			totalSectors =
				sectorsToAlloc - 1 - bitmapSectors -
				DragonBootSize;
		}
		else
		{
			totalSectors = sectorsToAlloc - 1 - bitmapSectors;
		}
		totalBytes = totalSectors * sectorSize;
		allocedSectors = (char *) malloc(totalBytes);

		if (allocedSectors == NULL)
		{
			return (1);
		}

		memset(allocedSectors, 0, totalBytes);

		/* Build FD sector */
		{
			Fd_stats statSector = (Fd_stats) allocedSectors;

			char lastModifiedDate[5], createDate[5];

			UnixToOS9Time(time(NULL), lastModifiedDate);
			UnixToOS9Time(time(NULL), createDate);

			statSector->fd_att =
				FAP_DIR | FAP_READ | FAP_WRITE | FAP_EXEC |
				FAP_PREAD | FAP_PWRITE | FAP_PEXEC;
			_int2(0, statSector->fd_own);
			memcpy(statSector->fd_dat, lastModifiedDate, 5);
			statSector->fd_lnk = 1;
			_int4(sizeof(os9_dir_entry) * 2, statSector->fd_siz);
			memcpy(statSector->fd_creat, lastModifiedDate, 3);

			_int3(int3(s0.dd_dir) + 1, statSector->fd_seg[0].lsn);
			_int2(rootSects - 1, statSector->fd_seg[0].num);
		}

		/* Build directory sector */
		{
			os9_dir_entry *d =
				(os9_dir_entry *) &
				allocedSectors[sectorSize];

			/* Create '..' */
			strcpy((char *) d->name, "..");
			CStringToOS9String(d->name);
			_int3(int3(s0.dd_dir), d->lsn);

			/* Create '.' */
			d++;

			strcpy((char *) d->name, ".");
			CStringToOS9String(d->name);
			_int3(int3(s0.dd_dir), d->lsn);
		}


		write_lsns(&out, allocedSectors, totalBytes);

		free(allocedSectors);
	}

	/* Write Rest of disk as empty sectors */
	/* An image of tracks already has them all. */
	if (formatEntire == 1 && out.image == NULL)
	{
		char *oneEmptySector;

		oneEmptySector = (char *) malloc(sectorSize);

		if (oneEmptySector != NULL)
		{
			memset(oneEmptySector, 0xE5, sectorSize);

			while (sectorsLeft--)
			{
				write_lsns(&out, oneEmptySector, sectorSize);
			}

			free(oneEmptySector);
		}
	}

	if (out.image != NULL)
	{
		ec = _diskimage_close(image.disk);

		return out.ec != 0 ? out.ec : ec;
	}

	_native_close(out.native);

	return (ec);
}

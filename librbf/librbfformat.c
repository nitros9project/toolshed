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

error_code _os9_format(char *pathname, int os968k, int tracks,
		       int sectorsPerTrack, int sectorsTrack0,
			   int heads, int sectorSize,
		       int *clusterSize, char *diskName,
		       int sectorAllocationSize, int tpi, int density,
		       int formatEntire, int isDragon, int isHDD, int interleave,
		       unsigned int *totalSectors, unsigned int *totalBytes)
{
	error_code ec = 0;
	native_path_id path;
	lsn0_sect s0;
	unsigned int sectorsLeft;
	int b;
	unsigned int sectorsToAlloc = 0;
	unsigned int bitmapSectors, bitmapBytes;
	unsigned int rootSects;

	/* 1. Open a path to the virtual disk. */

	ec = _native_create(&path, pathname, FAM_WRITE, FAP_READ | FAP_WRITE);

	if (ec != 0)
	{
		return (ec);
	}

	_native_seek(path, 0, SEEK_SET);

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

	_int1(DT_os9, s0.pd_dtp);
	_int1(1, s0.pd_drv);
	_int1(0, s0.pd_stp);
	if (isHDD)
		_int1(0x80, s0.pd_typ);
	else
		_int1(0x20, s0.pd_typ);
	_int1(1, s0.pd_dns);

	_int2(tracks, s0.pd_cyl);

	_int1(heads, s0.pd_sid);
	_int1(0, s0.pd_vfy);

	_int2(sectorsPerTrack, s0.pd_sct);
	_int2(sectorsTrack0, s0.pd_t0s);

	_int1(interleave, s0.pd_ilv);
	_int1(sectorAllocationSize, s0.pd_sas);

	if (os968k == 1)
	{
		/* Build LSN0 for OS-9/68K */

		/* put sync bytes */
		_int4(0x4372757A, s0.dd_sync);
		/* put bitmap starting sector number */
		_int4(1, s0.dd_maplsn);
		/* put sector 0 version ID */
		_int2(1, s0.dd_versid);
	}

	/* put bytes per sector */
	_int1(sectorSize / 256, s0.dd_lsnsize);

	/***** Write LSN0 *****/
	{
		u_int s0size = sizeof(s0);
		u_int padding = sectorSize - s0size;

		/* write LSN0 structure */
		_native_write(path, &s0, &s0size);

		/* fill in rest of sector with zeros */
		while (padding--)
		{
			u_int size = 1;
			char nil = '\0';

			_native_write(path, &nil, &size);
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

		_native_write(path, bitmap, &bitmapSize);

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
		_native_write(path, DragonBoot, &BootSize);

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


		_native_write(path, allocedSectors, &totalBytes);

		free(allocedSectors);
	}

	/* Write Rest of disk as empty sectors */
	if (formatEntire == 1)
	{
		char *oneEmptySector;

		oneEmptySector = (char *) malloc(sectorSize);

		if (oneEmptySector != NULL)
		{
			memset(oneEmptySector, 0xE5, sectorSize);

			while (sectorsLeft--)
			{
				u_int size = sectorSize;

				_native_write(path, oneEmptySector, &size);
			}

			free(oneEmptySector);
		}
	}

	_native_close(path);

	return (ec);
}

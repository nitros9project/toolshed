/********************************************************************
 * open.c - OS-9 open/create routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef WIN32
#include <dirent.h>
#endif

#include "cocotypes.h"
#include "os9path.h"
#include "cocopath.h"
#include "cococonv.h"
#include "cocosys.h"
#include "util.h"


static int init_pd(os9_path_id * path, int mode);
static int term_pd(os9_path_id path);
static int init_bitmap(os9_path_id path);
static error_code term_bitmap(os9_path_id path);
static error_code open_image(os9_path_id path);
static error_code load_lsn0(os9_path_id path);
static int init_lsn0(os9_path_id path);
static int term_lsn0(os9_path_id path);
static error_code _os9_truncate_seg_list(os9_path_id path);
error_code _os9_file_exists(os9_path_id folder_path, char *filename);
int validate_pathlist(os9_path_id * path, char *pathlist);


/*
 * _os9_create()
 *
 * Create a file
 */
error_code _os9_create(os9_path_id * path, char *pathlist, int mode,
		       int perms)
{
	error_code ec = 0;


	/* 1. Allocate & initialize path descriptor. */

	ec = init_pd(path, mode);

	if (ec != 0)
	{
		return ec;
	}


	/* 2. Attempt to validate the pathlist. */

	ec = validate_pathlist(path, pathlist);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 3. Determine if disk is being open in raw mode. */

	if (pathlist[strlen(pathlist) - 1] == '@')
	{
		/* 1. Yes, raw mode. */

		(*path)->israw = 1;
	}
	else
	{
		/* 1. No, normal mode. */

		(*path)->israw = 0;
	}


	{
		fd_stats newFD;
		time_t now;
//        struct tm     *tm;
		int newLSN;
		char *filename;
		os9_path_id parent_path;
		os9_dir_entry newDEntry;


		term_pd(*path);

	      aa:
		filename = NULL;
		ec = _os9_open_parent_directory(&parent_path, pathlist,
						FAM_DIR | FAM_WRITE,
						&filename);

		if (ec != 0)
		{
			if (filename != NULL)
				free(filename);
			return ec;
		}


		/* 1. Check if filename conforms to OS-9 standard. */

		ec = _os9_prsnam(filename);

		if (ec != 0)
		{
			free(filename);
			_os9_close(parent_path);

			return ec;
		}


		/* 2. Check if file already exists. */

		ec = _os9_file_exists(parent_path, filename);

		if (ec == EOS_FAE && (mode & FAM_NOCREATE) == 0)
		{
			/* 1. If file already exists and FAM_NOCRFEATE isn't specified,
			 * delete the file and try to create it again.
			 */

			_os9_close(parent_path);

			ec = _os9_delete(pathlist);

			if (ec == 0)
			{
				free(filename);
				goto aa;
			}
		}

		if (ec != 0)
		{
			free(filename);

			_os9_close(parent_path);

			return ec;
		}

		now = GetSafeTimestamp();

		/* 3. Populate file descriptor. */

		memset(&newFD, 0, sizeof(fd_stats));

		newFD.fd_att = perms;
		_int2(0, newFD.fd_own);
		UnixToOS9Time(now, (char *) newFD.fd_dat);
		newFD.fd_lnk = 1;
		_int4(0, newFD.fd_siz);
		newFD.fd_creat[0] = newFD.fd_dat[0];
		newFD.fd_creat[1] = newFD.fd_dat[1];
		newFD.fd_creat[2] = newFD.fd_dat[2];


		/* 4. Allocate new cluster on 'sectors per cluster' boundary. */

		if (parent_path->bitmap_ec != 0)
		{
			ec = parent_path->bitmap_ec;
			free(filename);
			_os9_close(parent_path);

			return ec;
		}

		newLSN = _os9_getfreebit(parent_path->bitmap,
					 int3(parent_path->lsn0->dd_tot)) *
			parent_path->spc;

		if (newLSN < 0)
		{
			free(filename);
			_os9_close(parent_path);

			return EOS_DF;
		}

		/* 5. If our cluster size > 1, add this cluster and leftover count
		 * to the segment list of the newly allocated file descriptor
		 */

		if (parent_path->spc > 1)
		{
			_int3(newLSN + 1, newFD.fd_seg[0].lsn);
			_int2(parent_path->spc - 1, newFD.fd_seg[0].num);
		}


		/* 6. Write file descriptor to image file. */

		ec = _os9_lsn_put(parent_path, newLSN, 0, &newFD, sizeof(fd_stats));

		if (ec != 0)
		{
			free(filename);
			_os9_delbit(parent_path->bitmap, newLSN / parent_path->spc, 1);
			_os9_close(parent_path);

			return ec;
		}

		memset(&newDEntry, 0, sizeof(os9_dir_entry));
		strcpy((char *) &(newDEntry.name), filename);
		free(filename);
		CStringToOS9String((u_char *) & (newDEntry.name));
		_int3(newLSN, newDEntry.lsn);

		_os9_seek(parent_path, 0, SEEK_SET);


		/* 7. Add directory entry to the parent's directory. */

		while ((ec = _os9_gs_eof(parent_path)) == 0)
		{
			os9_dir_entry dentry;
			int mode = parent_path->mode;


			/* 1. Set up path temporarily as a directory so _os9_readdir won't fail. */

			parent_path->mode |= FAM_DIR | FAM_READ;

			ec = _os9_readdir(parent_path, &dentry);

			parent_path->mode = mode;

			if (ec != 0)
			{
				break;
			}

			if (dentry.name[0] == '\0')
			{
				_os9_seek(parent_path, -(int) sizeof(dentry),
					  SEEK_CUR);
				break;
			}
		}


		if (ec != 0 && ec != EOS_EOF)
		{
			_os9_delbit(parent_path->bitmap, newLSN / parent_path->spc, 1);
			_os9_close(parent_path);

			return ec;
		}


		/* 8. Write the directory entry back to the image. */

		ec = _os9_writedir(parent_path, &newDEntry);

		if (ec != 0)
		{
			_os9_delbit(parent_path->bitmap, newLSN, 1);
			_os9_close(parent_path);

			return ec;
		}

		ec = _os9_close(parent_path);

		if (ec != 0)
		{
			return ec;
		}

		return (_os9_open(path, pathlist, mode));
	}


//    return ec;
}



/*
 * _os9_open()
 *
 * Open a path to a file or directory.
 *
 * Legal pathnames are:
 *
 * 1. imagename,@     (considered to be a 'raw' open of the image)
 * 2. imagename,      (considered to be an open of the root directory)
 * 3. imagename,file  (considered to be a file or directory open within the image)
 * 4. imagename       (considered to be an error)
 *
 * The presence of a comma in the pathlist indicates that at the least, a non-native open will
 * be performed.
 */
error_code _os9_open(os9_path_id * path, char *pathlist, int mode)
{
	error_code ec = 0;
	char *p;
	char *tmppathlist;


	/* 1. Strip off FAM_NOCREATE if passed -- irrelavent to _os9_open. */

	mode = mode & ~FAM_NOCREATE;


	/* 2. Allocate & initialize path descriptor. */

	ec = init_pd(path, mode);

	if (ec != 0)
	{
		return ec;
	}


	/* 3. Attempt to validate the pathlist. */

	ec = validate_pathlist(path, pathlist);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 4. Determine if disk is being open in raw mode. */

	if (pathlist[strlen(pathlist) - 1] == '@')
	{
		/* 1. Yes, raw mode. */

		(*path)->israw = 1;
	}
	else
	{
		/* 1. No, normal mode. */

		(*path)->israw = 0;
	}

	{
		/* 1. Open a path to the image file. */

		ec = open_image(*path);

		if (ec != 0)
		{
			term_pd(*path);

			return ec;
		}
	}


	/* 6. Read LSN0. */

	ec = init_lsn0(*path);

	if (ec != 0)
	{
		_diskimage_close((*path)->disk);
		term_pd(*path);

		return ec;
	}


	/* 7. Read bitmap. */

	ec = init_bitmap(*path);

	if (ec != 0)
	{
		free((*path)->bitmap);
		free((*path)->saved_bitmap);
		term_lsn0(*path);
		_diskimage_close((*path)->disk);
		term_pd(*path);

		return ec;
	}


	/* 8. If path is raw, just return now. */

	if ((*path)->israw == 1)
	{
		return 0;
	}


	/* 9. Walk the pathlist to find the FD LSN of the last element
	 * in the pathlist.
	 */

	(*path)->pl_fd_lsn = int3((*path)->lsn0->dd_dir);

	tmppathlist = strdup((*path)->pathlist);
	p = strtok(tmppathlist, "/");
	if (p == NULL)
	{
		p = ".";
	}
	do
	{
		os9_dir_entry diskent;


		for (;;)
		{
			char q[64];
			int mode = (*path)->mode;


			/* 1. Set up path temporarily as a directory so _os9_readdir won't fail. */

			(*path)->mode |= FAM_DIR | FAM_READ;

			ec = _os9_readdir(*path, &diskent);

			(*path)->mode = mode;

			if (ec != 0)
			{
				break;
			}


			/* 2. Try to match up the pathlist element to this entry. */

			strcpy(q, (char *) diskent.name);

			if (strcasecmp
			    ((char *) p,
			     (char *) OS9StringToCString((u_char *) q)) == 0)
			{
				(*path)->pl_fd_lsn = int3(diskent.lsn);
				(*path)->filepos = 0;
				break;
			}
		}
	}
	while (ec == 0 && (p = strtok(NULL, "/")) != 0);


	/* 10. If error encountered, return. */

	if (ec != 0)
	{
		free(tmppathlist);

		free((*path)->bitmap);
		free((*path)->saved_bitmap);
		term_lsn0(*path);

		_diskimage_close((*path)->disk);

		term_pd(*path);

		if (ec == EOS_EOF)
		{
			return EOS_PNNF;
		}

		return ec;
	}


	/* 11. Obtain fd sector and check file permissions against
	 * passed permissions.
	 *
	 * Note that we only check for owner read/write/dir permissions
	 * and ignore whether the user on the host OS has the same UID
	 * as the file's creator.  Those checks may come later.
	 */

	{
		fd_stats fd_sector;
		int andresult;


		ec = _os9_lsn_get(*path, (*path)->pl_fd_lsn, 0, &fd_sector, sizeof(fd_stats));

		if (ec != 0)
		{
			free(tmppathlist);

			free((*path)->bitmap);
			free((*path)->saved_bitmap);
			term_lsn0(*path);

			_diskimage_close((*path)->disk);

			term_pd(*path);

			return ec;
		}

		/* 1. Check permissions to determine if we can access the file. */

		andresult =
			mode & (fd_sector.
				fd_att & (FAM_DIR | FAM_READ | FAM_WRITE));

		if (andresult != mode
		    || ((fd_sector.fd_att & FAM_DIR) != (mode & FAM_DIR)))
		{
			free(tmppathlist);

			free((*path)->bitmap);
			free((*path)->saved_bitmap);
			term_lsn0(*path);

			_diskimage_close((*path)->disk);

			term_pd(*path);

			return EOS_FNA;
		}
	}

	free(tmppathlist);


	return 0;
}


/* _os9_open_parent_directory()
 *
 * Open a path to the file's directory
 */
error_code _os9_open_parent_directory(os9_path_id * path, char *pathlist,
				      int mode, char **filename)
{
	char *pathcopy, *lastPathComponent, *lastPathSeperator;

	*filename = NULL;

	/* 1. A pathlist with no comma does not name anything in an image. */

	if (strchr(pathlist, ',') == NULL)
	{
		return EOS_BPNAM;
	}

	/* 2. Generate path to parent. */

	pathcopy = strdup(pathlist);

	lastPathComponent = strchr(pathcopy, ',') + 1;
	lastPathSeperator = strrchr(lastPathComponent, '/');

	if (lastPathSeperator != NULL)
	{
		lastPathComponent = lastPathSeperator + 1;
	}

	*filename = strdup(lastPathComponent);
//    strcpy(filename, lastPathComponent);
	pathcopy[lastPathComponent - pathcopy] = '.';
	pathcopy[lastPathComponent - pathcopy + 1] = '\0';

	error_code ec = _os9_open(path, pathcopy, mode);

	free(pathcopy);

	return ec;
}



/*
 * _os9_close()
 *
 * Close a path to a file
 */
error_code _os9_close(os9_path_id path)
{
	error_code ec = 0;
	error_code step_ec;

	if (path->disk != NULL)
	{
		/* 1. This is a valid path. */

		{
			if (path->israw == 0)
			{
				ec = _os9_truncate_seg_list(path);
			}

			step_ec = term_bitmap(path);
			if (ec == 0)
			{
				ec = step_ec;
			}
			term_lsn0(path);

			step_ec = _diskimage_close(path->disk);
			if (ec == 0)
			{
				ec = step_ec;
			}
		}

		term_pd(path);
	}


	return ec;
}



static error_code _os9_truncate_seg_list(os9_path_id path)
{
	fd_stats fd_sector;
	int i;
	unsigned int file_size, rounded_size, max_size, truncation;
	unsigned int clusters_to_truncate = 0;


	/* 1. Only a path that can write can truncate anything. */

	if ((path->mode & FAM_WRITE) == 0)
	{
		return 0;
	}


	/* 2. Get the file descriptor sector for this file in memory. If it is
	 * missing, nothing done with the path could have changed the file.
	 */

	if (_os9_lsn_get(path, path->pl_fd_lsn, 0, &fd_sector, sizeof(fd_stats)) != 0)
	{
		return 0;
	}


	/* 2a. If this file is a directory, then abort. */

	if ((fd_sector.fd_att & FAP_DIR) == FAP_DIR)
	{
		return 0;
	}


	/* 3. Get size of file. */

	file_size = int4(fd_sector.fd_siz);


	/* 4. Get number of bytes when rounded to next cluster. */

	rounded_size = NextHighestMultiple(file_size, path->cs);


	/* 5. Get number of bytes as currently represented by segment list. */

	max_size = _os9_maximum_file_size(fd_sector, path->bps);


	/* 6. If the size that the segment list conveys is larger than our round size,
	 * then compute the truncation value (how much we will cut).
	 */

	if (max_size > rounded_size)
	{
		truncation =
			_os9_maximum_file_size(fd_sector,
					       path->bps) - rounded_size;
		clusters_to_truncate = truncation / path->cs;
	}


	/* 7. Leave the disk alone if there is nothing to cut, or no bitmap to free it in. */

	if (clusters_to_truncate == 0 || path->bitmap_ec != 0)
	{
		return 0;
	}

	while (clusters_to_truncate--)
	{
		/* 1. Remove one cluster from end of segement list. */

		for (i = NUM_SEGS - 1; i >= 0; i--)
		{
			if (int3(fd_sector.fd_seg[i].lsn) != 0)
			{
				_os9_delbit(path->bitmap,
					    (int3(fd_sector.fd_seg[i].lsn) +
					     int2(fd_sector.fd_seg[i].num) -
					     1) / path->spc, 1);
				_int2(int2(fd_sector.fd_seg[i].num) -
				      path->spc, fd_sector.fd_seg[i].num);

				if (int2(fd_sector.fd_seg[i].num) == 0)
				{
					_int3(0, fd_sector.fd_seg[i].lsn);
				}
				break;
			}
		}
	}


	return _os9_lsn_put(path, path->pl_fd_lsn, 0, &fd_sector, sizeof(fd_stats));
}



/* Return the maximum size of the file based on the state of the segments */
int _os9_maximum_file_size(fd_stats fd_sector, int bytesPerSector)
{
	int maximum_size = 0, i;


	for (i = 0; i < NUM_SEGS && int3(fd_sector.fd_seg[i].lsn) != 0; i++)
	{
		maximum_size +=
			int2(fd_sector.fd_seg[i].num) * bytesPerSector;
	}


	return maximum_size;
}



/*
 * validate_pathlist()
 *
 * Determines if the passed <image,path> pathlist is valid.
 *
 * Copies the image file and pathlist file portions into
 * the path descriptor.
 *
 * Valid pathlist examples:
 *	foo,/
 *	foo,.
 *	foo,/bar
 *	foo,bar
 */
int validate_pathlist(os9_path_id * path, char *pathlist)
{
	char *p;
	char *tmppathlist;


	if (strchr(pathlist, ',') == NULL)
	{
		return EOS_BPNAM;
	}


	/* 2. Check validity of pathlist. */

	tmppathlist = strdup(pathlist);

	p = strtok(tmppathlist, ",");

	if (p == NULL)
	{
		free(tmppathlist);

		return (EOS_BPNAM);
	}


	/* 3. Copy OS-9 pathlist element. */

	(*path)->imgfile = strdup(p);

	p = strtok(NULL, ",");

	if (p == NULL)
	{
		/* 1. There was nothing following the native/os9 delimiter, assume root. */


		(*path)->pathlist = strdup(".");
	}
	else
	{
		/* 1. If here, there is something following the native/os9 delimiter */
		/* skip over any leading slashes after the , delimiter */

		while (*p == '/')
		{
			p++;
		}

		(*path)->pathlist = strdup(p);
	}

	free(tmppathlist);


	return 0;
}



error_code _os9_file_exists(os9_path_id folder_path, char *filename)
{
	error_code ec = 0;
	os9_dir_entry dentry;


	_os9_seek(folder_path, 0, SEEK_SET);

	while ((ec = _os9_gs_eof(folder_path)) == 0)
	{
		int mode = folder_path->mode;

		folder_path->mode |= FAM_DIR | FAM_READ;
		ec = _os9_readdir(folder_path, &dentry);
		folder_path->mode = mode;

		if (ec != 0)
		{
			return ec;
		}


		if (strcasecmp
		    (filename, (char *) OS9StringToCString(dentry.name)) == 0)
		{
			return EOS_FAE;
		}
	}


	return ec == EOS_EOF ? 0 : ec;
}



static int init_pd(os9_path_id * path, int mode)
{
	/* 1. Allocate path structure and initialize it. */

	*path = malloc(sizeof(struct _os9_path_id));

	if (*path == NULL)
	{
		return 1;
	}


	/* 2. Clear out newly allocated path structure. */

	memset(*path, 0, sizeof(**path));

	(*path)->mode = mode;


	return 0;
}



static int term_pd(os9_path_id path)
{
	/* 1. Deallocate path structure. */
	if (path->imgfile != NULL)
	{
		free(path->imgfile);
	}

	if (path->pathlist != NULL)
	{
		free(path->pathlist);
	}

	free(path);


	return 0;
}



/*
 * init_bitmap()
 *
 * read the bitmap sectors
 */
static int init_bitmap(os9_path_id path)
{
	int bitmap_sectors;


	bitmap_sectors = (int2(path->lsn0->dd_map) / path->bps) +
		(int2(path->lsn0->dd_map) % path->bps != 0);
	path->bitmap_bytes = int2(path->lsn0->dd_map);
	path->spc = int2(path->lsn0->dd_bit);
	path->cs = path->spc * path->bps;	/* compute cluster size */

	path->bitmap = (u_char *) malloc(bitmap_sectors * path->bps);
	path->saved_bitmap = (u_char *) malloc(bitmap_sectors * path->bps);

	if (path->bitmap == NULL || path->saved_bitmap == NULL)
	{
		return 1;
	}

	/* Only what needs the bitmap fails if it is missing. What could not be
	 * read counts as allocated.
	 */

	memset(path->bitmap, 0xFF, bitmap_sectors * path->bps);
	path->bitmap_ec = _os9_lsn_get(path, 1, 0, path->bitmap, bitmap_sectors * path->bps);
	memcpy(path->saved_bitmap, path->bitmap, bitmap_sectors * path->bps);


	return 0;
}


/*
 * term_bitmap()
 *
 * free the bitmap sectors
 */
static error_code term_bitmap(os9_path_id path)
{
	error_code ec = 0;

	/* 1. Write back out bitmap if it changed. */

	if ((path->mode & FAM_WRITE) && path->bitmap_ec == 0 &&
	    memcmp(path->bitmap, path->saved_bitmap, path->bitmap_bytes) != 0)
	{
		ec = _os9_lsn_put(path, 1, 0, path->bitmap, path->bitmap_bytes);
	}

	free(path->bitmap);
	free(path->saved_bitmap);


	return ec;
}


/*
 * init_lsn0()
 *
 * Read LSN0, the first sector of an OS-9 disk
 */
static int init_lsn0(os9_path_id path)
{
	error_code ec = load_lsn0(path);

	if (ec != 0)
	{
		return ec;
	}

	if (int2(memcmp(path->lsn0->dd_sync, "Cruz", 4) == 0 ? path->lsn0->dd_opt.m68k.pd_t0s
							     : path->lsn0->dd_opt.m6809.pd_t0s) == 0)
	{
		fprintf(stderr, "Warning: T0S is zero. Assuming SPT: %d\n", path->spt);
	}

	return 0;
}


/* Open a path's image file and find how its sectors are addressed. */
static error_code open_image(os9_path_id path)
{
	diskimage_geometry geometry;
	error_code ec = _diskimage_open(&path->disk, path->imgfile, FAM_READ | (path->mode & FAM_WRITE));

	if (ec != 0)
	{
		return ec;
	}

	ec = _diskimage_query_geometry(path->disk, &geometry);

	if (ec != 0)
	{
		_diskimage_close(path->disk);
		path->disk = NULL;

		return ec;
	}

	path->raw_image = geometry.format == DISKIMAGE_RAW;
	path->sides = geometry.sides > 1 ? geometry.sides : 1;
	path->first_sector = geometry.first_sector == 0 && !path->raw_image ? 0 : 1;

	return 0;
}


/*
 * load_lsn0()
 *
 * Read LSN0 and the layout of the disk from it.
 */
static error_code load_lsn0(os9_path_id path)
{
	int is_osk;
	u_char sector[1024];
	u_int size = sizeof(sector);
	error_code ec;

	/* 1. Allocate 256 bytes for LSN0. */

	path->lsn0 = (lsn0_sect *) malloc(1 * 256);

	if (path->lsn0 == NULL)
	{
		return 1;
	}


	/* 2. LSN0 is the first sector of the disk, whatever its layout. */

	ec = _diskimage_read_sector(path->disk, 0, 0, path->first_sector, sector, &size);

	if (ec == EOS_CRC)
	{
		fprintf(stderr, "Warning: bad CRC in LSN0 of %s\n", path->imgfile);
		ec = 0;
	}

	if (ec != 0 || size < 256)
	{
		free(path->lsn0);
		path->lsn0 = NULL;

		return ec != 0 ? (ec == EOS_IA ? EOS_SECT : ec) : EOS_SECT;
	}


	/* 3. Keep 256 byte LSN0. */

	memcpy(path->lsn0, sector, 256);

	is_osk = (memcmp(path->lsn0->dd_sync, "Cruz", 4) == 0);


	/* 4. Compute bytes per sector from LSN0's lsnsize field. */

	if (int1(path->lsn0->dd_lsnsize) == 0)
	{
		/* 1. OS-9/6809 and some OS-9/68K formats have this field as 0,
		 * which means 256 bytes/sector.
		 */

		path->bps = 256;
	}
	else
	{
		/* 1. In this case, OS-9/68K has the proper value in
		 * the field (1 = 256 bps, 2 = 512 bps, etc.).
		 */

		path->bps = int1(path->lsn0->dd_lsnsize) * 256;
	}

	/* 5. Determine proper sectors per track */

	path->spt = int2(path->lsn0->dd_spt);
	path->t0s = int2(is_osk ? path->lsn0->dd_opt.m68k.pd_t0s
				: path->lsn0->dd_opt.m6809.pd_t0s);

	if (path->t0s == 0)
	{
		path->t0s = path->spt;
	}

	/* 6. A disk's tracks use its other side only if it says it has one. */

	if ((int1(path->lsn0->dd_fmt) & 1) == 0)
	{
		path->sides = 1;
	}

	return 0;
}


/*
 * _os9_identify()
 *
 * Determine if an image is an OS-9 disk, by looking for the . and .. entries
 * of its root directory. A sector it cannot read means that it is not.
 */
error_code _os9_identify(const char *pathname, int *found)
{
	u_char sector[256];
	os9_path_id path;
	error_code ec = init_pd(&path, FAM_READ);

	*found = 0;

	if (ec != 0)
	{
		return ec;
	}

	path->imgfile = strdup(pathname);
	ec = open_image(path);

	if (ec != 0)
	{
		term_pd(path);

		return ec;
	}

	if (load_lsn0(path) == 0)
	{
		if (_os9_lsn_get(path, int3(path->lsn0->dd_dir) + 1, 0, sector, sizeof(sector)) == 0)
		{
			*found = sector[0] == 0x2E && sector[1] == 0xAE && sector[32] == 0xAE;
		}

		term_lsn0(path);
	}

	_diskimage_close(path->disk);
	term_pd(path);

	return 0;
}



/*
 * term_lsn0()
 *
 * Terminate LSN0 buffer
 */
static int term_lsn0(os9_path_id path)
{
	free(path->lsn0);

	return 0;
}

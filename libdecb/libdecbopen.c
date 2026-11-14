/********************************************************************
 * decb_open.c - Disk BASIC open/create routines
 *
 * $Id$
 ********************************************************************/

/*
From http://www.cs.unc.edu/~yakowenk/coco/text/diskformat.html

CoCo Disk BASIC disks are formatted to contain 35 tracks, numbered 0 through 34.
Each track has 18 sectors, numbered 1 through 18. A sector contains 256 bytes.

Track number 17 is special; it contains the directory and File Allocation Table (or FAT).
Every other track is divided into two granules; in those tracks, sectors 1 through 9 form
one granule, and sectors 10 through 18 form the other. So there are 68 granules on a disk,
numbered 0 through 67, each containing 2304 bytes. Disk space for files is allocated by
the granule, so even if you create a file that contains only one byte, a whole granule of
2304 bytes is reserved for it. While it may seem wasteful at first, this reduces the
amount of work in allocating space for the file as you add to it. The computer only has
to do that allocation work once for every 2304 bytes that you add. It also reduces
fragmentation - by reserving space in such big chunks, your file can't possibly end up
scattered all over the disk in little tiny pieces.

The directory track (17) contains the file allocation table in sector 2, and the directory
of files in sectors 3 through 11. The remaining sectors on the directory track are unused
("reserved for future use").

The file allocation table is 68 bytes long; one byte for each granule on the disk. If one
of these bytes is between 0 and 67, it tells the number of the next granule used by the
same file. If it is between 192 and 201 (hex C0 and C9), then this is the last granule
allocated for its file, and the least significant four bits tell how many sectors of the
granule are used. If it is FF then it is unused, and may be allocated as needed. So the
bytes in the FAT form a linked list for each file, telling which granules the file
consists of.

Each directory sector contains eight entries of 32 bytes each. So the entire directory
has room for 72 files. (There is room in the directory for more files than there are
granules on the disk!) Each entry contains:

	eight bytes for the filename (padded with spaces)
	three bytes for the filename extension (padded with spaces)
	one file-type byte
	(0=BASIC program, 1=BASIC data, 2=machine code, or 3=ASCII text)
	one format byte (0=binary or FF=ASCII)
	one byte telling the number of the file's first granule
	two bytes telling the number of bytes used in the last sector in the last granule,
	sixteen unused bytes ("reserved for future use" again).
	
Color Disk BASIC reserves track 17 for the directory because that is the middle position
for the read/write head of the disk drive, so it should be efficient for frequent access.
When allocating granules to be used in files, it chooses granules that are close to the
directory first, so in a half-full disk you would expect the outermost and innermost
tracks to be empty, and the tracks near the directory to be full.
*/

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>

#include "decbpath.h"
#include "errno.h"
#include "dirent.h"
#include "util.h"


static int init_pd(decb_path_id * path, int mode);
static int term_pd(decb_path_id path);
static int validate_pathlist(decb_path_id * path, char *pathlist);
static int _decb_cmp(decb_dir_entry * entry, char *name);

int decb_granule_count;

extern error_code find_free_granule(decb_path_id path, int *granule,
				    int next_to);


/*
 * _decb_create()
 *
 * Create a file
 */

error_code _decb_create(decb_path_id * path, char *pathlist, int mode,
			int file_type, int data_type)
{
	error_code ec = EOS_BPNAM;
	int empty_entry = -1;


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


	/* 3. Open a path to the image file. */

	ec = _decb_open_image(*path);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 4. At this point, sector and granule function will work - Load FAT */

	_decb_load_fat(*path);

	if ((*path)->fat_ec != 0)
	{
		ec = (*path)->fat_ec;

		_diskimage_close((*path)->disk);

		term_pd(*path);

		return ec;
	}


	/* 5. Determine if there is enough space. */

	{
		int i, free_granules = 0;


		for (i = 0; i < 256; i++)
		{
			if (i >= (*path)->granule_count)
				break;

			if ((*path)->FAT[i] == 0xFF)
			{
				free_granules++;

				break;
			}
		}

		if (free_granules == 0)
		{
			_diskimage_close((*path)->disk);

			term_pd(*path);

			return EOS_DF;
		}
	}


	/* 6. Construct a directory entry. */

	{
		char *p = strchr((*path)->filename, '.');


		/* 1. Clear memory. */

		memset(&((*path)->dir_entry), 0, sizeof(decb_dir_entry));
		memset(&((*path)->dir_entry), 0x20, 11);


		if (p == NULL)
		{
			int length = strlen((*path)->filename);

			if (length > 8)
				length = 8;

			memcpy((char *) (*path)->dir_entry.filename,
			       (*path)->filename, length);
		}
		else
		{
			int length = p - (*path)->filename;


			if (length > 8)
			{
				length = 8;
			}

			memcpy((char *) (*path)->dir_entry.filename,
			       (*path)->filename, length);


			p++;	/* skip over '.' */

			length = strlen(p);

			if (length > 3)
			{
				length = 3;
			}

			memcpy((char *) (*path)->dir_entry.file_extension, p,
			       length);
		}

		(*path)->dir_entry.file_type = file_type;

		(*path)->dir_entry.ascii_flag = data_type;
	}


	/* 7. Determine if file already exists. */

	{
		decb_dir_entry de;


		/* 1. Find an empty dir (and check for duplicate entry too) */

		_decb_seekdir(*path, 0, SEEK_SET);

		while ((ec = _decb_readdir(*path, &de)) == 0)
		{
			/* 1. If we run across an empty directory entry, make a note of it. */

			if ((de.filename[0] == 255 || de.filename[0] == '\0')
			    && empty_entry == -1)
			{
				empty_entry =
					(*path)->directory_entry_index - 1;

				(*path)->this_directory_entry_index =
					(*path)->directory_entry_index;
			}

			/* Names are found without regard to case (see _decb_cmp), so
			   one differing only in case is the same file. */

			if (strncasecmp
			    ((char *) de.filename,
			     (char *) (*path)->dir_entry.filename, 8) == 0)
			{
				if (strncasecmp
				    ((char *) de.file_extension,
				     (char *) (*path)->dir_entry.
				     file_extension, 3) == 0)
				{
					/* 1. A file of this type already exists. */

					/* Error if we are not to create it */
					if (mode & FAM_NOCREATE)
					{
						_diskimage_close((*path)->disk);
						term_pd(*path);
						return EOS_FAE;
					}
					else
					{
						_diskimage_close((*path)->disk);
						term_pd(*path);
						_decb_kill(pathlist);
						return _decb_create(path,
								    pathlist,
								    mode,
								    file_type,
								    data_type);
					}
				}
			}
		}


		if (ec != EOS_PNNF)
		{
			/* 1. A directory sector this needs is missing. */

			_diskimage_close((*path)->disk);

			term_pd(*path);

			return ec;
		}

		if (empty_entry == -1)
		{
			/* 1. There are no more directory entries left. */

			_diskimage_close((*path)->disk);

			term_pd(*path);

			return EOS_DF;
		}
	}


	/* 8. Allocate a granule for this file. */

	{
		error_code ec;
		int new_granule;


		ec = find_free_granule(*path, &new_granule, 34);

		if (ec != 0)
		{
			return ec;
		}

		(*path)->FAT[new_granule] = 0xC1;
		(*path)->dir_entry.first_granule = new_granule;

		_int2(0, (*path)->dir_entry.last_sector_size);
	}


	/* 9. Write the new directory entry. */

	_decb_seekdir(*path, empty_entry, SEEK_SET);

	(*path)->this_directory_entry_index = empty_entry;

	ec = _decb_writedir(*path, &(*path)->dir_entry);


	return ec;
}



/*
 * _decb_open()
 *
 * Open a path to a file
 *
 * Legal pathnames are:
 *
 * 1. imagename,      (considered to be a 'raw' open of the image)
 * 2. imagename,file  (considered to be a file open within the image)
 * 3. imagename       (considered to be an error) 
*/

error_code _decb_open(decb_path_id * path, char *pathlist, int mode)
{
	error_code ec = 0;


	/* 1. Strip off FAM_NOCREATE if passed -- irrelavent to _decb_open */

	mode = mode & ~FAM_NOCREATE;

	/* 2. Allocate & initialize path descriptor */

	ec = init_pd(path, mode);

	if (ec != 0)
	{
		return ec;
	}


	/* 3. Attempt to validate the pathlist */

	ec = validate_pathlist(path, pathlist);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 4. Determine if disk is being open in raw mode. (We know it is raw mode if there is no filename). */

	if (*(*path)->filename == '\0')
	{
		/* 1. Yes, raw mode */

		(*path)->israw = 1;
	}
	else
	{
		(*path)->israw = 0;

		/* If mode is FAM_DIR, then we need to error */
		if (mode & FAM_DIR)
		{
			term_pd(*path);
			return EOS_SN;
		}
	}


	/* 5. Open a path to the image file. */

	ec = _decb_open_image(*path);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 6. At this point, sector and granule function will work - Load FAT */

	_decb_load_fat(*path);


	/* 7. If path is raw, just return now. */

	if ((*path)->israw == 1)
	{
		return 0;
	}


	/* 8. Find directory entry matching filename. */

	{
		/* 1. Seek to the first directory entry. */

		_decb_seekdir(*path, 0, SEEK_SET);


		/* 2. Check each entry until we find a match. */

		while ((ec = _decb_readdir(*path, &(*path)->dir_entry)) == 0)
		{
			if (_decb_cmp(&(*path)->dir_entry, (*path)->filename)
			    == 0)
			{
				/* 1. We have a match! */

				(*path)->directory_entry_index--;

				(*path)->this_directory_entry_index =
					(*path)->directory_entry_index;

				break;
			}
		}

	}


	/* 9. Return status. */

	return (ec);
}



/*
 * _decb_close()
 *
 * Close a path to a file
 */
error_code _decb_close(decb_path_id path)
{
	error_code ec = 0;


	/* 1. Write out FAT sector if it changed, and close path. */

	ec = _decb_close_image(path);


	/* 2. Terminate path descriptor */

	term_pd(path);


	/* 4. Return status. */

	return (ec);
}



static int _decb_cmp(decb_dir_entry * entry, char *name)
{
	unsigned char modified_name[13];

	DECBStringToCString(entry->filename, entry->file_extension,
			    modified_name);

	return (strcasecmp((const char *) modified_name, name));
}


/*
 * validate_pathlist()
 *
 * Determines if the passed <image,path:drive> pathlist is valid.
 *
 * Copies the image file and pathlist file portions into
 * the path descriptor.
 *
 * Valid pathlist examples:
 * foo,				 	Opens foo for raw access
 * foo,:2				Opens third disk in foo for raw access
 * foo,bar.bas			Opens file bar.bas in disk image foo
 * foo,bar.bin:1		Opens file bar.bin in second disk image in file foo
 * foo,bar.bin:0+12345  Opens file bar.bin in first disk at OS9 offset 12345
 */

static int validate_pathlist(decb_path_id * path, char *pathlist)
{
	error_code ec = 0;
	char *p;


	/* 1. Validate the pathlist. */

	if ((p = strchr(pathlist, ',')) == NULL)
	{
		/* 1. No native/RS-DOS delimiter in pathlist, return error. */

		ec = EOS_BPNAM;
	}
	else
	{
		char *q;


		/* 1. Extract information out of pathlist. */

		/* (*path)->imgfile = strndup(pathlist, p - pathlist); */
		(*path)->imgfile = malloc(p - pathlist + 1);
		memcpy((*path)->imgfile, pathlist, p - pathlist);
		(*path)->imgfile[p - pathlist] = '\0';

		p++;
		if (*p == '/')
			p++;
		q = strchr(p, ':');
		if (q != NULL)
		{
			/* Parse HDB-DOS 'colon' parameters */
			/* (*path)->filename = strndup(p, q - p); */
			(*path)->filename = malloc(q - p + 1);
			memcpy((*path)->filename, p, q - p);
			(*path)->filename[q - p] = '\0';

			(*path)->drive = atoi(q + 1);
			(*path)->has_drive = 1;

			q = strchr(p, '+');
			if (q != NULL)
			{
				if (strncmp(q + 1, "0x", 2) == 0
				    || strncmp(q + 1, "0X", 2) == 0)
					(*path)->hdbdos_offset =
						strtol(q + 3, (char **) NULL,
						       16) * 256;
				else
					(*path)->hdbdos_offset =
						atoi(q + 1) * 256;
			}
			else
			{
				(*path)->hdbdos_offset = 0;
			}
			
			if ((*path)->granule_count != 68)
			{
				fprintf(stderr, "HDB-DOS enabled. Forcing 68 granules per disk. (%d)\n",
					(*path)->granule_count);
				(*path)->granule_count = 68;
			}	
		}
		else /* no HDB-DOS 'colon' found in path name */
		{
			int len = strlen(p);
			if (len <= 12)
			{
				(*path)->filename = strdup(p);
			}
			else
			{
				ec = EOS_BPNAM;
			}
		}

#if 0
		count = sscanf(pathlist, "%512[^,]%*c%64[^:]%*c%d",
			       (*path)->imgfile, (*path)->filename,
			       &((*path)->drive));

		if (count < 2)
		{
			count = sscanf(pathlist, "%512[^,]%*c%*c%d",
				       (*path)->imgfile, &((*path)->drive));
		}
#endif
	}


	/* 2. Return. */

	return ec;
}



static int init_pd(decb_path_id * path, int mode)
{
	/* 1. Allocate path structure and initialize it. */

	*path = malloc(sizeof(struct _decb_path_id));

	if (*path == NULL)
	{
		return 1;
	}


	/* 2. Clear out newly allocated path structure. */

	memset(*path, 0, sizeof(struct _decb_path_id));

	(*path)->mode = mode;


	/* 3. set up defaults */
	
	if (decb_granule_count == 0)
	{
		(*path)->granule_count = 68;
	}
	else
	{
		(*path)->granule_count = decb_granule_count;
	}
	
	/* 4. Return. */

	return 0;
}



static int term_pd(decb_path_id path)
{
	/* 1. Deallocate path structure. */

	if (path->imgfile)
		free(path->imgfile);

	if (path->filename)
		free(path->filename);

	free(path);


	/* 2. Return. */

	return 0;
}

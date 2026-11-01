/********************************************************************
 * dragondospath.h - Dragon DOS path definitions header file
 *
 * A Dragon DOS disk has 18 sectors of 256 bytes per track on each side.
 * Logical sectors (LSNs) number the tracks in order, and on a double
 * sided disk each track holds side 0 then side 1.
 *
 * Track 20 side 0 is the directory track, and track 16 side 0 is a copy
 * of it.  Sectors 1 and 2 are the allocation bitmap, with a set bit for
 * each free sector, each covering 1440 LSNs in its first 180 bytes.
 * Bytes $FC to $FF of sector 1 are the tracks on the disk, the sectors per
 * track (18, or 36 if double sided), and the complement of each.  Sectors
 * 3 to 18 hold 10 directory entries of 25 bytes each.
 *
 * All access goes through libdiskimage, and only sectors an operation
 * needs are touched, so an image can lack any others.  The bitmap's first
 * sector is needed to open anything, as it holds the geometry.  A
 * directory track sector that is missing or has a bad CRC is read from
 * track 16 instead, and writes reach whichever copies exist.
 *
 * Writers are expected to be alone on an image.  Sectors a file might
 * grow into stay allocated until it is closed, as they do in Dragon DOS.
 *
 * Functions return 0 on success or an EOS_ error code.  Those of
 * libdiskimage are passed on, and in addition:
 *   EOS_BPNAM  the pathlist or filename is not valid
 *   EOS_PNNF   the file does not exist
 *   EOS_FAE    the file already exists
 *   EOS_WT     the image does not hold a valid Dragon DOS disk
 *   EOS_FNA    the file is protected
 *   EOS_BMODE  the path was not opened for this kind of access
 *   EOS_DF     the disk or its directory is full
 *   EOS_EOF    a read began at or past the end of the file
 *   EOS_SE     a seek went before the start of the file
 ********************************************************************/
#ifndef	_DRAGONDOSPATH_H
#define	_DRAGONDOSPATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cocotypes.h>
#include <diskimage.h>

#define DRAGONDOS_SECTOR_SIZE		256
#define DRAGONDOS_TRACK_SECTORS		18
#define DRAGONDOS_MAX_DIR_ENTRIES	160

/* Directory entry flag bits */
#define DRAGONDOS_FLAG_DELETED		0x80
#define DRAGONDOS_FLAG_CONTINUED	0x20	/* last_or_next is the next entry */
#define DRAGONDOS_FLAG_END		0x08	/* this and all later entries are unused */
#define DRAGONDOS_FLAG_PROTECTED	0x02
#define DRAGONDOS_FLAG_CONTINUATION	0x01	/* extents only, no filename */

typedef struct
{
	u_char lsn[2];		/* big endian */
	u_char count;
} dragondos_extent;

typedef struct _dragondos_dir_entry
{
	u_char flags;
	union
	{
		struct
		{
			u_char filename[8];		/* left justified, nul filled */
			u_char file_extension[3];	/* left justified, nul filled */
			dragondos_extent extent[4];
		} header;
		struct
		{
			dragondos_extent extent[7];
			u_char unused[2];
		} continuation;
	} u;
	u_char last_or_next;
		/* The last entry of a file holds the bytes used in its last
		   sector, with 0 meaning 256.  Others hold their next entry. */
} dragondos_dir_entry;

typedef struct _dragondos_path_id
{
	int		mode;		/* access mode */
	char		*imgfile;	/* image file */
	char		*filename;	/* file within the image, as given */
	int		israw;		/* no file I/O possible, just sectors and directory */
	diskimage_id	disk;

	u_int		tracks;		/* from the bitmap */
	u_int		sectors_per_track;	/* from the bitmap: 18 or 36 */
	u_int		total_sectors;	/* the most the bitmap can cover */
	u_int		directory_lsn;
	int		linear;		/* image has no sides: a raw double sided disk */

	u_char		track[DRAGONDOS_TRACK_SECTORS][DRAGONDOS_SECTOR_SIZE];
	u_char		track_loaded[DRAGONDOS_TRACK_SECTORS];
	u_char		track_dirty[DRAGONDOS_TRACK_SECTORS];

	u_int		entry_index;	/* directory entry of this file */
	u_int		directory_entry_index;	/* next entry for _dragondos_readdir */
	u_int		filepos;
	u_int		filesize;

	u_int		spare_lsn;	/* sectors allocated, not yet part of the file */
	u_int		spare_count;
} *dragondos_path_id;

typedef struct
{
	int		protected_file;
	u_int		file_size;
} dragondos_file_stat;

/* Pathlists are <image,file>.  With no file, as in "foo.dsk,", or with "@" for
   a file, the path is raw: it has sectors and the directory, but no file.  Filenames follow
   the Dragon DOS rules: up to 8 of A-Z, 0-9 and "-", then optionally "." or
   "/" and up to 3 more.  Lower case is made upper, and extra characters
   in the name part are ignored, but not extra characters in the extension. */

/* Report whether a file is an image of a Dragon DOS disk: its geometry is
   valid, and its bitmap has the directory track and copy in use.  A file
   that can't be read as an image is not an error, but not a Dragon DOS disk. */
error_code _dragondos_identify(const char *pathname, int *found);

/* Make a new image of a blank disk, formatted as DSKINIT does.  An existing
   file is not replaced.  tracks is 21 to 80, sides is 1 or 2.  Unlike DSKINIT,
   an 80 track single sided disk has all of its sectors free. */
error_code _dragondos_dskinit(const char *pathname, diskimage_format format, u_int tracks, u_int sides);

/* mode is FAM_READ and/or FAM_WRITE, optionally with
   DISKIMAGE_IGNORE_WRITE_PROTECT. */
error_code _dragondos_open(dragondos_path_id *path, const char *pathlist, int mode);

/* Create an empty file, open for writing.  An existing file of that name is
   replaced, unless mode has FAM_NOCREATE, which makes that EOS_FAE. */
error_code _dragondos_create(dragondos_path_id *path, const char *pathlist, int mode);

/* Returns unused sectors to the bitmap. */
error_code _dragondos_close(dragondos_path_id path);

/* On entry *size is the number of bytes to transfer; on return it is the
   number transferred, including when an error is returned.  Writing past
   the end allocates sectors with the algorithm of Dragon DOS. */
error_code _dragondos_read(dragondos_path_id path, void *buffer, u_int *size);
error_code _dragondos_write(dragondos_path_id path, const void *buffer, u_int *size);

/* Read up to and including a carriage return. */
error_code _dragondos_readln(dragondos_path_id path, void *buffer, u_int *size);

/* mode is SEEK_SET, SEEK_CUR or SEEK_END.  Seeking past the end is allowed. */
error_code _dragondos_seek(dragondos_path_id path, long offset, int mode);

error_code _dragondos_kill(const char *pathlist);
error_code _dragondos_rename(const char *pathlist, const char *new_name);

/* As _dragondos_rename, and gets the renamed file's directory entry. */
error_code _dragondos_rename_ex(const char *pathlist, const char *new_name, dragondos_dir_entry *entry);

/* Read directory entries in order, unused and continuation entries too.
   EOS_EOF marks the end.  An entry that cannot be read returns its error,
   and the next call moves on to the next entry. */
error_code _dragondos_readdir(dragondos_path_id path, dragondos_dir_entry *entry);
error_code _dragondos_seekdir(dragondos_path_id path, u_int entry);

/* Make an entry's "NAME.EXT" as a C string.  name must hold 13 bytes. */
error_code _dragondos_ncpy_name(const dragondos_dir_entry *entry, char *name, size_t length);

error_code _dragondos_gs_size(dragondos_path_id path, u_int *size);
error_code _dragondos_gs_pos(dragondos_path_id path, u_int *pos);
error_code _dragondos_gs_eof(dragondos_path_id path);
error_code _dragondos_gs_fd(dragondos_path_id path, dragondos_file_stat *stat);
error_code _dragondos_gs_free(dragondos_path_id path, u_int *sectors);
error_code _dragondos_gs_sector(dragondos_path_id path, u_int lsn, void *buffer);

/* Change the file's size.  Growing does not write the new sectors. */
error_code _dragondos_ss_size(dragondos_path_id path, u_int size);

/* Only protected_file can be changed. */
error_code _dragondos_ss_fd(dragondos_path_id path, const dragondos_file_stat *stat);

/* Only a raw path can be used. */
error_code _dragondos_ss_sector(dragondos_path_id path, u_int lsn, const void *buffer);

#include <cocopath.h>

#ifdef __cplusplus
}
#endif

#endif	/* _DRAGONDOSPATH_H */

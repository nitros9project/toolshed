/********************************************************************
 * flexpath.h - FLEX path definitions header file
 *
 * A FLEX disk is addressed by track (from 0) and sector (from 1).  Every
 * sector holds 256 bytes: a link to the next sector of its chain (track,
 * sector, or 0,0 at the end), a record number, and 252 bytes of data.
 *
 * Track 0 holds the boot sectors 1 and 2, the System Information Record
 * (SIR) in sector 3, and the directory from sector 5.  The SIR names the
 * last track and the sectors on every other track, counting both sides,
 * and the chain of free sectors.  The directory is a chain of sectors of
 * 10 entries each, and grows by taking a sector from the free chain.
 *
 * On a double sided disk, sector numbers carry on across the second side:
 * 1 to 18 are on side 0 of a double density track and 19 to 36 on side 1.
 * The Color Computer's FLEX also has track 0 single density, with 10
 * sectors on each side, whatever the density of the others.
 *
 * All access goes through libdiskimage.  A raw image holds the sectors of
 * each track in order, track 0 either with only the sectors it has, or
 * padded to as many as the other tracks.  Which one is seen from the
 * image's size.  Other images are addressed by side and sector number.
 *
 * A file is a chain of sectors.  Its directory entry has the first and
 * last, how many there are, and the date.  Only whole sectors are recorded,
 * so a file's size is always a multiple of 252.  A random file starts with
 * sectors that map the rest, as many as its random flag says, which FLEX
 * makes 2.  They are not part of its data here.
 *
 * Writers are expected to be alone on an image.
 *
 * Functions return 0 on success or an EOS_ error code.  Those of
 * libdiskimage are passed on, and in addition:
 *   EOS_BPNAM  the pathlist or filename is not valid
 *   EOS_PNNF   the file does not exist
 *   EOS_FAE    the file already exists
 *   EOS_WT     the image does not hold a valid FLEX disk
 *   EOS_FNA    the file is protected, or random and can't grow
 *   EOS_BMODE  the path was not opened for this kind of access
 *   EOS_DF     the disk is full
 *   EOS_EOF    a read began at or past the end of the file
 *   EOS_SE     a seek went before the start of the file
 *   EOS_SECT   a chain leads to a sector the disk doesn't have, or loops
 ********************************************************************/
#ifndef	_FLEXPATH_H
#define	_FLEXPATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

#include <cocotypes.h>
#include <diskimage.h>

#define FLEX_SECTOR_SIZE	256
#define FLEX_DATA_OFFSET	4
#define FLEX_DATA_SIZE		(FLEX_SECTOR_SIZE - FLEX_DATA_OFFSET)
#define FLEX_SIR_SECTOR		3
#define FLEX_DIR_SECTOR		5
#define FLEX_DIR_ENTRIES	10	/* in each directory sector */
#define FLEX_DIR_HEADER		16	/* bytes before the first entry */
#define FLEX_SD_SECTORS		10	/* on each side of a single density track */
#define FLEX_DD_SECTORS		18	/* on each side of a double density track */
#define FLEX_RANDOM_MAP_SECTORS	2

/* Directory entry attribute bits */
#define FLEX_ATTR_WRITE		0x80	/* write protected */
#define FLEX_ATTR_DELETE	0x40	/* delete protected */
#define FLEX_ATTR_READ		0x20	/* read protected */
#define FLEX_ATTR_CATALOG	0x10	/* left out of a catalog */

/* The first byte of a name that has never been used, or of a deleted file */
#define FLEX_ENTRY_UNUSED	0x00
#define FLEX_ENTRY_DELETED	0x80	/* the bit set; FLEX writes $FF */

typedef struct _flex_dir_entry
{
	u_char filename[8];	/* left justified, nul filled */
	u_char file_extension[3];
	u_char attributes;
	u_char reserved1;
	u_char start_track;
	u_char start_sector;
	u_char end_track;
	u_char end_sector;
	u_char sectors[2];	/* big endian */
	u_char random;		/* nonzero for a random file */
	u_char reserved2;
	u_char month;
	u_char day;
	u_char year;		/* less 1900, or a year of the century */
} flex_dir_entry;

/* The System Information Record, decoded */
typedef struct
{
	char volume_name[12];	/* 11 bytes as recorded, nul filled like a filename, and a nul */
	u_int volume_number;
	u_int free_track;	/* first free sector */
	u_int free_sector;
	u_int last_free_track;
	u_int last_free_sector;
	u_int free_sectors;
	u_int month;
	u_int day;
	u_int year;
	u_int max_track;	/* last track */
	u_int max_sector;	/* sectors on tracks past 0, counting both sides */
} flex_sir;

typedef struct _flex_path_id
{
	int		mode;		/* access mode */
	char		*imgfile;	/* image file */
	char		*filename;	/* file within the image, as given */
	int		israw;		/* no file I/O possible, just sectors and directory */
	diskimage_id	disk;

	/* Geometry */
	int		linear;		/* raw image: sectors in file order */
	u_int		track0_slots;	/* linear: sectors kept for track 0 */
	u_int		sides;		/* not linear: sides of the image */
	u_int		track0_side0;	/* not linear: sectors on side 0 of track 0 */
	u_int		max_track;
	u_int		max_sector;

	u_char		sir[FLEX_SECTOR_SIZE];
	int		sir_dirty;

	/* The file's directory entry */
	u_int		entry_track;
	u_int		entry_sector;
	u_int		entry_slot;
	flex_dir_entry	entry;
	int		entry_dirty;

	/* The file's data */
	u_int		data_track;	/* first sector of data, past a random file's map */
	u_int		data_sector;
	u_int		data_sectors;
	u_int		filepos;
	int		making_random;	/* _flex_ss_random: the map is written on close */

	/* The sector holding filepos, or the last one before it */
	int		buffer_valid;
	int		buffer_dirty;
	u_int		buffer_index;	/* from 0, among the data sectors */
	u_int		buffer_track;
	u_int		buffer_sector;
	u_char		buffer[FLEX_SECTOR_SIZE];

	/* _flex_readdir */
	u_int		dir_track;
	u_int		dir_sector;
	u_int		dir_slot;
	u_int		dir_sectors_seen;
} *flex_path_id;

typedef struct
{
	u_int		attributes;	/* FLEX_ATTR_ bits */
	int		random;
	u_int		sectors;	/* the file's sectors, with a random file's map */
	u_int		file_size;	/* bytes of data */
	u_int		month;
	u_int		day;
	u_int		year;
	u_int		start_track;
	u_int		start_sector;
	u_int		end_track;
	u_int		end_sector;
} flex_file_stat;

/* Pathlists are <image,file>.  With no file, as in "foo.dsk,", or with "@"
   for a file, the path is raw: it has sectors and the directory, but no
   file.  A drive number in front, as in "0.NAME.TXT", is ignored.  Names are
   a letter, then up to 7 letters, digits, "-" or "_", then optionally "."
   and up to 3 more.  Extra characters in the name part are ignored, but not
   extra characters in the extension.  Names match whatever their case, and
   new names are made upper case. */

/* Report whether a file is an image of a FLEX disk: it has a SIR with a
   believable geometry and free chain, and the image is big enough to hold
   it.  A file that can't be read as an image is not an error, but not a
   FLEX disk. */
error_code _flex_identify(const char *pathname, int *found);

/* Report how a raw image of a FLEX disk lays out its sectors.  A raw image
   has no tracks of its own, so FLEX's are taken from the SIR and the image's
   size: track0_sectors on track 0, and track_sectors, counting both sides,
   on every other.  found is 0, and the counts are left alone, if the file
   is not a raw image of a FLEX disk. */
error_code _flex_raw_layout(const char *pathname, int *found, u_int *track0_sectors, u_int *track_sectors);

/* Make a new image of a blank disk, as NEWDISK does.  An existing file is
   not replaced.  tracks is 2 to 255, sides is 1 or 2.  A double density disk
   still has track 0 single density.  DMK and SDF images record that; a raw
   image keeps only track 0's sectors unless padded, which fills it out to
   as many as the others have.  JVC and VDK images can't have a short track
   0, so they are always padded.  The date is from GetSafeTimestamp. */
error_code _flex_dskinit(const char *pathname, diskimage_format format, u_int tracks, u_int sides,
			 int double_density, int padded, const char *volume_name, u_int volume_number);

/* mode is FAM_READ and/or FAM_WRITE, optionally with
   DISKIMAGE_IGNORE_WRITE_PROTECT.  A write protected file can be opened
   for writing, to change its attributes, but not written.  A read
   protected file can still be read, though FLEX refuses to. */
error_code _flex_open(flex_path_id *path, const char *pathlist, int mode);

/* Create an empty file, open for writing.  An existing file of that name is
   replaced, unless mode has FAM_NOCREATE, which makes that EOS_FAE.  A
   protected file is never replaced. */
error_code _flex_create(flex_path_id *path, const char *pathlist, int mode);

/* A file written to and left with no sectors is deleted, as FLEX does. */
error_code _flex_close(flex_path_id path);

/* On entry *size is the number of bytes to transfer; on return it is the
   number transferred, including when an error is returned.  Writing past
   the end takes sectors from the start of the free chain.  A write may start
   past the end, which fills the sectors between with zeros.  A random file
   can be written, but not past its end. */
error_code _flex_read(flex_path_id path, void *buffer, u_int *size);
error_code _flex_write(flex_path_id path, const void *buffer, u_int *size);

/* Read up to and including a carriage return. */
error_code _flex_readln(flex_path_id path, void *buffer, u_int *size);

/* mode is SEEK_SET, SEEK_CUR or SEEK_END.  Seeking past the end is allowed. */
error_code _flex_seek(flex_path_id path, long offset, int mode);

/* Deleting gives the file's sectors to the end of the free chain and puts
   $FF at the start of its name, as FLEX does.  As in FLEX, a file that is
   write, delete or read protected can't be deleted or renamed. */
error_code _flex_kill(const char *pathlist);
error_code _flex_rename(const char *pathlist, const char *new_name);

/* As _flex_rename, and gets the renamed file's directory entry. */
error_code _flex_rename_ex(const char *pathlist, const char *new_name, flex_dir_entry *entry);

/* Read directory entries in order, deleted entries too.  EOS_EOF marks the
   end, which as in FLEX is the first entry never used, or the end of the
   directory's chain.  A directory sector that cannot be read returns its
   error, and the directory is taken to end there. */
error_code _flex_readdir(flex_path_id path, flex_dir_entry *entry);
error_code _flex_rewinddir(flex_path_id path);

/* Whether an entry is that of a file. */
int _flex_entry_in_use(const flex_dir_entry *entry);

/* Make an entry's "NAME.EXT" as a C string.  name should hold 13 bytes. */
error_code _flex_ncpy_name(const flex_dir_entry *entry, char *name, size_t length);

/* Number of sectors in an entry's file, with a random file's map. */
u_int _flex_entry_sectors(const flex_dir_entry *entry);

error_code _flex_gs_size(flex_path_id path, u_int *size);
error_code _flex_gs_pos(flex_path_id path, u_int *pos);
error_code _flex_gs_eof(flex_path_id path);
error_code _flex_gs_fd(flex_path_id path, flex_file_stat *stat);
error_code _flex_gs_sir(flex_path_id path, flex_sir *sir);
error_code _flex_gs_free(flex_path_id path, u_int *sectors);
error_code _flex_gs_sector(flex_path_id path, u_int track, u_int sector, void *buffer);

/* Change the file's size, to a whole number of sectors.  Growing fills the
   new sectors with zeros. */
error_code _flex_ss_size(flex_path_id path, u_int size);

/* Only the attributes and the date can be changed. */
error_code _flex_ss_fd(flex_path_id path, const flex_file_stat *stat);

/* Make a file just created a random file, as FLEX's random files are: two
   map sectors come before its data, and the map of its data, as runs of
   sectors, is written when it is closed.  The file grows as it is written
   until then.  EOS_BMODE if anything has been written.  A file whose data is
   in more runs than two map sectors hold, 168, can't be closed: EOS_DF. */
error_code _flex_ss_random(flex_path_id path);

/* Set a stat's date from a time, in local time.  If SOURCE_DATE_EPOCH is set,
   it is used instead, in UTC, as the date of every new file and disk. */
void _flex_stat_date(flex_file_stat *stat, time_t when);

/* If SOURCE_DATE_EPOCH is set, set stat's date from it, in UTC, and return 1.
   Otherwise stat is left as it is, and the result is 0. */
int _flex_reproducible_date(flex_file_stat *stat);

/* Only a raw path can be used. */
error_code _flex_ss_sector(flex_path_id path, u_int track, u_int sector, const void *buffer);

/* Count the sectors of the free chain by following it to the last free
   sector the SIR names.  EOS_SECT if it leads off the disk, loops or ends
   before then. */
error_code _flex_count_free_chain(flex_path_id path, u_int *sectors);

/* Text files end lines with a carriage return, and record a run of spaces
   as $09 and a count.  Nuls are padding.  _flex_text_to_native makes lines
   end with a line feed, expands spaces, and drops nul and $18, as FLEX does
   when it reads text.  _flex_text_from_native ends lines with a carriage
   return, makes a tab the spaces to the next multiple of 8 columns, and
   records runs of 2 to 127 spaces as FLEX writes them.  The caller frees
   out_buffer. */
error_code _flex_text_to_native(const u_char *in_buffer, u_int in_size, u_char **out_buffer, u_int *out_size);
error_code _flex_text_from_native(const u_char *in_buffer, u_int in_size, u_char **out_buffer, u_int *out_size);

/* A binary file is records of $02, a load address, a count and that many
   bytes, and $16 and a transfer address.  Nuls between records are padding.
   This finds the lowest and highest address loaded, and the transfer
   address.  EOS_WT if the file isn't made of these records, and has_transfer
   is 0 if it has no transfer address. */
error_code _flex_binary_info(const u_char *buffer, u_int size, u_int *load_low, u_int *load_high,
			     u_int *transfer, int *has_transfer, u_int *records);

/* A Color Computer boot disk, as FHL's PUTLDR and LINK make one */
#define FLEX_BOOT_TRACK		16
#define FLEX_BOOT_LOADER_SECTOR	9
#define FLEX_BOOT_LINK_OFFSET	5	/* in the loader: the file's first track and sector */

/* Make a disk able to boot FLEX on the Color Computer, with RUN "FLEX" in
   Disk BASIC, as FHL's PUTLDR does.  pathlist names the disk, as "disk,".
   The disk becomes a Disk BASIC disk too, holding FLEX.BAS, which runs the
   loader in track 16 sector 9.  Track 16 sectors 1 to 9 and track 17
   sectors 2 and 3 are taken from the free chain.  FLEX.BAS and the loader
   are ToolShed's own, and the loader reads sectors with Disk BASIC.

   The disk must be double density, 17 or 18 sectors a side, with at least
   19 tracks, as PUTLDR requires: EOS_IA if not.  The sectors must be free, or the disk set up
   before, which is set up again, linked to nothing: EOS_FNA if not.  Then
   use _flex_link. */
error_code _flex_putldr(const char *pathlist);

/* Make the loader of a boot disk load a file, as FLEX's LINK does.  The
   file's first track and sector go in the loader.  EOS_FNA if the disk
   isn't set up by _flex_putldr.  The loader reads only side 0, so a file
   with a sector on side 1, or on track 0, is EOS_IA. */
error_code _flex_link(const char *pathlist);

#include <cocopath.h>

#ifdef __cplusplus
}
#endif

#endif	/* _FLEXPATH_H */

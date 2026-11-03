/********************************************************************
 * diskimage.h - Common access to floppy disk image files
 *
 * Supported formats are DMK, JVC, SDF, VDK and raw images (an unadorned
 * array of 256 byte sectors, 18 sectors per track).  A JVC image with
 * no header is byte for byte a raw image, and is reported as raw.
 *
 * Sectors are addressed by physical cylinder and side (both from zero)
 * and by the sector number recorded in the sector's ID field.
 *
 * The logical sectors of an image are those a floppy controller could
 * find, ordered by cylinder, then side, then ascending sector number.  In
 * DMK and SDF images these are not logical sectors:
 *   - sectors whose ID field has a bad CRC
 *   - sectors whose ID track number is not the physical cylinder
 *   - sectors with no data field
 *   - sectors with a field that crosses the end of the track
 *   - later sectors with the number of an earlier one on the same track
 * In JVC images with sector attributes, sectors marked "not found" are
 * also not logical sectors.
 *
 * A raw image can have a short track 0, as OS-9 disks can.  Its file keeps
 * all 18 slots of track 0, and those past the short count are not logical
 * sectors.  This is read from LSN0 when the image is opened, so writing
 * a new LSN0 takes effect the next time it is opened.
 *
 * Tracks of an existing image cannot be formatted and missing sectors
 * cannot be added.  Only a raw image can grow: writing past its end
 * extends the file, filling any new sector or part of a sector not
 * written with $FF.
 *
 * Functions return 0 on success or an EOS_ error code:
 *   EOS_PNNF, EOS_FNA  the file could not be opened
 *   EOS_FAE            the file to create already exists
 *   EOS_WT             the file is not a recognized disk image
 *   EOS_BMODE          the image was not opened for this kind of access
 *   EOS_IA             an argument is invalid
 *   EOS_SECT           the sector does not exist
 *   EOS_CRC            the sector's data field has a bad CRC
 *   EOS_WP             the image's write protect flag is set
 *   EOS_WRITE          the file could not be written, or the sector
 *                      overlaps another sector's ID field
 *   EOS_READ           the file could not be read
 *   EOS_DF             a raw image cannot grow any larger
 ********************************************************************/
#ifndef	_DISKIMAGE_H
#define	_DISKIMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cocotypes.h>

#define EOS_SECT	241
#define EOS_WP		242
#define EOS_READ	244

typedef enum
{
	DISKIMAGE_UNKNOWN = 0,
	DISKIMAGE_RAW,
	DISKIMAGE_JVC,
	DISKIMAGE_DMK,
	DISKIMAGE_SDF,
	DISKIMAGE_VDK
} diskimage_format;

/* _diskimage_open mode flag, beyond the FAM_ flags */
#define DISKIMAGE_IGNORE_WRITE_PROTECT	0x0400

typedef struct _diskimage_path *diskimage_id;

typedef struct
{
	diskimage_format format;
	u_int cylinders;
	u_int sides;
	u_int sectors_per_track;	/* most logical sectors on any track */
	u_int sector_size;		/* largest logical sector, in bytes */
	u_int first_sector;		/* lowest sector number in use */
	u_int sectors_track0;		/* logical sectors on side 0 of track 0 */
	u_int total_sectors;		/* logical sectors in the image */
	int uniform;			/* every track has the same sectors */
	int write_protected;		/* the image's write protect flag */
	int extendable;			/* writes can grow the image */
} diskimage_geometry;

/* diskimage_sector_info flags */
#define DISKIMAGE_SECTOR_DELETED	0x01
#define DISKIMAGE_SECTOR_CRC_ERROR	0x02

typedef struct
{
	u_int cylinder;
	u_int side;
	u_int sector;
	u_int size;
	u_int flags;
} diskimage_sector_info;

/* Regions in the host file, not in the logical sector stream. End offsets
   are exclusive. Sector numbers are the recorded IDs, not logical indexes. */
typedef enum
{
	DISKIMAGE_BYTE_GAP = 0,
	DISKIMAGE_BYTE_HEADER,
	DISKIMAGE_BYTE_TRACK_HEADER,
	DISKIMAGE_BYTE_ID,
	DISKIMAGE_BYTE_DATA_MARK,
	DISKIMAGE_BYTE_DATA,
	DISKIMAGE_BYTE_CRC,
	DISKIMAGE_BYTE_ATTRIBUTE,
	DISKIMAGE_BYTE_UNUSED
} diskimage_byte_kind;

typedef struct
{
	diskimage_byte_kind kind;
	long long start;
	long long end;
	diskimage_sector_info sector;
	u_int step;		/* file bytes per decoded byte (1 or 2) */
	int logical;		/* this is the sector used by sector reads */
} diskimage_byte_region;

typedef struct
{
	long long file_size;
	diskimage_byte_region region;
	int has_previous;
	int has_next;
	diskimage_byte_region previous;
	diskimage_byte_region next;
} diskimage_byte_info;

/* The format with a name: raw, jvc, vdk, dmk or sdf, in either case.  Any other
   name is DISKIMAGE_UNKNOWN. */
diskimage_format _diskimage_format_named(const char *name);

/* The format a pathname's extension names, or DISKIMAGE_UNKNOWN if none. */
diskimage_format _diskimage_format_of_extension(const char *pathname);

/* Report a file's format.  An unrecognized file is not an error: its
   format is DISKIMAGE_UNKNOWN. */
error_code _diskimage_identify(const char *pathname, diskimage_format *format);

/* Create a new image with every sector formatted and holding fill.  An
   existing file is not replaced.  Sectors are numbered from 1.

   sector_size is 128, 256, 512 or 1024.  sectors_track0 is the number of
   sectors on side 0 of track 0; zero means sectors_per_track.

   interleave is how many positions apart on a track successive sector
   numbers are placed: 3 records 1, 7, 13, 2, 8, 14... on an 18 sector track.
   0 and 1 both mean in order.  It must be less than sectors_per_track, unless
   it is 1.  A position already taken moves on to the next free one.  It only
   changes the order on the track: sectors are still addressed by number, and
   logical sectors still follow ascending numbers.  Raw, JVC and VDK images do
   not record an order, so they ignore it.

   The result is EOS_IA if the format cannot represent the request:
     raw      256 byte sectors, 18 sectors per track, and no more than
              that on track 0
     JVC      no different track 0, at most 255 sectors per track, and
              sector data must total a multiple of 256 bytes
     VDK      256 byte sectors, 18 sectors per track, no different track 0,
              at most 255 tracks
     DMK/SDF  double density sectors that fit in a 6250 byte track;
              DMK has at most 255 tracks and 64 sectors per track,
              SDF at most 80 tracks and 31 sectors per track

   A raw image has no sides of its own, so two sides simply double its
   size, and it opens as one side of twice the tracks.  A JVC image of 1
   side and 18 sectors of 256 bytes needs no header, and so opens as raw.

   A raw image with a short track 0 records it in LSN0, the only place it
   can.  Its first sector then holds the OS-9 fields for total sectors,
   sectors per track and sectors on track 0, not only fill. */
error_code _diskimage_create(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
			     u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
			     u_int interleave, u_char fill);

/* As _diskimage_create, but each track of a DMK or SDF image starts with an
   index mark, moving its sectors 67 bytes later.  This is how a disk formatted
   by Dragon DOS is recorded by MAME.  Other formats have no tracks to differ. */
error_code _diskimage_create_index_mark(const char *pathname, diskimage_format format, u_int sides, u_int tracks,
					u_int sector_size, u_int sectors_per_track, u_int sectors_track0,
					u_int interleave, u_char fill);

/* Open an existing image.  mode is FAM_READ and/or FAM_WRITE, optionally with
   DISKIMAGE_IGNORE_WRITE_PROTECT, which allows writes to a DMK, SDF or VDK
   image whose write protect flag is set.  The flag itself is left as it is,
   and is still reported by _diskimage_query_geometry. */
error_code _diskimage_open(diskimage_id *disk, const char *pathname, int mode);

/* Close an image. */
error_code _diskimage_close(diskimage_id disk);

/* Set or clear the write protect flag of a DMK, SDF or VDK image.  Raw and JVC
   images have no such flag, so for them the result is EOS_IA. */
error_code _diskimage_set_write_protect(const char *pathname, int write_protect);

error_code _diskimage_query_geometry(diskimage_id disk, diskimage_geometry *geometry);

/* Describe a logical sector, by its position (from zero) in the ordering
   above.  Use this to find the layout of an image that is not uniform. */
error_code _diskimage_query_sector(diskimage_id disk, u_int index, diskimage_sector_info *info);

/* Describe an actual host-file byte and its surrounding fields in file
   order, including non-logical sector IDs. EOS_EOF means offset is at or
   beyond the file's end; file_size is still returned in that case. */
error_code _diskimage_query_byte(diskimage_id disk, long long offset, diskimage_byte_info *info);

/* Read one sector.  On entry *size is the size of buffer; on return it is
   the size of the sector.  If buffer is too small the result is EOS_IA
   and nothing is read.  EOS_CRC still delivers the data as recorded. */
error_code _diskimage_read_sector(diskimage_id disk, u_int cylinder, u_int side, u_int sector, void *buffer, u_int *size);

/* Write one whole sector.  size must be the size of the sector.  A CRC
   error recorded for the sector is cleared; a Deleted Data Mark is kept. */
error_code _diskimage_write_sector(diskimage_id disk, u_int cylinder, u_int side, u_int sector, const void *buffer, u_int size);

#ifdef __cplusplus
}
#endif

#endif	/* _DISKIMAGE_H */

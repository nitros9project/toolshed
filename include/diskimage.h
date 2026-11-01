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
 * find, ordered by cylinder, then side, then ascending sector number.
 * The stream functions treat them as one contiguous run of bytes.  In
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
 * all 18 slots of track 0, unused past the short count, and the stream
 * skips them.  This is read from LSN0 when the image is opened, so writing
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
 *   EOS_EOF            a stream read began at or past the end
 *   EOS_DF             a stream write reached the end of the image
 *   EOS_SE             a stream seek went before the start
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
typedef struct _diskimage_stream *diskimage_stream_id;

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
	long long stream_size;		/* bytes in all logical sectors */
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
	long long stream_offset;	/* where the sector begins in the stream */
} diskimage_sector_info;

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
   the stream still follows ascending numbers.  Raw, JVC and VDK images do
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

/* Close an image and any of its streams still open. */
error_code _diskimage_close(diskimage_id disk);

/* Set or clear the write protect flag of a DMK, SDF or VDK image.  Raw and JVC
   images have no such flag, so for them the result is EOS_IA. */
error_code _diskimage_set_write_protect(const char *pathname, int write_protect);

error_code _diskimage_query_geometry(diskimage_id disk, diskimage_geometry *geometry);

/* Describe a logical sector, by its position (from zero) in the ordering
   above.  Use this to find the layout of an image that is not uniform. */
error_code _diskimage_query_sector(diskimage_id disk, u_int index, diskimage_sector_info *info);

/* Read one sector.  On entry *size is the size of buffer; on return it is
   the size of the sector.  If buffer is too small the result is EOS_IA
   and nothing is read.  EOS_CRC still delivers the data as recorded. */
error_code _diskimage_read_sector(diskimage_id disk, u_int cylinder, u_int side, u_int sector, void *buffer, u_int *size);

/* Write one whole sector.  size must be the size of the sector.  A CRC
   error recorded for the sector is cleared; a Deleted Data Mark is kept. */
error_code _diskimage_write_sector(diskimage_id disk, u_int cylinder, u_int side, u_int sector, const void *buffer, u_int size);

/* Open a stream positioned at the start of the image.  An image may have
   many streams, and each keeps its own position. */
error_code _diskimage_stream_open(diskimage_stream_id *stream, diskimage_id disk);

/* On entry *size is the number of bytes to transfer; on return it is the
   number transferred, including when an error is returned.  A read that
   reaches the end returns the bytes available, then EOS_EOF on the next
   call.  A read stops at a sector with a bad CRC, before that sector;
   _diskimage_read_sector can still read it. */
error_code _diskimage_stream_read(diskimage_stream_id stream, void *buffer, u_int *size);
error_code _diskimage_stream_write(diskimage_stream_id stream, const void *buffer, u_int *size);

/* mode is SEEK_SET, SEEK_CUR or SEEK_END.  Seeking past the end is allowed. */
error_code _diskimage_stream_seek(diskimage_stream_id stream, long long offset, int mode);
error_code _diskimage_stream_tell(diskimage_stream_id stream, long long *offset);

error_code _diskimage_stream_close(diskimage_stream_id stream);

#ifdef __cplusplus
}
#endif

#endif	/* _DISKIMAGE_H */

/********************************************************************
 * libdiskimageint.h - Definitions private to libdiskimage
 *
 * Raw images and JVC images without sector attributes are linear: a
 * sector's position in the file follows from its logical index.  Other
 * images have a table of their logical sectors, built when opened and
 * kept in logical order.
 ********************************************************************/
#ifndef	_LIBDISKIMAGEINT_H
#define	_LIBDISKIMAGEINT_H

#include <stdio.h>

#include <cocopath.h>
#include <diskimage.h>

#define DI_RAW_SECTOR_SIZE	256
#define DI_RAW_SECTORS_PER_TRACK	18
#define DI_RAW_FILL		0xFF
#define DI_RAW_MAX_SECTORS	0x1000000	/* limit of a 3 byte LSN */
#define DI_MAX_SECTOR_SIZE	1024
#define DI_MAX_TRACK_IDS	256
#define DI_HEADER_PROBE		16
#define DI_MFM_TRACK_SIZE	6250	/* bytes in a double density track at 300 rpm */

/* di_sector flags, beyond the DISKIMAGE_SECTOR_ flags */
#define DI_FLAG_PUBLIC		(DISKIMAGE_SECTOR_DELETED | DISKIMAGE_SECTOR_CRC_ERROR)
#define DI_FLAG_DOUBLE_DENSITY	0x40
#define DI_FLAG_OVERLAP		0x80

#define DI_CRC_INIT_SD		0xFFFF
#define DI_CRC_INIT_DD		0xCDB4	/* $FFFF after the three $A1 sync bytes */

typedef struct
{
	long long data_off;	/* file offset of the first data byte */
	long long status_off;	/* file offset of the SDF data offset field or JVC attribute */
	long long stream_off;
	unsigned short cylinder;
	unsigned short size;
	u_char side;
	u_char id;
	u_char mark;		/* data address mark */
	u_char step;		/* file bytes per data byte */
	u_char flags;
} di_sector;

/* A sector ID found on a track, whether or not it is a logical sector. */
typedef struct
{
	di_sector sector;
	int valid;
	long long id_start;
	long long id_end;
} di_track_id;

struct _diskimage_path
{
	FILE *fd;
	int mode;
	diskimage_format format;
	int write_protected;
	u_int cylinders;
	u_int sides;
	u_int total_sectors;
	long long stream_size;

	/* Linear images */
	int linear;
	long long data_start;
	u_int sector_size;
	u_int sectors_per_track;
	u_int first_sector;

	/* Raw images with a short track 0 keep its unused slots in the file,
	   so a logical index past them is raw_missing short of its file slot. */
	u_int raw_track0;
	u_int raw_missing;
	long long raw_slots;	/* whole sectors in the file */

	/* Other images */
	di_sector *table;
	u_int table_capacity;

	struct _diskimage_stream *streams;
};

struct _diskimage_stream
{
	diskimage_id disk;
	long long position;
	struct _diskimage_stream *next;
};

typedef struct
{
	u_int sectors_per_track;
	u_int sides;
	u_int size_code;
	u_int first_sector;
	u_int attribute;
	u_int header_size;
	u_int cylinders;
} di_jvc_header;

/* What a new image holds. */
typedef struct
{
	u_int cylinders;
	u_int sides;
	u_int sector_size;
	u_int size_code;
	u_int sectors_per_track;
	u_int sectors_track0;
	u_int interleave;
	u_char fill;
	int index_mark;		/* tracks of a DMK or SDF image start with an index mark */
} di_layout;

/* libdiskimagecreate.c */
error_code di_file_write(FILE *fd, const void *buffer, size_t size);
int di_mfm_track(const di_layout *layout, u_int cylinder, u_int side, u_char *raw, int *id_pos, int *data_pos);
void di_raw_stamp_lsn0(u_char *sector, const di_layout *layout, long long total);

/* libdiskimagetable.c */
unsigned short di_crc16(unsigned short crc, const u_char *data, size_t size);
error_code di_read_at(diskimage_id disk, long long offset, void *buffer, size_t size);
error_code di_write_at(diskimage_id disk, long long offset, const void *buffer, size_t size);
error_code di_table_append(diskimage_id disk, const di_sector *sector);
error_code di_table_add_track(diskimage_id disk, di_track_id *ids, int count);
long long di_table_find(diskimage_id disk, u_int cylinder, u_int side, u_int id);

/* libdiskimagedmk.c */
int di_dmk_probe(const u_char *header, long long file_size);
error_code di_dmk_load(diskimage_id disk, const u_char *header);
error_code di_dmk_create(FILE *fd, const di_layout *layout);

/* libdiskimagesdf.c */
int di_sdf_probe(const u_char *header, long long file_size);
error_code di_sdf_load(diskimage_id disk, const u_char *header);
error_code di_sdf_create(FILE *fd, const di_layout *layout);

/* libdiskimagejvc.c */
int di_jvc_parse(const u_char *header, long long file_size, di_jvc_header *jvc);
error_code di_jvc_load(diskimage_id disk, const di_jvc_header *jvc);

/* libdiskimagevdk.c */
int di_vdk_probe(const u_char *header, long long file_size);
void di_vdk_load(diskimage_id disk, const u_char *header, long long file_size);

/* libdiskimagesector.c */
int di_write_blocked(diskimage_id disk);
u_int di_index_size(diskimage_id disk, long long index);
long long di_linear_slot(diskimage_id disk, long long index);
error_code di_index_read(diskimage_id disk, long long index, void *buffer);
error_code di_index_write(diskimage_id disk, long long index, const void *buffer);

#endif	/* _LIBDISKIMAGEINT_H */

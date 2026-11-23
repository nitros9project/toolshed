/********************************************************************
 * libflexint.h - Internals of libflex
 ********************************************************************/
#ifndef	_LIBFLEXINT_H
#define	_LIBFLEXINT_H

#include "flexpath.h"

#define FX_ENTRY_SIZE		24
#define FX_ATTR_DEFINED		(FLEX_ATTR_WRITE | FLEX_ATTR_DELETE | FLEX_ATTR_READ | FLEX_ATTR_CATALOG)

/* FLEX refuses to delete or rename a file with any of these. */
#define FX_ATTR_UNDELETABLE	(FLEX_ATTR_WRITE | FLEX_ATTR_DELETE | FLEX_ATTR_READ)

/* SIR offsets */
#define FX_SIR_NAME		0x10
#define FX_SIR_NAME_SIZE	11
#define FX_SIR_NUMBER		0x1B
#define FX_SIR_FREE		0x1D
#define FX_SIR_LAST_FREE	0x1F
#define FX_SIR_FREE_COUNT	0x21
#define FX_SIR_DATE		0x23
#define FX_SIR_MAX_TRACK	0x26
#define FX_SIR_MAX_SECTOR	0x27

/* libflexdisk.c */
int _fx_valid_address(flex_path_id path, u_int track, u_int sector);
u_int _fx_total_sectors(flex_path_id path);
error_code _fx_read_sector(flex_path_id path, u_int track, u_int sector, u_char *buffer);
error_code _fx_write_sector(flex_path_id path, u_int track, u_int sector, const u_char *buffer);
error_code _fx_load_geometry(flex_path_id path);
void _fx_decode_sir(const u_char *sector, flex_sir *sir);
void _fx_set_free_chain(flex_path_id path, u_int first_track, u_int first_sector,
			u_int last_track, u_int last_sector, u_int count);
error_code _fx_flush(flex_path_id path);
u_int _fx_get2(const u_char *p);
void _fx_put2(u_char *p, u_int value);
void _fx_today(u_char *date);

/* libflexdir.c */
error_code _fx_parse_name(const char *name, u_char *filename_and_extension);
error_code _fx_find_file(flex_path_id path, const u_char *name, u_int *track, u_int *sector, u_int *slot,
			 flex_dir_entry *entry);
error_code _fx_find_free_entry(flex_path_id path, u_int *track, u_int *sector, u_int *slot);
error_code _fx_read_entry(flex_path_id path, u_int track, u_int sector, u_int slot, flex_dir_entry *entry);
error_code _fx_write_entry(flex_path_id path, u_int track, u_int sector, u_int slot, const flex_dir_entry *entry);

/* libflexchain.c */
error_code _fx_allocate(flex_path_id path, u_int *track, u_int *sector);
error_code _fx_release(flex_path_id path, u_int first_track, u_int first_sector,
		       u_int last_track, u_int last_sector, u_int count);
error_code _fx_follow(flex_path_id path, u_int track, u_int sector, u_int steps, u_int *next_track, u_int *next_sector);

/* libflexread.c */
error_code _fx_load_data(flex_path_id path, u_int index);
error_code _fx_set_data_start(flex_path_id path);

/* libflexwrite.c */
error_code _fx_flush_buffer(flex_path_id path);
error_code _fx_truncate(flex_path_id path, u_int data_sectors);
error_code _fx_extend(flex_path_id path, u_int data_sectors);

/* libflexopen.c */
error_code _fx_open_image(flex_path_id *path, const char *pathlist, int mode);
error_code _fx_close_image(flex_path_id path, error_code ec);

#endif	/* _LIBFLEXINT_H */

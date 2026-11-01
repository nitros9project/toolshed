/********************************************************************
 * libdragondosint.h - Internals of libdragondos
 ********************************************************************/
#ifndef	_LIBDRAGONDOSINT_H
#define	_LIBDRAGONDOSINT_H

#include "dragondospath.h"

#define DD_BITMAP_SECTOR_LSNS	1440
#define DD_BITMAP_BYTES		180
#define DD_MAX_LSN		2880
#define DD_ENTRIES_PER_SECTOR	10
#define DD_ENTRY_SIZE		25
#define DD_NO_HINT		0xFFFF
#define DD_MAX_EXTENT		255
#define DD_KILLED_FLAGS		(DRAGONDOS_FLAG_DELETED | DRAGONDOS_FLAG_CONTINUATION)
#define DD_BACKUP_TRACKS_BACK	4	/* from the directory track */

/* libdragondosdisk.c */
error_code _dd_read_lsn(dragondos_path_id path, u_int lsn, u_char *buffer);
error_code _dd_write_lsn(dragondos_path_id path, u_int lsn, const u_char *buffer);
error_code _dd_set_geometry(dragondos_path_id path, const u_char *bitmap_sector);
error_code _dd_load_geometry(dragondos_path_id path);
error_code _dd_track_sector(dragondos_path_id path, u_int index, u_char **sector);
void _dd_track_dirty(dragondos_path_id path, u_int index);
error_code _dd_flush(dragondos_path_id path);

/* libdragondosdir.c */
error_code _dd_parse_name(const char *name, u_char *filename_and_extension);
error_code _dd_entry(dragondos_path_id path, u_int index, dragondos_dir_entry **entry);
void _dd_entry_dirty(dragondos_path_id path, u_int index);
error_code _dd_find_file(dragondos_path_id path, const u_char *name, u_int *index);
error_code _dd_find_free_entry(dragondos_path_id path, u_int first, u_int *index);
error_code _dd_next_entry(const dragondos_dir_entry *entry, u_int *next, int *more);
void _dd_extents(dragondos_dir_entry *entry, dragondos_extent **extent, int *count);
error_code _dd_file_size(dragondos_path_id path, u_int *size);
error_code _dd_sector_lsn(dragondos_path_id path, u_int file_sector, u_int *lsn);

/* libdragondosbitmap.c */
error_code _dd_sector_free(dragondos_path_id path, u_int lsn, int *is_free);
error_code _dd_allocate(dragondos_path_id path, u_int hint, u_int *lsn, u_int *count);
error_code _dd_release(dragondos_path_id path, u_int lsn, u_int count);

/* libdragondoswrite.c */
error_code _dd_truncate(dragondos_path_id path, u_int size);
error_code _dd_extend(dragondos_path_id path, u_int size);
error_code _dd_writable_file(dragondos_path_id path);

/* libdragondosopen.c */
error_code _dd_open_image(dragondos_path_id *path, const char *pathlist, int mode);
void _dd_free_path(dragondos_path_id path);

#endif	/* _LIBDRAGONDOSINT_H */

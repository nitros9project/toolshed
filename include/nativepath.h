/********************************************************************
 * nativepath.h - Native path definitions header file
 *
 * $Id$
 * A conditional removed from an #include RG
 ********************************************************************/
#ifndef	_NATIVEPATH_H
#define	_NATIVEPATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cococonv.h>
/* Turbo had ifndef, msys needs ifdef; was WIN32
  Removing the conditional works for both. RG */
#include <dirent.h>

typedef struct _native_path_id
{
	int			mode;		/* access mode */
	char		pathlist[512];	/* pointer to pathlist */
	FILE		*fd;		/* file path pointer */
	DIR			*dirhandle;
} *native_path_id;


typedef struct dirent		native_dir_entry;


/* prototypes */

error_code _native_open(native_path_id *, char *, int);
error_code _native_create(native_path_id *, char *, int, int);
error_code _native_read(native_path_id, void *, u_int *);
error_code _native_readdir(native_path_id, native_dir_entry *);
error_code _native_ncpy_name(native_dir_entry e, u_char *name, size_t len);
error_code _native_seek(native_path_id, int, int);
error_code _native_allbit(u_char *bitmap, int firstbit, int numbits);
error_code _native_delbit(u_char *bitmap, int firstbit, int numbits);
error_code _native_readln(native_path_id, void *, u_int *);
error_code _native_write(native_path_id, void *, u_int *);
error_code _native_writeln(native_path_id, char *, u_int *);
error_code _native_makdir(char *pathlist);
error_code _native_delete(char *pathlist);
error_code _native_delete_directory(char *pathlist);
error_code _native_truncate(char *pathlist, off_t length);
error_code _native_rename(char *pathlist, char *new_name);
error_code _native_close(native_path_id);

/* gs.c */
error_code _native_gs_attr(native_path_id, int *);
error_code _native_gs_eof(native_path_id path);
error_code _native_gs_fd(native_path_id, struct stat *);
error_code _native_gs_fd_pathlist(char *pathlist, struct stat *statbuf);
error_code _native_gs_size(native_path_id path, u_int *size);
error_code _native_gs_pos(native_path_id path, u_int *pos);

/* ss.c */
error_code _native_ss_attr(native_path_id, int);
error_code _native_ss_fd(native_path_id, struct stat *);
error_code _native_ss_size(native_path_id path, int size);

#include "cocopath.h"

#ifdef __cplusplus
}
#endif

#endif	/* _NATIVEPATH_H */


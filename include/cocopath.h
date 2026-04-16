/********************************************************************
 * cocopath.h - CoCo path definitions header file
 *
 * $Id$
 ********************************************************************/
#ifndef	_COCOPATH_H
#define	_COCOPATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cococonv.h>
#ifndef WIN32
#include <dirent.h>
#endif

#include "nativepath.h"
#include "os9path.h"
#include "decbpath.h"
#include "cecbpath.h"

typedef enum { NATIVE, OS9, DECB, CECB } _path_type;


/* ERROR CODES */
#define EOS_IA		187
#define EOS_IC		192
#define EOS_PTHFUL	200
#define EOS_BMODE	203
#define EOS_MF      207
#define	EOS_EOF		211
#define EOS_FNA		214
#define EOS_BPNAM	215
#define EOS_PNNF	216
#define EOS_SF		217
#define EOS_FAE		218
#define EOS_CRC		243
#define EOS_WRITE	246
#define EOS_SE		247
#define EOS_DF		248
#define EOS_WT		249
#define EOS_TIMEOUT 1024

/* file access modes */
#define	FAM_READ	0x0001
#define	FAM_WRITE	0x0002
#define	FAM_EXEC	0x0004
#define FAM_DIR		0x0080
#define FAM_NOCREATE	0x0100
#define FAM_RAW		0x0200

/* file access permissions */
#define FAP_READ	0x01
#define FAP_WRITE	0x02
#define FAP_EXEC	0x04
#define FAP_PREAD	0x08
#define FAP_PWRITE	0x10
#define FAP_PEXEC	0x20
#define FAP_SINGLE	0x40
#define FAP_DIR		0x80



/* Common CoCo structures. */

typedef struct _coco_path_id
{
	_path_type   type;
	
	union
	{
		struct _native_path_id  *native;
		struct _os9_path_id		*os9;
		struct _decb_path_id	*decb;
		struct _cecb_path_id	*cecb;
	} path;
} *coco_path_id;


typedef struct coco_dir_entry
{
	_path_type   type;
	
	union
	{
#ifdef WIN32
		struct _finddata_t  native;
#else
		struct dirent		native;
#endif
		os9_dir_entry			os9;
		decb_dir_entry			decb;
		cecb_dir_entry			cecb;
	} dentry;
} coco_dir_entry;


typedef struct coco_file_stat
{
	int					perms;
	int					attributes;
	int					user_id;
	int					group_id;
	time_t				create_time;
	time_t				last_modified_time;
	u_char				file_type;
	u_char				data_type;
	u_char				gap_flag;
	int					ml_load_address;
	int					ml_exec_address;
} coco_file_stat;


/* prototypes */

error_code _coco_open(coco_path_id *, char *, int);
error_code _coco_create(coco_path_id *, char *, int, coco_file_stat *);
error_code _coco_open_parent_directory(coco_path_id *path, char *pathlist, int mode, char *filename);
error_code _coco_read(coco_path_id, void *, u_int *);
error_code _coco_readdir(coco_path_id, coco_dir_entry *);
error_code _coco_ncpy_name( coco_dir_entry *e, u_char *name, size_t len );
error_code _coco_seek(coco_path_id, int, int);
error_code _coco_readln(coco_path_id, void *, u_int *);
error_code _coco_write(coco_path_id, void *, u_int *);
error_code _coco_writeln(coco_path_id, char *, u_int *);
error_code _coco_makdir(char *pathlist);
error_code _coco_delete(char *pathlist);
error_code _coco_delete_directory(char *pathlist);
error_code _coco_rename(char *pathlist, char *new_name);
error_code _coco_rename_ex(char *pathlist, char *new_name, coco_dir_entry *dentry);
error_code _coco_open_read_whole_file(coco_path_id *path, char *pathlist, int mode, u_char **buffer, u_int *size);
error_code _coco_close(coco_path_id);

/* gs.c */
error_code _coco_gs_attr(coco_path_id, int *);
error_code _coco_gs_eof(coco_path_id path);
error_code _coco_gs_fd(coco_path_id, coco_file_stat *);
error_code _coco_gs_fd_pathlist(char *pathlist, coco_file_stat *statbuf);
error_code _coco_gs_pathtype(coco_path_id, _path_type *);
error_code _coco_gs_size(coco_path_id path, u_int *size);
error_code _coco_gs_size_pathlist(char *pathlist, u_int *size);
error_code _coco_gs_pos(coco_path_id path, u_int *pos);

/* ss.c */
error_code _coco_ss_attr(coco_path_id, int);
error_code _coco_ss_fd(coco_path_id, coco_file_stat *);
error_code _coco_ss_size(coco_path_id path, int size);

error_code _coco_identify_image(char *pathlist, _path_type *type);

#ifdef __cplusplus
}
#endif

#endif	/* _COCOPATH_H */


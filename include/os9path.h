/********************************************************************
 * os9path.h - os9 path definitions header file
 *
 * $Id$
 ********************************************************************/
#ifndef	_OS9PATH_H
#define	_OS9PATH_H

#include <stdio.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cococonv.h>
#ifndef WIN32
#include <dirent.h>
#endif

/* File descriptor segment descriptor */
typedef struct
{
	u_char	lsn[3],
		num[2];
} fd_seg, *Fd_seg;


/* File descriptor sector */
typedef struct
{
	u_char	fd_att,
		fd_own[2],
		fd_dat[5],
		fd_lnk,
		fd_siz[4],
		fd_creat[3];
#define	NUM_SEGS	48
	fd_seg	fd_seg[NUM_SEGS];
} fd_stats, *Fd_stats;


/* disk file entry in directory structure */
typedef struct
{
#define	D_NAMELEN	29
	u_char	name[D_NAMELEN],
/*		res[1], */
		lsn[3];
} os9_dir_entry;

/* LSN0 */
typedef struct
{
	u_char	dd_tot[3],
		dd_tks[1],
		dd_map[2],
		dd_bit[2],
		dd_dir[3],
		dd_own[2],
		dd_att[1],
		dd_dsk[2],
		dd_fmt[1],
		dd_spt[2],
		dd_res[2],
		dd_bt[3],
		dd_bsz[2],
		dd_dat[5],
		dd_nam[32],
		/* These used to be called dd_opt[32] */
			pd_dtp[1],
			pd_drv[1],
			pd_stp[1],
			pd_typ[1],
			pd_dns[1],
			pd_cyl[2],
			pd_sid[1],
			pd_vfy[1],
			pd_sct[2],
			pd_t0s[2],
			pd_ilv[1],
			pd_sas[1],
			pd_tfm[1],
			pd_exten[2],
			pd_stoff[1],
			pd_att[1],
			pd_fd[3],
			pd_dfd[3],
			pd_dcp[4],
			pd_dvt[2],
		/* These are used by OS-9/68K */
		dd_res1[1],
		dd_sync[4],		/* CRUZ */
		dd_maplsn[4],
		dd_lsnsize[2],
		dd_versid[2];
} lsn0_sect, *Lsn0_sect;


typedef struct _os9_path_id
{
	int		mode;		/* access mode */
	char		*imgfile;	/* pointer to image file */
	char		*pathlist;	/* pointer to pathlist */
	unsigned int	pl_fd_lsn;	/* pathlist's FD LSN */
	unsigned int	filepos;	/* file position */
	FILE		*fd;		/* file path pointer */
	lsn0_sect	*lsn0;		/* copy of LSN0 */
	u_char		*bitmap;	/* bitmap */
	unsigned int	spc;		/* sectors per cluster */
	unsigned int	bps;		/* bytes per sector */
	unsigned int	spt;		/* sectors per track */
	unsigned int	t0s;		/* sectors per track on track zero */
	int		cs;		/* cluster size in bytes */
	int		bitmap_bytes;
	int		israw;		/* raw flag */
} *os9_path_id;

#define	DT_os9	1


/* prototypes */

error_code _os9_open(os9_path_id *, char *, int);
error_code _os9_create(os9_path_id *, char *, int, int);
error_code _os9_open_parent_directory( os9_path_id *path, char *pathlist, int mode, char **filename );
error_code _os9_read(os9_path_id, void *, u_int *);
error_code _os9_readdir(os9_path_id, os9_dir_entry *);
error_code _os9_ncpy_name( os9_dir_entry e, u_char *name, size_t len );
error_code _os9_seek(os9_path_id, int, int);
int _os9_lsn_fseek(os9_path_id seek_path, int lsn);
error_code _os9_allbit(u_char *bitmap, int firstbit, int numbits);
error_code _os9_delbit(u_char *bitmap, int firstbit, int numbits);
int _os9_ckbit( u_char *bitmap, int LSN );
int _os9_getfreebit( u_char *bitmap, int bitmap_bytes );
int _os9_maximum_file_size( fd_stats fd_sector, int cluster_size );
error_code _os9_getSASSegment( os9_path_id path, int *cluster, int *size );
int read_lsn(os9_path_id path, int lsn, void *buffer);
error_code _os9_readln(os9_path_id, void *, u_int *);
error_code _os9_write(os9_path_id, void *, u_int *);
error_code _os9_writeln(os9_path_id, char *, u_int *);
error_code _os9_writedir(os9_path_id, os9_dir_entry *);
error_code _os9_makdir( char *pathlist );
error_code _os9_delete( char *pathlist );
error_code _os9_delete_directory(char *pathlist);
error_code _os9_rename( char *pathlist, char *new_name );
error_code _os9_rename_ex(char *pathlist, char *new_name, os9_dir_entry *dentry);
error_code _os9_close(os9_path_id);
error_code _os9_format(char *pathname, int os968k, int tracks, int sectorsPerTrack, int sectorsTrack0, int heads, int sectorSize, int *clusterSize, char *diskName, int sectorAllocationSize, int tpi, int density, int formatEntire, int isDragon, int isHDD, int interleave, unsigned int *totalSectors, unsigned int *totalBytes);

/* gs.c */
error_code _os9_gs_attr(os9_path_id, int *);
error_code _os9_gs_eof(os9_path_id path);
error_code _os9_gs_fd_pathlist(char *pathlist, int count, fd_stats *fdbuf);
error_code _os9_gs_fd(os9_path_id, int, fd_stats *);
error_code _os9_gs_size(os9_path_id path, u_int *size);
error_code _os9_gs_size_pathlist(char *pathlist, u_int *size);
error_code _os9_gs_pos(os9_path_id path, u_int *pos);

/* ss.c */
error_code _os9_ss_attr(os9_path_id, int);
error_code _os9_ss_fd(os9_path_id, int, fd_stats *);
error_code _os9_ss_size(os9_path_id path, int size);

unsigned int NextHighestMultiple(unsigned int value, unsigned int multiple);


/* ERROR CODES */
#define EOS_IA		187
#define EOS_IC		192
#define EOS_PTHFUL	200
#define EOS_BMODE	203
#define	EOS_EOF		211
#define EOS_FNA		214
#define EOS_BPNAM	215
#define EOS_PNNF	216
#define EOS_SF		217
#define EOS_FAE		218
#define EOS_WRITE	246
#define EOS_SE		247
#define EOS_DF		248


/* file access modes */
#define	FAM_READ	0x0001
#define	FAM_WRITE	0x0002
#define	FAM_EXEC	0x0004
#define FAM_DIR		0x0080
#define FAM_NOCREATE	0x0100

/* file access permissions */
#define FAP_READ	0x01
#define FAP_WRITE	0x02
#define FAP_EXEC	0x04
#define FAP_PREAD	0x08
#define FAP_PWRITE	0x10
#define FAP_PEXEC	0x20
#define FAP_SINGLE	0x40
#define FAP_DIR		0x80

#endif	/* _OS9PATH_H */

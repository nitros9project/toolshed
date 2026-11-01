/********************************************************************
 * decbpath.h - Disk BASIC path definitions header file
 *
 * $Id$
 ********************************************************************/
#ifndef	_DECBPATH_H
#define	_DECBPATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cococonv.h>
#include <diskimage.h>

#ifndef WIN32
#include <dirent.h>
#endif

typedef struct _decb_dir_entry
{
	u_char	filename[8];
		/* Left Justified, space filled
		   If first byte is 00 then that means the directory entry is free
		   If first byte is FF then this entry and all that remain are unused
		*/
	u_char	file_extension[3];
		/* Left justified, space filled */
	u_char	file_type;
		/* 00 - BASIC
		   01 - BASIC Data
		   02 - Machine Language program
		   03 - Text Editor Source
		*/
	u_char	ascii_flag;
		/* 00 - Binary file
		   FF - ASCII file
		*/
	u_char	first_granule;
		/* Number of first granule in file */
	u_char	last_sector_size[2];
		/* Number of bytes used in last sector of last granule */
	u_char	file_status;
		/* For Color Tdecb (by Robert Kilgus)
		   Bit 0 - On allows reads
		   Bit 1 - On allows writes
		   Bit 2 - On allows file creation
		   Bit 3 - On allows file extension beyond EOF
		   Bit 4 - On means temporary file - delet file when closed
		   Bit 5 - On prevents rewrite of FAT every time a sector is added to the file
		   Bit 6 - On means I/O buffer is shared. Each logical I/O requires a physical I/O
		   Bit 7 - Reserved (Like release space when file is shortened)
		   (All bits off - file closed)
		*/
	u_char	logical_record_size[2];
		/* 0000 - Variable length with record terminated by the delimiter stored below
		   FFFF - Variable length with first two bytes of record containing size of the rest of the record
		   All other values mean fixed length of specified size
		*/
	u_char	record_terminator;
		/* Variable length record terminator */
	u_char	unused[12];
		/* Unused by Color Tdecb */
} decb_dir_entry;
	

typedef struct _decb_path_id
{
	int				mode;			/* access mode */
	char			*imgfile;		/* pointer to image file */
	char			*filename;		/* pointer to filename */
	int				drive;			/* drive number */
	int				has_drive;		/* pathlist has a drive number or offset */
	decb_dir_entry  dir_entry;
	unsigned int	this_directory_entry_index;
	unsigned int	directory_entry_index;
	u_char			FAT[256];
	u_char			saved_FAT[256];	/* FAT as read, to tell if it changed */
	error_code		fat_ec;		/* why the FAT could not be read */
	unsigned int	filepos;		/* file position */
	diskimage_id	disk;			/* the image file */
	int				israw;			/* no file I/O possible, just get/set sector and granule */
	long int		disk_offset;	/* offset for drive number */
	long int		hdbdos_offset;	/* offset for HDB-DOS */
	int				granule_count;	/* usually 68 */
} *decb_path_id;


/* File descriptor sector */
/* Disk BASIC doesn't have a file descriptor per se, but we use this structure as one. */

typedef struct
{
	u_char	file_type;		/* 0 = BASIC program, 1 = BASIC data file,
	                           2 = M/L program, 3 = Text editor source file */
	u_char	data_type;		/* 0 = Binary, 1 = ASCII */
	int		file_size;		/* file size */
} decb_file_stat, *Decb_file_stat;

/* Disk BASIC Prototypes */

error_code _decb_dskini(char *vdisk, int tracks, char *diskName, int hdbdrives, int bps, int skitzo);
error_code _decb_open(decb_path_id *, char *, int);
error_code _decb_close(decb_path_id);
error_code _decb_create(decb_path_id *path, char *pathlist, int mode, int file_type, int data_type);
error_code _decb_read(decb_path_id path, void *buffer, u_int *size);
error_code _decb_readln(decb_path_id path, void *buffer, u_int *size);
error_code _decb_write(decb_path_id path, void *buffer, u_int *size);
error_code _decb_kill(char *filename);
error_code _decb_seek(decb_path_id, int, int);
error_code _decb_readdir(decb_path_id path, decb_dir_entry *de);
error_code _decb_ncpy_name(decb_dir_entry e, u_char *name, size_t len);
error_code _decb_writedir(decb_path_id path, decb_dir_entry *de);
error_code _decb_seekdir(decb_path_id path, int entry, int mode);
error_code _decb_open_image(decb_path_id path);
error_code _decb_close_image(decb_path_id path);
void _decb_load_fat(decb_path_id path);
error_code _decb_slot_read(decb_path_id path, long long slot, void *buffer);
error_code _decb_slot_write(decb_path_id path, long long slot, const void *buffer);
error_code _decb_raw_read(decb_path_id path, void *buffer, u_int *size);
error_code _decb_raw_write(decb_path_id path, const void *buffer, u_int *size);
error_code _decb_rename(char *pathlist, char *newname);
error_code _decb_rename_ex(char *pathlist, char *new_name, decb_dir_entry *dirent);
error_code _decb_gs_size(decb_path_id path, u_int *size);
error_code _decb_gs_size_pathlist(char *pathlist, u_int *size);
error_code _decb_gs_pos(decb_path_id path, u_int *pos);
error_code _decb_ss_size(decb_path_id path, int size);
error_code _decb_gs_eof(decb_path_id path);
error_code _decb_gs_fd(decb_path_id path, decb_file_stat *stat);
error_code _decb_gs_fd_pathlist(char *pathlist, decb_file_stat *statbuf);
error_code _decb_ss_fd(decb_path_id path, decb_file_stat *stat);
error_code _decb_gs_sector(decb_path_id path, int track, int sector, char *buffer);
error_code _decb_ss_sector(decb_path_id path, int track, int sector, char *buffer);
error_code _decb_gs_granule(decb_path_id path, int granule, char *buffer);
error_code _decb_ss_granule(decb_path_id path, int granule, char *buffer);
error_code _decb_detoken(unsigned char *in_buffer, int in_size, char **out_buffer, u_int *out_size);
error_code _decb_entoken(unsigned char *in_buffer, int in_size, unsigned char **out_buffer, u_int *out_size, int path_type);
error_code _decb_buffer_sprintf(u_int *position, char **str, size_t *buffersize, const char *format, ...);
error_code _decb_detect_tokenized( unsigned char *in_buffer, u_int in_size );
error_code _decb_binconcat(unsigned char *in_buffer, int in_size, unsigned char **out_buffer, u_int *out_size);
int _decb_count_segements( u_char *buffer, u_int buffer_size );
error_code _decb_extract_first_segment( u_char *buffer, u_int buffer_size, u_char **extracted_buffer, u_int *extracted_buffer_size, u_int *load_address, u_int *exec_address );
error_code _decb_srec_encode(unsigned char *in_buffer, int in_size, char **out_buffer, u_int *out_size);
error_code _decb_srec_encode_sr(unsigned char *in_buffer, int in_size, int start_address, int exec_address, char **out_buffer, u_int *out_size);
error_code _decb_srec_decode(unsigned char *in_buffer, int in_size, u_char **out_buffer, u_int *out_size);

#include <cocopath.h>

/* ERROR CODES */
#define EOS_OM		256		/* Out of memory error */
#define EOS_SN		257		/* Syntax error */

#ifdef __cplusplus
}
#endif

#endif	/* _DECBPATH_H */


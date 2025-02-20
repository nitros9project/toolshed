/********************************************************************
 * libcebcopen.c - Cassette BASIC open/create routines
 *
 * $Id$
 ********************************************************************/

#include "cecbpath.h"

double cecb_threshold = 0.1;
double cecb_frequency = 0;
_wave_parity cecb_wave_parity = AUTO;
long cecb_start_sample = 0;
int cecb_suggest_mc10 = 0;

static error_code parse_header(cecb_path_id path);
static error_code validate_pathlist(cecb_path_id path, char *pathlist);
static int init_pd(cecb_path_id * path, int mode);
static int term_pd(cecb_path_id path);

/*
 * _cecb_create()
 *
 * Create a file
 */

error_code _cecb_create(cecb_path_id * path, char *pathlist, int mode,
			int file_type, int data_type, int gap,
			int ml_load_address, int ml_exec_address)
{
	error_code ec = EOS_BPNAM;
	char *open_mode;

	/* 1. Allocate & initialize path descriptor. */

	ec = init_pd(path, mode);

	if (ec != 0)
	{
		return ec;
	}

	/* 2. Attempt to validate the pathlist. */

	ec = validate_pathlist(*path, pathlist);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}

	(*path)->mode = mode;

	/* 3. Open a path to the image file. */

	if (mode & FAM_WRITE)
	{
		open_mode = "rb+";
	}
	else
	{
		open_mode = "rb";
	}

	(*path)->fd = fopen((*path)->imgfile, open_mode);

	if ((*path)->fd == NULL)
	{
		term_pd(*path);

		return (EOS_BPNAM);
	}


	/* 4. Open and determine CAS or WAV and fill in data structors */

	(*path)->play_at = cecb_start_sample;
	(*path)->wav_threshold = cecb_threshold;
	(*path)->wav_frequency_limit = cecb_frequency;
	(*path)->wav_parity = cecb_wave_parity;

	ec = parse_header(*path);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 5. Seek to end of data */

	/* WAV files may have more chunks after the data chunk, cache them. */

	if ((*path)->tape_type == WAV)
	{
		int count;

		fseek((*path)->fd, 0, SEEK_END);
		(*path)->extra_chunks_buffer_size =
			ftell((*path)->fd) - ((*path)->wav_data_start +
					      (*path)->wav_data_length);

		if ((*path)->extra_chunks_buffer_size > 0)
		{
			fseek((*path)->fd,
			      (*path)->wav_data_start +
			      (*path)->wav_data_length, SEEK_SET);
			(*path)->extra_chunks_buffer =
				malloc((*path)->extra_chunks_buffer_size);

			if ((*path)->extra_chunks_buffer == NULL)
				return EOS_OM;

			count = fread((*path)->extra_chunks_buffer, 1,
				      (*path)->extra_chunks_buffer_size,
				      (*path)->fd);

			if (count != (*path)->extra_chunks_buffer_size)
				return EOS_EOF;
		}

		fseek((*path)->fd,
		      ((*path)->wav_data_start + (*path)->wav_data_length),
		      SEEK_SET);
	}
	else if (((*path)->tape_type == CAS) || ((*path)->tape_type == C10))
	{
		fseek((*path)->fd, 0, SEEK_END);
		(*path)->cas_start_byte = ftell((*path)->fd);
		(*path)->cas_start_bit = 0x01;
	}
	else
	{
		fprintf(stderr, "Unknown error\n");
		return -1;
	}


	/* 6. Fill in dir_entry */

	strncpy((char *) (*path)->dir_entry.filename, (*path)->filename, 8);
	(*path)->dir_entry.file_type = file_type;
	(*path)->dir_entry.ascii_flag = data_type;
	(*path)->dir_entry.gap_flag = gap;
	(*path)->dir_entry.ml_load_address1 = ml_load_address >> 8;
	(*path)->dir_entry.ml_load_address2 = ml_load_address & 0x0f;
	(*path)->dir_entry.ml_exec_address1 = ml_exec_address >> 8;
	(*path)->dir_entry.ml_exec_address2 = ml_exec_address & 0x0f;

	/* 7. Write half second of silence */

	ec = _cecb_write_silence(*path, 0.50);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}

	/* 8. Write leader, dir_entry */

	ec = _cecb_write_leader(*path);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}

	ec = _cecb_write_block(*path, 0,
			       (unsigned char *) &((*path)->dir_entry),
			       sizeof(cecb_dir_entry));

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}

	/* 9. Write gap, leader */

	ec = _cecb_write_silence(*path, 0.58);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}

	if ((*path)->dir_entry.gap_flag == 0)
	{
		ec = _cecb_write_leader(*path);

		if (ec != 0)
		{
			term_pd(*path);

			return ec;
		}
	}

	/* Get ready for data blocks */
	(*path)->block_type = 1;

	return ec;
}

/*
 * _cecb_open()
 *
 * Open a path to a file
 *
 * Legal pathnames are:
 *
 * 1. imagename,      (considered to be a 'raw' open of the image)
 * 2. imagename,file  (considered to be a file open within the image)
 * 3. imagename       (considered to be an error)
*/

error_code _cecb_open(cecb_path_id * path, char *pathlist, int mode)
{
	error_code ec = 0;
	char *open_mode;

	/* 0. Currently -- no writing supported */

	if ((mode & FAM_WRITE) == FAM_WRITE)
		return EOS_IC;

	/* 1. Strip off FAM_NOCREATE if passed -- irrelavent to _cecb_open */

	mode = mode & ~FAM_NOCREATE;

	/* 2. Allocate & initialize path descriptor */

	ec = init_pd(path, mode);

	if (ec != 0)
	{
		return ec;
	}

	/* 3. Attempt to validate the pathlist */

	ec = validate_pathlist(*path, pathlist);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}


	/* 4. Determine if Cassette is being open in raw mode. (We know it is raw mode if there is no filename). */

	if (strncmp((*path)->filename, "        ", 8) == 0)
	{
		/* 1. Yes, raw mode */

		(*path)->israw = 1;
	}
	else
	{
		(*path)->israw = 0;

		/* If mode is FAM_DIR, then we need to error */
		if (mode & FAM_DIR)
		{
			term_pd(*path);
			return EOS_SN;
		}
	}


	/* 5. Open a path to the image file. */

	if (mode & FAM_WRITE)
	{
		open_mode = "rb+";
	}
	else
	{
		open_mode = "rb";
	}

	(*path)->fd = fopen((*path)->imgfile, open_mode);

	if ((*path)->fd == NULL)
	{
		term_pd(*path);

		return (EOS_BPNAM);
	}

	/* 6. Open and determine CAS or WAV and fill in data structors */

	(*path)->play_at = cecb_start_sample;
	(*path)->wav_threshold = cecb_threshold;
	(*path)->wav_frequency_limit = cecb_frequency;
	(*path)->wav_parity = cecb_wave_parity;

	ec = parse_header(*path);

	if (ec != 0)
	{
		term_pd(*path);

		return ec;
	}

	/* if raw, exit */

	if ((*path)->israw == 1)
		return ec;

	/* Find file */

	while (ec == 0)
	{
		ec = _cecb_read_next_dir_entry(*path, &((*path)->dir_entry));

		if (ec == 0)
		{
			if (strncmp
			    ((char *) (*path)->dir_entry.filename,
			     (*path)->filename, 8) == 0)
			{
				/* File found, save the place of the start of data */

				if (((*path)->tape_type == CAS) || ((*path)->tape_type == C10))
				{
					(*path)->cas_start_byte =
						(*path)->cas_current_byte;
					(*path)->cas_start_bit =
						(*path)->cas_current_bit;
				}
				else if ((*path)->tape_type == WAV)
					(*path)->wav_start_sample =
						(*path)->wav_current_sample;

				break;
			}
		}
	}

	return ec;
}

/*
 * _cecb_close()
 *
 * Close a path to a file
 */

error_code _cecb_close(cecb_path_id path)
{
	error_code ec = 0;

	/* if data was written, write last data block and end block */
	if ((path->mode & FAM_WRITE) == FAM_WRITE)
	{
		if (path->length > 0)
		{
			ec = _cecb_write_block(path, path->block_type,
					       path->data, path->length);
			path->length = 0;
			path->current_pointer = 0;
		}

		path->block_type = 0xff;
		ec = _cecb_write_block(path, path->block_type, path->data,
				       path->length);

		/* Write half second of silence */

		ec = _cecb_write_silence(path, 0.58);

		if (ec != 0)
		{
			term_pd(path);

			return ec;
		}

		if (path->tape_type == WAV)
		{
			/* Update RIFF chunk lengths */
			fseek(path->fd, 4, SEEK_SET);
			fwrite_le_int(path->wav_riff_size, path->fd);
			fseek(path->fd, path->wav_data_start - 4, SEEK_SET);
			fwrite_le_int(path->wav_data_length, path->fd);

			if (path->extra_chunks_buffer_size > 0)
			{
				/* Write end of WAV file chunks */
				fseek(path->fd, path->wav_data_length,
				      SEEK_CUR);
				fwrite(path->extra_chunks_buffer, 1,
				       path->wav_data_length, path->fd);
			}
		}
	}


	/* Close path. */

	fclose(path->fd);


	/* Terminate path descriptor */

	ec = term_pd(path);


	/* Return status. */

	return (ec);

}

/*
 * parse_header()
 *
 * Determines if file is CAS or WAV.
 *
 * If necessary reads WAV headers to fill in data structurs
 */

static error_code parse_header(cecb_path_id path)
{
	error_code ec = 0;

	if (strendcasecmp(path->imgfile, CAS_FILE_EXTENSION) == 0)
		ec = _cecb_parse_cas(path);
	else if (strendcasecmp(path->imgfile, C10_FILE_EXTENSION) == 0)
	{
		ec = _cecb_parse_cas(path);
		path->tape_type = C10;
	}
	else
		ec = _cecb_parse_riff(path);

	return ec;
}


/*
 * validate_pathlist()
 *
 * Determines if the passed <image,path:drive> pathlist is valid.
 *
 * Copies the image file and pathlist file portions into
 * the path descriptor.
 *
 * Valid pathlist examples:
 *	foo,			Opens foo for raw access
 *	foo,bar			Opens file bar in Cassette image foo
 */

static error_code validate_pathlist(cecb_path_id path, char *pathlist)
{
	error_code ec = 0;
	char *p;
	int i, j;
	
	/* 1. Validate the pathlist. */

	if ((p = strchr(pathlist, ',')) == NULL)
	{
		/* 1. No native/Cassette delimiter in pathlist, return error. */

		ec = EOS_BPNAM;
	}
	else
	{
		/* 1. Extract information out of pathlist. */

		/* path->imgfile = strndup(pathlist, p - pathlist); */
		path->imgfile = malloc(p - pathlist + 1);
		memcpy(path->imgfile, pathlist, p - pathlist);
		path->imgfile[p - pathlist] = '\0';

		p++;

		/* Copy characters and space fill buffer */
		for (i = 0, j = 0; j < 8; j++)
		{
			if (p[i] != 0)
			{
				path->filename[j] = p[i++];
			}
			else
			{
				path->filename[j] = ' ';
			}
		}
	}

	/* 2. Return. */

	return ec;
}

static int init_pd(cecb_path_id * path, int mode)
{
	/* 1. Allocate path structure and initialize it. */

	*path = malloc(sizeof(struct _cecb_path_id));

	if (*path == NULL)
	{
		return 1;
	}


	/* 2. Clear out newly allocated path structure. */

	memset(*path, 0, sizeof(struct _cecb_path_id));

	(*path)->mode = mode;

	/* 3. Return. */

	return 0;
}

static int term_pd(cecb_path_id path)
{
	/* 0. Deallocate internal buffers */

	if (path->extra_chunks_buffer_size > 0)
		free(path->extra_chunks_buffer);

	if (path->buffer_1200 != NULL)
		free(path->buffer_1200);
	if (path->buffer_2400 != NULL)
		free(path->buffer_2400);

	/* 1. Deallocate path structure. */
	if (path->imgfile)
		free(path->imgfile);
	free(path);

	/* 2. Return. */

	return 0;
}

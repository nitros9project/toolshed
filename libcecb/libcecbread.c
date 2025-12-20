/********************************************************************
 * libcebcread.c - Cassette BASIC read routines
 *
 * $Id$
 ********************************************************************/

#include "cecbpath.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

error_code _cecb_read(cecb_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	u_int requested_bytes;
	char *b = (char *) buffer;

	/* 1. Check the mode. */

	if ((path->mode & FAM_READ) == 0)
	{
		return EOS_FNA;
	}


	/* 2. Treat raw path differently. */

	if (path->israw == 1)
	{
		return EOS_FNA;
	}

	requested_bytes = *size;
	*size = 0;

	if ((path->eof_flag == 1) && (path->current_pointer == path->length))
		return EOS_EOF;

	while (requested_bytes > 0)
	{
		if ((path->eof_flag == 1)
		    && (path->current_pointer == path->length))
			break;

		/* Fufill request from buffer */
		if (path->current_pointer < path->length)
		{
			size_t copy_bytes;

			copy_bytes =
				MIN((path->length - path->current_pointer),
				    requested_bytes);

			memcpy(b + (*size),
			       &(path->data[path->current_pointer]),
			       copy_bytes);

			path->current_pointer += copy_bytes;
			*size += copy_bytes;
			requested_bytes -= copy_bytes;
			path->filepos += copy_bytes;
		}

		/* if buffer empty, get new block */
		if (path->current_pointer == path->length)
		{
			ec = _cecb_read_next_block(path, &(path->block_type),
						   &(path->length),
						   path->data);
			path->current_pointer = 0;

			if (ec != 0)
				return ec;

			if (path->block_type == 0xff)
				path->eof_flag = 1;	/* End of file */
		}
	}

	return ec;
}

error_code _cecb_readln(cecb_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	char *current;
	u_int read_size;
	u_int requested_bytes;

	current = buffer;
	requested_bytes = *size;
	*size = 0;

	while (requested_bytes > 0)
	{
		read_size = 1;
		ec = _cecb_read(path, current, &read_size);

		if (ec != 0)
			return ec;

		if (read_size != 1)
			break;

		current++;
		*size += 1;
		requested_bytes -= 1;

		if (*current == 0x0d)
			break;
	}

	return ec;
}

error_code _cecb_read_next_dir_entry(cecb_path_id path,
				     cecb_dir_entry * dir_entry)
{
	error_code ec = 0;
	unsigned char data[256];
	unsigned char block_type, block_length;

	while (ec == 0)
	{
		ec = _cecb_read_next_block(path, &block_type, &block_length,
					   data);

		if ((ec == EOS_CRC) || (ec == 0))
		{
			if ((block_type == 0)
			    && (block_length == sizeof(cecb_dir_entry)))
			{
				memcpy(dir_entry, data,
				       sizeof(cecb_dir_entry));
				break;
			}
		}
	}

	return ec;
}

error_code _cecb_ncpy_name(cecb_dir_entry e, u_char * name, size_t len)
{
	error_code ec = 0;
	u_char c_name[9];

	DECBStringToCString(e.filename, (u_char *) " ", c_name);
	c_name[8] = 0;

	strncpy((char *) name, (const char *) c_name, len);

	return ec;

}

error_code _cecb_read_next_block(cecb_path_id path, unsigned char *block_type,
				 unsigned char *block_length,
				 unsigned char *data)
{
	error_code ec = 0;
	unsigned char find_block;
	unsigned char checksum, checksum_ck;
	int i;

	find_block = 0;

	while (find_block != 0x3c)
	{
		unsigned char newbit;

		find_block >>= 1;

		ec = _cecb_read_bits(path, 1, &newbit);

		if (ec != 0)
			return ec;

		find_block |= (unsigned short) newbit;

// 		printf( "find_block: %2x, sample: %ld\n", find_block, path->wav_current_sample );
	}

	ec = _cecb_read_bits(path, 8, block_type);
	ec = _cecb_read_bits(path, 8, block_length);

	checksum = *block_type + *block_length;

// 	printf( "\nblock type: %d, Length: %d\n", *block_type, *block_length );

	for (i = 0; i < *block_length; i++)
	{
		ec = _cecb_read_bits(path, 8, &(data[i]));
		checksum += data[i];
		//printf( "c: %2.2x, i: %d\n", data[i], i );
	}

	ec = _cecb_read_bits(path, 8, &checksum_ck);

	if (checksum != checksum_ck)
		ec = EOS_CRC;

	return ec;
}

error_code _cecb_read_bits(cecb_path_id path, int count,
			   unsigned char *result)
{
	error_code ec;

	if ((path->tape_type == CAS) || (path->tape_type == C10))
		ec = _cecb_read_bits_cas(path, count, result);
	else if (path->tape_type == WAV)
		ec = _cecb_read_bits_wav(path, count, result);
	else
		ec = EOS_IA;

	return ec;
}

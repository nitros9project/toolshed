/********************************************************************
 * libcebcwrite.c - Cassette BASIC read routines
 *
 * $Id$
 ********************************************************************/

#include "cecbpath.h"

static error_code write_byte(cecb_path_id path, unsigned char byte);
static error_code write_buffer(cecb_path_id path, unsigned char *buffer,
			       int length);
static unsigned char checksum_buffer(unsigned char checksum,
				     unsigned char *buffer, int length);

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

error_code _cecb_write(cecb_path_id path, void *buffer, unsigned int *size)
{
	error_code ec = 0;
	int fill_bytes, buffer_pointer = 0;
	char *b = (char *) buffer;

	while (*size > 0)
	{
		if (path->length == 0xff)
		{
			ec = _cecb_write_block(path, path->block_type,
					       path->data, path->length);
			path->length = 0;
			path->current_pointer = 0;

			if (ec != 0)
				break;
		}

		fill_bytes = MIN(0xff - path->length, *size);

		memcpy(&(path->data[path->current_pointer]),
		       b + buffer_pointer, fill_bytes);
		*size -= fill_bytes;
		path->current_pointer += fill_bytes;
		path->length += fill_bytes;
		buffer_pointer += fill_bytes;
	}

	*size = buffer_pointer;

	return 0;
}

error_code _cecb_write_block(cecb_path_id path, unsigned char block_type,
			     unsigned char *data, int length)
{
	error_code ec = 0;
	unsigned char checksum;

	if (length > 0xff)
	{
		fprintf(stderr,"Can not write block longer than 255 bytes in length\n");
		return -1;
	}

	/* Add gap, if requested */
	if ((path->dir_entry.gap_flag == 0xff) && (path->block_type != 0x00))
	{
		ec |= _cecb_write_silence(path, 0.5);
		ec |= _cecb_write_leader(path);
	}

	ec |= write_byte(path, 0x55);
	ec |= write_byte(path, 0x3c);
	ec |= write_byte(path, block_type);
	checksum = block_type;
	ec |= write_byte(path, length);
	checksum += length;
	ec |= write_buffer(path, data, length);
	checksum = checksum_buffer(checksum, data, length);
	ec |= write_byte(path, checksum);
	ec |= write_byte(path, 0x55);

	return ec;
}

error_code _cecb_write_leader(cecb_path_id path)
{
	error_code ec = 0;
	int i;

	ec |= _cecb_write_silence(path, 0.125);

	for (i = 0; i < 128; i++)
		ec |= write_byte(path, 0x55);

	return ec;
}

error_code _cecb_write_silence(cecb_path_id path, double length)
{
	if ((path->tape_type == CAS) || (path->tape_type == C10))
	{
		char *buffer;
		int size;

		size = length * 20;

		buffer = calloc(size, 1);

		if (buffer != NULL)
		{
			_cecb_write_cas_data(path, buffer, size);
			free(buffer);
		}
	}
	else if (path->tape_type == WAV)
	{
		int bytes_written, sample_count;

		sample_count = path->wav_sample_rate * length;

		if (path->wav_bits_per_sample == 8)
			bytes_written = _cecb_write_wav_repeat_byte(path, sample_count, 127);
		else
			bytes_written = _cecb_write_wav_repeat_byte(path, sample_count * (path->wav_bits_per_sample/8), 0);

		path->wav_data_length += bytes_written;
		path->wav_riff_size += bytes_written;
	}
	else
	{
		fprintf(stderr,"Unknown tape type\n");
		return -1;
	}

	return 0;
}

static error_code write_byte(cecb_path_id path, unsigned char byte)
{
	int bytes_written;

	if ((path->tape_type == CAS) || (path->tape_type == C10))
	{
		_cecb_write_cas_data(path, (char *) &byte, 1);
	}
	else if (path->tape_type == WAV)
	{
		bytes_written =
			_cecb_write_wav_audio(path, (char *) &byte, 1);
		path->wav_data_length += bytes_written;
		path->wav_riff_size += bytes_written;
	}
	else
	{
		fprintf(stderr, "Unknown tape type\n");
		return -1;
	}

	return 0;
}

static error_code write_buffer(cecb_path_id path, unsigned char *buffer,
			       int length)
{
	int bytes_written;

	if ((path->tape_type == CAS) || (path->tape_type == C10))
	{
		_cecb_write_cas_data(path, (char *) buffer, length);
	}
	else if (path->tape_type == WAV)
	{
		bytes_written =
			_cecb_write_wav_audio(path, (char *) buffer, length);
		path->wav_data_length += bytes_written;
		path->wav_riff_size += bytes_written;
	}
	else
	{
		fprintf(stderr, "Unknown tape type\n");
		return -1;
	}
	
	return 0;
}


static unsigned char checksum_buffer(unsigned char checksum,
				     unsigned char *buffer, int length)
{
	int i;

	for (i = 0; i < length; i++)
		checksum += buffer[i];

	return checksum;
}

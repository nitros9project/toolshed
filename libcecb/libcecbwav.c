/********************************************************************
 * libcebcwav.c - Cassette BASIC WAVE file routines
 *
 * $Id$
 ********************************************************************/

#include "math.h"
#include <limits.h>
#include "cecbpath.h"

#define PI 3.1415926

static void build_sinusoidal_bufer_8(unsigned char *buffer, int length, int mc10);
static void build_sinusoidal_bufer_16(short *buffer, int length, int mc10);
static void build_sinusoidal_buffer_24(uint8_t *buffer, int length, int mc10);
static void build_sinusoidal_buffer_32(uint8_t *buffer, int length, int mc10);

/*
 * _cecb_parse_riff()
 *
 * Parse RIFF for WAV headers.
 *
 */

error_code _cecb_parse_riff(cecb_path_id path)
{
	error_code ec = 0;
	u_char data[255];
	int found;
	unsigned int chunk_length;

	if ((fread(data, 1, 4, path->fd) != 4)
	    || (strncmp((char *) data, "RIFF", 4) != 0))
		return EOS_WT;

	if (fread_le_int(&(path->wav_riff_size), path->fd) != 4)
		return EOS_WT;

	if ((fread(data, 1, 4, path->fd) != 4)
	    || (strncmp((char *) data, "WAVE", 4) != 0))
		return EOS_WT;

	found = 0;

	while (found == 0)
	{
		if (fread(data, 1, 4, path->fd) != 4)
			return EOS_WT;

		if (fread_le_int(&chunk_length, path->fd) != 4)
			return EOS_WT;

		if (strncmp((char *) data, "fmt ", 4) == 0)
		{
			unsigned short fmt_channel_count, fmt_block_align;
			unsigned int fmt_bytes_per_second;

			if (chunk_length != 16)
				return EOS_WT;

			if (fread_le_short(&(path->wav_audioFormat), path->fd) != 2)
				return EOS_WT;
			if (fread_le_short(&fmt_channel_count, path->fd) != 2)
				return EOS_WT;
			if (fread_le_int(&(path->wav_sample_rate), path->fd)
			    != 4)
				return EOS_WT;
			if (fread_le_int(&fmt_bytes_per_second, path->fd) !=
			    4)
				return EOS_WT;
			if (fread_le_short(&fmt_block_align, path->fd) != 2)
				return EOS_WT;
			if (fread_le_short
			    (&(path->wav_bits_per_sample), path->fd) != 2)
				return EOS_WT;

			if (fmt_channel_count != 1)
				return EOS_WT;

			if (path->wav_bits_per_sample == 8 && path->wav_audioFormat == 1)
			{
				path->wav_zero_high.uc_value = 127 + (cecb_zero_adjust * 128.0);
				path->wav_zero_low.uc_value = 128 - (cecb_zero_adjust * 128.0);
			}
			else if (path->wav_bits_per_sample == 16 && path->wav_audioFormat == 1)
			{	
				path->wav_zero_high.s_value = (cecb_zero_adjust * 32767.0);
				path->wav_zero_low.s_value = -(cecb_zero_adjust * 32767.0);
			}
			else if (path->wav_bits_per_sample == 24 && path->wav_audioFormat == 1)
			{
				path->wav_zero_high.i_value = (cecb_zero_adjust * 8388607.0);
				path->wav_zero_low.i_value = -(cecb_zero_adjust * 8388607.0);
			}
			else if (path->wav_bits_per_sample == 32 && path->wav_audioFormat == 1)
			{
				path->wav_zero_high.i_value = (cecb_zero_adjust * INT32_MAX);
				path->wav_zero_low.i_value = -(cecb_zero_adjust * INT32_MAX);
			}
			else if (path->wav_bits_per_sample == 32 && path->wav_audioFormat == 3)
			{
				path->wav_zero_high.d_value = cecb_zero_adjust;
				path->wav_zero_low.d_value = -cecb_zero_adjust;
			}
			else if (path->wav_bits_per_sample == 64 && path->wav_audioFormat == 3)
			{
				path->wav_zero_high.d_value = cecb_zero_adjust;
				path->wav_zero_low.d_value = -cecb_zero_adjust;
			}
			else
			{
				fprintf(stderr, "Unknown WAV format: %d, %d\n", path->wav_bits_per_sample, path->wav_audioFormat);
				return EOS_WT;
			}

			found = 1;
		}
		else
		{
			char *buffer;

			buffer = malloc(chunk_length);

			if (buffer == NULL)
				return EOS_MF;

			if (fread(buffer, 1, chunk_length, path->fd) !=
			    chunk_length)
				return EOS_WT;

			free(buffer);
		}
	}

	if (found == 0)
		return EOS_WT;

	found = 0;

	while (found == 0)
	{
		if (fread(data, 1, 4, path->fd) != 4)
			return EOS_WT;

		if (fread_le_int(&chunk_length, path->fd) != 4)
			return EOS_WT;

		if (strncmp((char *) data, "data", 4) == 0)
		{
			path->wav_data_length = chunk_length;
			path->wav_data_start = ftell(path->fd);
			path->tape_type = WAV;

			found = 1;
		}
	}

	if (found == 0)
		return EOS_WT;

	path->wav_total_samples = path->wav_data_length / (path->wav_bits_per_sample/8);
	position_tape(path, path->play_at);

	/* Scan rest tape for a leader, this will 
	 set high and low frequencies and threshold */
	 
	advance_to_next_leader(path, 0.0, 0.15);

	position_tape(path, path->play_at);
	
	if ((path->mode & FAM_WRITE) == FAM_WRITE)
		ec = create_sinusoidal_write_buffers(path);
	
	return ec;
}

/*
 * _cecb_read_bits_wav()
 *
 * 
 *
 */

error_code _cecb_read_bits_wav(cecb_path_id path, int count,
			       unsigned char *result)
{
	if (path->wav_current_sample >= path->wav_total_samples)
	{
		return EOS_EOF;
	}
	
	if (count > 8)
	{
		return EOS_MF;
	}
	
	/* pre leader mode */
	if (path->wav_mode == 0)
	{
		advance_to_next_leader(path, 0, 0.5);
		path->wav_mode = 1;
	}
	/* post leader mode */
	else if (path->wav_mode == 1)
	{
		while (count--)
		{
			path->wav_crossings[4] = path->wav_crossings[2];
			path->wav_crossings[3] = path->wav_crossings[1];
			path->wav_crossings[2] = path->wav_crossings[0];
			path->wav_crossings[1] = next_zero_crossing(path);
			path->wav_crossings[0] = next_zero_crossing(path);
			
			int wave = path->wav_crossings[0] - path->wav_crossings[2];
			
			if (wave > (0.006 * path->wav_sample_rate))
			{
				path->wav_mode = 0;
				return 0;
			}
			
			if (wave > path->wav_average_sample_count)
			{	
				(*result) >>= 1;
			}
			else
			{
				(*result) >>= 1;
				(*result) |= 0x80;
			}
		}
	}
	/* error mode */
	else
	{
		fprintf( stderr, "Should never get here\n");
		exit(-1);
	}
	return 0;
}

/*
 * next_zero_crossing()
 *
 *
 */

long next_zero_crossing(cecb_path_id path)
{
	if (path->wav_bits_per_sample == 8 && path->wav_audioFormat == 1)
	{
		while (path->wav_current_sample < path->wav_total_samples)
		{
			path->wav_ss1.uc_value = path->wav_ss2.uc_value;
			path->wav_ss2.uc_value = fgetc(path->fd);
			(path->wav_current_sample)++;

			// Crossing from low to high
			if(path->wav_ss1.uc_value < path->wav_zero_low.uc_value && 
			   path->wav_ss2.uc_value >= path->wav_zero_low.uc_value)
			{
				return path->wav_current_sample;
			}

			// Crossing from high to low
			if(path->wav_ss1.uc_value > path->wav_zero_high.uc_value && 
			   path->wav_ss2.uc_value <= path->wav_zero_high.uc_value)
			{
				return path->wav_current_sample;
			}
		}
	}
	else if (path->wav_bits_per_sample == 16 && path->wav_audioFormat == 1)
	{
		while (path->wav_current_sample < path->wav_total_samples)
		{
			path->wav_ss1.s_value = path->wav_ss2.s_value;
			short s;
			if (fread_le_sshort(&s, path->fd) != 2)
				return LONG_MAX;
			path->wav_ss2.s_value = s;
			(path->wav_current_sample)++;
			
			// Crossing from low to high
			if(path->wav_ss1.s_value < path->wav_zero_low.s_value && 
			   path->wav_ss2.s_value >= path->wav_zero_low.s_value)
			{
				return path->wav_current_sample;
			}

			// Crossing from high to low
			if(path->wav_ss1.s_value > path->wav_zero_high.s_value && 
			   path->wav_ss2.s_value <= path->wav_zero_high.s_value)
			{
				return path->wav_current_sample;
			}
		}
	}
	else if (path->wav_bits_per_sample == 24 && path->wav_audioFormat == 1)
	{
		while (path->wav_current_sample < path->wav_total_samples)
		{
			path->wav_ss1.i_value = path->wav_ss2.i_value;
			int i;
			if (fread_le_24bit(&i, path->fd) != 3)
				return LONG_MAX;
			path->wav_ss2.i_value = i;
			(path->wav_current_sample)++;

			// Crossing from low to high
			if(path->wav_ss1.i_value < path->wav_zero_low.i_value && 
			   path->wav_ss2.i_value >= path->wav_zero_low.i_value)
			{
				return path->wav_current_sample;
			}

			// Crossing from high to low
			if(path->wav_ss1.i_value > path->wav_zero_high.i_value && 
			   path->wav_ss2.i_value <= path->wav_zero_high.i_value)
			{
				return path->wav_current_sample;
			}
		}
	}
	else if (path->wav_bits_per_sample == 32 && path->wav_audioFormat == 1)
	{
		while (path->wav_current_sample < path->wav_total_samples)
		{
			path->wav_ss1.i_value = path->wav_ss2.i_value;
			int i;
			if (fread_le_sint(&i, path->fd) != 4)
				return LONG_MAX;
			path->wav_ss2.i_value = i;
			(path->wav_current_sample)++;

			// Crossing from low to high
			if(path->wav_ss1.i_value < path->wav_zero_low.i_value && 
			   path->wav_ss2.i_value >= path->wav_zero_low.i_value)
			{
				return path->wav_current_sample;
			}

			// Crossing from high to low
			if(path->wav_ss1.i_value > path->wav_zero_high.i_value && 
			   path->wav_ss2.i_value <= path->wav_zero_high.i_value)
			{
				return path->wav_current_sample;
			}
		}
	}
	else
	{
		fprintf( stderr, "Should never get here: %d, %d\n", path->wav_bits_per_sample, path->wav_audioFormat);
		exit(-1);
	}
	
	return LONG_MAX;
}

/*
 * advance_to_next_leader()
 *
 *
 */

void advance_to_next_leader(cecb_path_id path, double timeout, double min_length)
{
	long position_limit;
	double ratio;
	long wave1, wave2;
	long count, high_sum, low_sum;
	long sample_min_length = min_length * path->wav_sample_rate;
	long start_block;
	
	if(timeout == 0.0 )
	{
		position_limit = path->wav_total_samples;
	}
	else
	{
		position_limit = path->wav_current_sample + (timeout * path->wav_sample_rate);
	}
	
	start_block = path->wav_current_sample;
	path->wav_crossings[4] = next_zero_crossing(path);
	path->wav_crossings[3] = next_zero_crossing(path);
	path->wav_crossings[2] = next_zero_crossing(path);
	path->wav_crossings[1] = next_zero_crossing(path);
	path->wav_crossings[0] = next_zero_crossing(path);
	
	low_sum = 0;
	high_sum = 0;
	count = 0;
	
	while(path->wav_current_sample < position_limit)
	{
		wave1 = path->wav_crossings[2] - path->wav_crossings[4];
		wave2 = path->wav_crossings[0] - path->wav_crossings[2];
	
		ratio = (double)wave1 / wave2;
		
		if (ratio>path->wav_ratio_low && ratio<path->wav_ratio_high)
		{
			count++;
			low_sum += wave1;
			high_sum += wave2;
			
			if((path->wav_current_sample - start_block) > sample_min_length )
			{
				path->wav_low_sample_count = low_sum / count;
				path->wav_high_sample_count = high_sum / count;
				path->wav_average_sample_count = (path->wav_low_sample_count + path->wav_high_sample_count) / 2.0;
				return;
			}

			path->wav_crossings[4] = path->wav_crossings[0];
			path->wav_crossings[3] = next_zero_crossing(path);
			path->wav_crossings[2] = next_zero_crossing(path);
			path->wav_crossings[1] = next_zero_crossing(path);
			path->wav_crossings[0] = next_zero_crossing(path);
		}
		else
		{
			low_sum = 0;
			high_sum = 0;
			count = 0;

			path->wav_crossings[4] = path->wav_crossings[3];
			path->wav_crossings[3] = path->wav_crossings[2];
			path->wav_crossings[2] = path->wav_crossings[1];
			path->wav_crossings[1] = path->wav_crossings[0];
			path->wav_crossings[0] = next_zero_crossing(path);

			start_block = path->wav_crossings[4];
		}
	}

	/* Measured 2025/12/13 */
	path->wav_low_sample_count = (double)path->wav_sample_rate / 2051.1627907;
	path->wav_high_sample_count = (double)path->wav_sample_rate / 1102.5;
	path->wav_average_sample_count = (path->wav_low_sample_count + path->wav_high_sample_count) / 2.0;
	return;	
}

/*
 * position_tape()
 *
 *
 */

error_code position_tape(cecb_path_id path, long sample_position)
{
	if(sample_position < path->wav_total_samples)
	{
		fseek(path->fd, path->wav_data_start + sample_position * (path->wav_bits_per_sample/8), SEEK_SET);
		path->wav_current_sample = sample_position;
		return 0;
	}
	
	return EOS_EOF;
}

/*
 * _cecb_write_wav_audio()
 *
 *
 */

int _cecb_write_wav_audio(cecb_path_id path, char *buffer, int total_length)
{
	int result = 0, i;

	for (i = 0; i < total_length; i++)
	{
		int j;

		for (j = 0; j < 8; j++)
		{
			if (((buffer[i] >> j) & 0x01) == 0)
			{
				fwrite(path->buffer_2400,
				       path->buffer_2400_length, 1, path->fd);
				result += path->buffer_2400_length;
			}
			else
			{
				fwrite(path->buffer_1200,
				       path->buffer_1200_length, 1, path->fd);
				result += path->buffer_1200_length;
			}
		}
	}

	return result;
}

/*
 * _cecb_write_wav_audio_repeat_byte()
 *
 *
 */

int _cecb_write_wav_audio_repeat_byte(cecb_path_id path, int length,
				      char byte)
{
	int i, result = 0;

	for (i = 0; i < length; i++)
		result += _cecb_write_wav_audio(path, &byte, 1);

	return result;
}

/*
 * _cecb_write_wav_repeat_byte()
 *
 *
 */

int _cecb_write_wav_repeat_byte(cecb_path_id path, int length, char byte)
{
	int i;

	for (i = 0; i < length; i++)
		fwrite_le_char(byte, path->fd);

	return length;
}

/*
 * _cecb_write_wav_repeat_short()
 *
 *
 */

int _cecb_write_wav_repeat_short(cecb_path_id path, int length, short bytes)
{
	int i;

	for (i = 0; i < length; i++)
		fwrite_le_short(bytes, path->fd);

	return length * 2;
}

/*
 * create_sinusoidal_write_buffers()
 *
 *
 */

error_code create_sinusoidal_write_buffers(cecb_path_id path)
{
	path->buffer_1200_length =
		path->wav_low_sample_count * (path->wav_bits_per_sample/8);
	path->buffer_2400_length =
		path->wav_high_sample_count * (path->wav_bits_per_sample/8);

	path->buffer_1200 = malloc(path->buffer_1200_length);
	path->buffer_2400 = malloc(path->buffer_2400_length);

	if ((path->buffer_1200 == NULL) || (path->buffer_2400 == NULL))
		return EOS_MF;

	if (path->wav_bits_per_sample == 8 && path->wav_audioFormat == 1)
	{
		build_sinusoidal_bufer_8(path->buffer_1200,
					 path->buffer_1200_length, cecb_suggest_mc10);
		build_sinusoidal_bufer_8(path->buffer_2400,
					 path->buffer_2400_length, cecb_suggest_mc10);
	}
	else if (path->wav_bits_per_sample == 16 && path->wav_audioFormat == 1)
	{
		build_sinusoidal_bufer_16((short *) path->buffer_1200,
					  path->buffer_1200_length / 2, cecb_suggest_mc10);
		build_sinusoidal_bufer_16((short *) path->buffer_2400,
					  path->buffer_2400_length / 2, cecb_suggest_mc10);
	}
	else if (path->wav_bits_per_sample == 24 && path->wav_audioFormat == 1)
	{
		build_sinusoidal_buffer_24(path->buffer_1200,
					  path->buffer_1200_length / 3, cecb_suggest_mc10);
		build_sinusoidal_buffer_24(path->buffer_2400,
					  path->buffer_2400_length / 3, cecb_suggest_mc10);
	}
	else if (path->wav_bits_per_sample == 32 && path->wav_audioFormat == 1)
	{
		build_sinusoidal_buffer_32(path->buffer_1200,
					  path->buffer_1200_length / 4, cecb_suggest_mc10);
		build_sinusoidal_buffer_32(path->buffer_2400,
					  path->buffer_2400_length / 4, cecb_suggest_mc10);
	}
	else
	{
		fprintf(stderr, "WAV writing in this format unsupported. %d, %d\n", path->wav_bits_per_sample, path->wav_audioFormat);
		exit(-1);
	}

	return 0;
}

/*
 * build_sinusoidal_bufer_8()
 *
 *
 */

static void build_sinusoidal_bufer_8(unsigned char *buffer, int length, int mc10)
{
	double offset, increment = (PI * 2.0) / length; 
	int i;

	offset = PI * 2.0;

	for (i = 0; i < length; i++)
	{
		if (mc10)
			buffer[i] = sin(increment * i + offset) > 0 ? 255 : 0;
		else
			buffer[i] = (sin(increment * i + offset) * 110.0) + 127.0;
	}
}

/*
 * build_sinusoidal_bufer_16()
 *
 *
 */

static void build_sinusoidal_bufer_16(short *buffer, int length, int mc10)
{
	double offset, increment = (PI * 2.0) / length;
	int i;

	offset = PI * 2.0;

	for (i = 0; i < length; i++)
	{
		if (mc10)
			buffer[i] = sin(increment * i + offset) > 0 ? 32767 : -32768;
		else
			buffer[i] = sin(increment * i + offset) * 25500.0;
#if defined(__BIG_ENDIAN__)
		buffer[i] = swap_short(buffer[i]);
#endif
	}
}

/*
 * build_sinusoidal_buffer_24()
 *
 *
 */

static void build_sinusoidal_buffer_24(uint8_t *buffer, int length, int mc10)
{
    double offset, increment = (PI * 2.0) / length;
    int i;

    offset = PI * 2.0;

    for (i = 0; i < length; i++)
    {
        int32_t sample;

        if (mc10)
        {
            /* Full-scale square wave */
            sample = (sin(increment * i + offset) > 0) ? 0x7FFFFF : -0x800000;
        }
        else
        {
            /* Match 16-bit scaling proportionally */
            sample = (int32_t)(sin(increment * i + offset) * 0x63C000);
            /* 0x63C000 ≈ 25500 << 8 */
        }

        /* Clamp just in case */
        if (sample >  0x7FFFFF) sample =  0x7FFFFF;
        if (sample < -0x800000) sample = -0x800000;

        /* WAV is little-endian, 24-bit */
        buffer[i * 3 + 0] = (uint8_t)( sample        & 0xFF);
        buffer[i * 3 + 1] = (uint8_t)((sample >>  8) & 0xFF);
        buffer[i * 3 + 2] = (uint8_t)((sample >> 16) & 0xFF);
    }
}

/*
 * build_sinusoidal_buffer_32()
 *
 * Generate 32-bit signed PCM samples (little-endian)
 */

static void build_sinusoidal_buffer_32(uint8_t *buffer, int length, int mc10)
{
    double offset, increment = (PI * 2.0) / length;
    int i;

    offset = PI * 2.0;

    for (i = 0; i < length; i++)
    {
        int32_t sample;

        if (mc10)
        {
            /* Full-scale square wave */
            sample = (sin(increment * i + offset) > 0)
                     ? 0x7FFFFFFF
                     : (int32_t)0x80000000;
        }
        else
        {
            /* Proportional scaling from 16-bit reference */
            /* 25500 / 32767 ≈ 0.778 */
            sample = (int32_t)(sin(increment * i + offset) * 0x63C00000);
            /* 0x63C00000 ≈ 25500 << 16 */
        }

        /* Clamp just in case */
        if (sample >  0x7FFFFFFF) sample =  0x7FFFFFFF;
        if (sample < (int32_t)0x80000000) sample = (int32_t)0x80000000;

        /* WAV is little-endian, 32-bit */
        buffer[i * 4 + 0] = (uint8_t)( sample        & 0xFF);
        buffer[i * 4 + 1] = (uint8_t)((sample >>  8) & 0xFF);
        buffer[i * 4 + 2] = (uint8_t)((sample >> 16) & 0xFF);
        buffer[i * 4 + 3] = (uint8_t)((sample >> 24) & 0xFF);
    }
}

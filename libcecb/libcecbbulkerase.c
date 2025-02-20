/********************************************************************
 * libcebcbulkerase.c - Cassette BASIC create WAV/CAS routines
 *
 * $Id$
 ********************************************************************/

#include <cocotypes.h>
#include "cecbpath.h"

/*
 * _cecb_bulkerase()
 *
 * Create a container
 */

error_code _cecb_bulkerase(char *path, int sample_rate, int bits_per_sample,
			   double silence_length)
{
	error_code ec = 0;
	native_path_id nativepath;
	int i, headers_size, bytes_per_sample, silent_samples_count,
		silent_samples_bytes;

	_native_truncate(path, 0);

	/* 1. Open a path to the cassette image. */

	ec = _native_open(&nativepath, path, FAM_WRITE);

	if (ec != 0)
	{
		ec = _native_create(&nativepath, path, FAM_READ | FAM_WRITE,
				    FAP_READ | FAP_WRITE);

		if (ec != 0)
		{
			return (ec);
		}
	}

	_native_seek(nativepath, 0, SEEK_SET);

	if (strendcasecmp(path, CAS_FILE_EXTENSION) == 0)
	{
		_native_close(nativepath);
		return 0;
	}

	if (strendcasecmp(path, C10_FILE_EXTENSION) == 0)
	{
		_native_close(nativepath);
		return 0;
	}

	bytes_per_sample = bits_per_sample / 8;
	silent_samples_count = sample_rate * silence_length;
	silent_samples_bytes = silent_samples_count * bytes_per_sample;

	headers_size = 4 +	/* RIFF */
		4 +		/* Data size */
		4 +		/* RIFF type */
		4 +		/* fmt  chunk id */
		4 +		/* fmt  chunk size */
		18 +		/* fmt  chunk data */
		4 +		/* data chunk id */
		4 +		/* data chunk size */
		silent_samples_bytes;

	/* Set up WAV file format header */

	fwrite("RIFF", 4, 1, nativepath->fd);
	fwrite_le_int(headers_size - 8, nativepath->fd);
	fwrite("WAVE", 4, 1, nativepath->fd);

	fwrite("fmt ", 4, 1, nativepath->fd);
	fwrite_le_int(16, nativepath->fd);	/* chunk size */
	fwrite_le_short(1, nativepath->fd);	/* compression code: uncompressed */
	fwrite_le_short(1, nativepath->fd);	/* number of channels */
	fwrite_le_int(sample_rate, nativepath->fd);	/* sample rate */
	fwrite_le_int(sample_rate * bytes_per_sample, nativepath->fd);	/* average bytes per second */
	fwrite_le_short(1, nativepath->fd);	/* block align */
	fwrite_le_short(bits_per_sample, nativepath->fd);	/* significant bits per sample */

	fwrite("data", 4, 1, nativepath->fd);
	fwrite_le_int(silent_samples_bytes, nativepath->fd);	/* chunk size */

	for (i = 0; i < silent_samples_count; i++)
	{
		if (bits_per_sample == 8)
			fwrite_le_char(127, nativepath->fd);
		else
			fwrite_le_short(0, nativepath->fd);
	}

	_native_close(nativepath);

	return 0;
}

/********************************************************************
 * read.c - Disk BASIC Read routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "decbpath.h"


error_code _decb_read(decb_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	int curr_granule, last_granule;
	u_int accum_size = 0;
	int bytes_left;
	u_int filesize;


	/* 1. Check the mode. */

	if ((path->mode & FAM_READ) == 0)
	{
		return EOS_FNA;
	}


	/* 2. Treat raw path differently. */

	if (path->israw == 1)
	{
		unsigned int disksize = 35 * (18 * 256);


		if (path->filepos >= disksize)
		{
			*size = 0;

			ec = EOS_EOF;
		}
		else
		{
			fseek(path->fd, path->filepos, SEEK_SET);
			fread(buffer, 1, *size, path->fd);
			path->filepos += *size;
		}

		return ec;
	}


	/* 3. Get file's size. */

	ec = _decb_gs_size(path, &filesize);

	if (ec != 0)
	{
		return ec;
	}


	/* 4. If our file position is greater than the file size, return error. */

	if (path->filepos >= filesize)
	{
		/* 1. End of file */

		*size = 0;

		return EOS_EOF;
	}


	/* 5. If the passed size is greater than the length of the file minus
	 *    the file position, then reset the size
	 */

	if (*size > filesize - path->filepos)
	{
		*size = filesize - path->filepos;
	}


	/* 6. Determine which granule the offset starts by looping
	 *    through each entry until we reach the end.
	 */

	accum_size = 0;

	curr_granule = path->dir_entry.first_granule;
	last_granule = curr_granule;

	while (path->FAT[curr_granule] < 0xC0)
	{
		last_granule = curr_granule;
		curr_granule = path->FAT[curr_granule];

		accum_size += 2304;

		if (accum_size > path->filepos)
		{
			/* this is the granule! */
			accum_size -= 2304;
			curr_granule = last_granule;

			break;
		}
	}


	/* 7. Start copying data into the user supplied buffer for 'bytes_left' bytes. */

	bytes_left = *size;

	while (bytes_left > 0)
	{
		char granule_buffer[2304];
		int read_size, offset_in_granule;
		int bytes_in_last_granule = 2304;


		_decb_gs_granule(path, curr_granule, granule_buffer);

		if (path->FAT[curr_granule] >= 0xC0)
		{
			bytes_in_last_granule =
				(((path->FAT[curr_granule] & 0x3F) -
				  1) * 256) +
				int2(path->dir_entry.last_sector_size);
		}
		else
		{
			curr_granule = path->FAT[curr_granule];
		}


		offset_in_granule = path->filepos % 2304;


		read_size = bytes_in_last_granule - offset_in_granule;

		if (read_size > bytes_left)
		{
			read_size = bytes_left;
		}


		memcpy(buffer, granule_buffer + offset_in_granule, read_size);

		bytes_left -= read_size;
		path->filepos += read_size;
		buffer += read_size;
	}


	return ec;
}



error_code _decb_readln(decb_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	int curr_granule, last_granule;
	u_int accum_size = 0;
	u_int bytes_left;
	u_int filesize;


	/* 1. Check the mode. */

	if ((path->mode & FAM_READ) == 0)
	{
		return EOS_FNA;
	}


	/* 2. Treat raw path differently. */

	if (path->israw == 1)
	{
		unsigned int disksize = 35 * (18 * 256);


		if (path->filepos >= disksize)
		{
			*size = 0;

			ec = EOS_EOF;
		}
		else
		{
			fseek(path->fd, path->filepos, SEEK_SET);
			fread(buffer, 1, *size, path->fd);
			path->filepos += *size;
		}

		return ec;
	}


	/* 3. Get file's size. */

	ec = _decb_gs_size(path, &filesize);

	if (ec != 0)
	{
		return ec;
	}


	/* 4. If our file position is greater than the file size, return error. */

	if (path->filepos >= filesize)
	{
		/* 1. End of file */

		*size = 0;

		return EOS_EOF;
	}


	/* 5. If the passed size is greater than the length of the file minus
	 *    the file position, then reset the size
	 */

	if (*size > filesize - path->filepos)
	{
		*size = filesize - path->filepos;
	}


	/* 6. Determine which granule the offset starts by looping
	 *    through each entry until we reach the end.
	 */

	accum_size = 0;

	curr_granule = path->dir_entry.first_granule;
	last_granule = curr_granule;

	while (path->FAT[curr_granule] < 0xC0)
	{
		last_granule = curr_granule;
		curr_granule = path->FAT[curr_granule];

		accum_size += 4608 / 2;

		if (accum_size > path->filepos)
		{
			/* this is the granule! */
			accum_size -= 4608 / 2;
			curr_granule = last_granule;

			break;
		}
	}


	/* 7. Start copying data into the user supplied buffer for 'bytes_left' bytes. */

	bytes_left = *size;

	while (bytes_left > 0)
	{
		char *buf_ptr = buffer;
		char granule_buffer[2304], *z;
		u_int read_size, offset_in_granule;
		int bytes_in_last_granule = 2304;


		_decb_gs_granule(path, curr_granule, granule_buffer);

		if (path->FAT[curr_granule] >= 0xC0)
		{
			bytes_in_last_granule =
				((path->FAT[curr_granule] & 0x3F) * 256) +
				int2(path->dir_entry.last_sector_size);
		}
		else
		{
			curr_granule = path->FAT[curr_granule];
		}


		offset_in_granule = path->filepos % 2304;

		read_size = bytes_in_last_granule - offset_in_granule;

		if (read_size > bytes_left)
		{
			read_size = bytes_left;
		}


		memcpy(buffer, granule_buffer + offset_in_granule, read_size);


		/* 4. Look for line terminator in this fresh buffer. */

		for (z = buf_ptr; z < buf_ptr + read_size; z++)
		{
			if (*z == 0x0D)
			{
				read_size = (int) (z - buf_ptr) + 1;
				/* Include bytes already read from earlier granules. */
				*size = *size - bytes_left + read_size;
				bytes_left = read_size;
				break;
			}
		}


		bytes_left -= read_size;
		path->filepos += read_size;
		buffer += read_size;
	}


	return ec;
}



error_code _decb_readdir(decb_path_id path, decb_dir_entry * dirent)
{
	error_code ec = 0;
	unsigned char buffer[256];
	int sector;
	int entry_in_sector;

	/* 1. Calculate sector and sector offset */

	sector = (path->directory_entry_index * sizeof(decb_dir_entry)) / 256;
	entry_in_sector =
		(path->directory_entry_index++ * sizeof(decb_dir_entry)) %
		256;


	/* 2. Check if we have passed last entry. */

	if (path->directory_entry_index == 73)
	{
		/* 1. Yes.  Return error. */

		path->directory_entry_index = 72;

		return (EOS_PNNF);
	}


	ec = _decb_gs_sector(path, 17, 3 + sector, (char *) buffer);

	if (ec == 0)
	{
		memcpy(dirent, buffer + entry_in_sector,
		       sizeof(decb_dir_entry));
	}


	return (ec);
}

error_code _decb_ncpy_name(decb_dir_entry e, u_char * name, size_t len)
{
	error_code ec = 0;
	u_char c_name[13];

	DECBStringToCString(e.filename, e.file_extension, c_name);
	c_name[12] = 0;

	strncpy((char *) name, (const char *) c_name, len);

	return ec;

}

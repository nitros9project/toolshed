/********************************************************************
 * read.c - OS-9 Read routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include <cocotypes.h>
#include <os9path.h>


/*
 * Read the passed logical sector number.
 */
int read_lsn(os9_path_id path, int lsn, void *buffer)
{
	return _os9_lsn_read(path, lsn, 0, buffer, path->bps);
}


error_code _os9_read(os9_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;
	fd_stats fd_sector;
	Fd_seg segptr;
	int i;
	u_int accum_size = 0;
	int bytes_left;
	char *buf_ptr = buffer;
	int seg_size_bytes, read_size;
	u_int filesize;


	/* 1. Check the mode. */

	if ((path->mode & FAM_DIR) != 0 || (path->mode & FAM_READ) == 0)
	{
		/* 1. Must use _os9_readdir. */

		return EOS_BMODE;
	}


	/* 2. Treat raw path differently. */

	if (path->israw == 1)
	{
		unsigned int disksize = int3(path->lsn0->dd_tot) * path->bps;


		if (path->filepos >= disksize)
		{
			*size = 0;

			return EOS_EOF;
		}

		if (*size > disksize - path->filepos)
		{
			*size = disksize - path->filepos;
		}

		{
			/* Stop before a missing sector; only fail if nothing could be read. */

			u_int count = _os9_lsn_read(path, 0, path->filepos, buffer, *size);

			if (count == 0)
			{
				ec = _os9_lsn_get(path, 0, path->filepos, buffer, *size);
			}

			*size = count;
			path->filepos += count;
		}


		return ec;
	}


	/* 2. Read the file descriptor sector */

	ec = _os9_lsn_get(path, path->pl_fd_lsn, 0, &fd_sector, sizeof(fd_stats));

	if (ec != 0)
	{
		*size = 0;

		return ec;
	}


	/* 4. Point to segment list */

	segptr = (Fd_seg) & (fd_sector.fd_seg);


	/* 5. Extract file size from FD */

	filesize = int4(fd_sector.fd_siz);


	/* 6. If our file position is greater than the file size, return error */

	if (path->filepos >= filesize)
	{
		/* 1. End of file reached. */

		*size = 0;

		return EOS_EOF;
	}


	/* 7. If the passed size is greater than the length of the file minus
	 *    the file position, then reset the size
	 */
	if (*size > filesize - path->filepos)
	{
		*size = filesize - path->filepos;
	}


	/* 8. Determine which segment the offset starts by looping
	 *    through each segptr entry until we reach the end
	 */

	accum_size = 0;

	for (i = 0; i < NUM_SEGS || int3(segptr[i].lsn) != 0; i++)
	{
		accum_size += int2(segptr[i].num) * path->bps;

		if (accum_size > path->filepos)
		{
			/* 1. This is the sector! */

			accum_size -= int2(segptr[i].num) * path->bps;
			break;
		}
	}


	/* 9. make out-of-loop check to insure we exited from the for()
	 *    loop due to finding the proper sector, and not because we
	 *    ran out of sectors in the segment list to search.
	 */

	if (int3(segptr[i].lsn) == 0 || i == NUM_SEGS)
	{
		/* 1. Apparently, the file position in the path was too
		 *    large, because we couldn't find a sector.
		 */
		*size = 0;

		return 1;
	}


	/* 10. Start copying data into the user supplied buffer for 'bytes_left' bytes.
	 *
	 * i == segment entry to start
	 */

	bytes_left = *size;

	while (bytes_left > 0 && i != NUM_SEGS && int3(segptr[i].lsn) != 0)
	{
		accum_size += int2(segptr[i].num) * path->bps;


		/* 1. Compute the segment size. */

		seg_size_bytes = int2(segptr[i].num) * path->bps;


		/* 2. Compute read size for this segment. */

		read_size = accum_size - path->filepos;

		if (read_size > bytes_left)
		{
			read_size = bytes_left;
		}


		/* 3. Read from the offset within the segment. */

		ec = _os9_lsn_get(path, int3(segptr[i].lsn),
				  path->filepos - (accum_size - seg_size_bytes),
				  buf_ptr, read_size);

		if (ec != 0)
		{
			*size -= bytes_left;

			return ec;
		}

		buf_ptr += read_size;
		path->filepos += read_size;
		bytes_left -= read_size;
		i++;
	}


	return ec;
}



error_code _os9_readdir(os9_path_id path, os9_dir_entry * dirent)
{
	error_code ec = 0;


	if (!(path->mode & FAM_DIR))
	{
		/* 1. Must be a directory. */

		ec = EOS_BMODE;
	}
	else
	{
		u_int size = sizeof(os9_dir_entry);
		int temp_mode = path->mode;

		/* 1. Temporarily turn off FAM_DIR so that read won't fail. */

		path->mode &= ~FAM_DIR;
		path->mode |= FAM_READ;

		ec = _os9_read(path, dirent, &size);

		path->mode = temp_mode;
	}


	return ec;
}

error_code _os9_ncpy_name(os9_dir_entry e, u_char * name, size_t len)
{
	error_code ec = 0;
	u_char c_name[D_NAMELEN];

	memcpy(c_name, e.name, D_NAMELEN);
	OS9StringToCString(c_name);

	strncpy((char *) name, (const char *) c_name, len);

	return ec;

}

/********************************************************************
 * readln.c - OS-9 ReadLn routine
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "os9path.h"


error_code _os9_readln(os9_path_id path, void *buffer, u_int * size)
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

		return EOS_FNA;
	}


	/* 2. Seek to FD LSN of pathlist */

	_os9_lsn_fseek(path, path->pl_fd_lsn);


	/* 3. Read the file descriptor sector */

	fread(&fd_sector, 1, sizeof(fd_stats), path->fd);


	/* 4. Point to segment list */

	segptr = (Fd_seg) & (fd_sector.fd_seg);


	/* 5. Extract file size from FD */

	filesize = int4(fd_sector.fd_siz);


	/* 6. If our file position is greater than the file size, return error */

	if (path->filepos >= filesize)
	{
		/* end of file */
		return (EOS_EOF);
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


	/* 9. Make out-of-loop check to insure we exited from the for()
	 *    loop due to finding the proper sector, and not because we
	 *    ran out of sectors in the segment list to search.
	 */
	if (int3(segptr[i].lsn) == 0 || i == NUM_SEGS)
	{
		/* 1. Apparently, the file position in the path was too
		 * large, because we couldn't find a sector.
		 */

		return 1;
	}


	/* 10. Start copying data into the user supplied buffer for 'bytes_left' bytes.
	 *
	 * i == segment entry to start
	 */

	bytes_left = *size;

	while (bytes_left > 0 && i != NUM_SEGS && int3(segptr[i].lsn) != 0)
	{
		char *z;

		accum_size += int2(segptr[i].num) * path->bps;


		/* 1. Seek to sector where segment starts and compute the segment size. */

		_os9_lsn_fseek(path, int3(segptr[i].lsn));
		seg_size_bytes = int2(segptr[i].num) * path->bps;


		/* 2. Seek within segment to offset. */

		fseek(path->fd, path->filepos - (accum_size - seg_size_bytes),
		      SEEK_CUR);


		/* 3. Compute read size for this segment. */

		read_size = accum_size - path->filepos;

		if (read_size > bytes_left)
		{
			read_size = bytes_left;
		}

		fread(buf_ptr, 1, read_size, path->fd);


		/* 4. Look for line terminator in this fresh buffer. */

		for (z = buf_ptr; z < buf_ptr + read_size; z++)
		{
			if (*z == 0x0D)
			{
				read_size = (int) (z - buf_ptr) + 1;
				/* Include bytes already read from earlier segments. */
				*size = (u_int) (buf_ptr - (char *) buffer) + read_size;
				bytes_left = read_size;
				break;
			}
		}

		buf_ptr += read_size;
		path->filepos += read_size;
		bytes_left -= read_size;
		i++;
	}


	return ec;
}

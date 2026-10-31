/********************************************************************
 * write.c - OS-9 Write routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "os9path.h"


static int _os9_extendSegList(os9_path_id path, Fd_seg segptr, int *delta);


error_code _os9_write(os9_path_id path, void *buffer, u_int * size)
{
	error_code ec = 0;


	/* 1. Check the mode. */

	if (path->mode & FAM_DIR || !(path->mode & FAM_WRITE))
	{
		return EOS_BMODE;
	}


	/* 2. Treat raw path differently. */

	if (path->israw == 1)
	{
		*size = fwrite(buffer, 1, *size, path->fd);
		path->filepos += *size;
	}
	else
	{
		fd_stats fd_sector;
		Fd_seg segptr;
		int i;
		u_int accum_size = 0;
		int bytes_left;
		char *buf_ptr = buffer;
		int seg_size_bytes, write_size;
		u_int filesize;

		/* 1. Seek to FD LSN of pathlist */

		_os9_lsn_fseek(path, path->pl_fd_lsn);


		/* 2. Read the file descriptor sector */

		fread(&fd_sector, 1, sizeof(fd_stats), path->fd);


		/* 3. Point to segment list */

		segptr = (Fd_seg) & (fd_sector.fd_seg);


		/* 4. Extract file size from FD */

		filesize = int4(fd_sector.fd_siz);


		/* 5. If our file position is greater than the file size, return error */

		if (path->filepos > filesize)
		{
			/* 1. End of file. */

			return EOS_EOF;
		}


		/* 6a. Determine maximum file size by adding up the segment list */

		accum_size = _os9_maximum_file_size(fd_sector, path->bps);


		/* 6b. If there is not enough room, we need to extend the segment list */

		while (accum_size < path->filepos + *size)
		{
			int delta;


			/* 1. We need to enlarge the file. Delta will contain the number of bytes added. */

			ec = _os9_extendSegList(path, segptr, &delta);


			/* 1. If there is an error, time to abort. */

			if (ec != 0)
			{
				return (ec);
			}

			accum_size += delta;
		}


		/* 7. Determine which segment the offset starts by looping
		 *    through each segptr entry until we reach the end
		 */

		accum_size = 0;

		for (i = 0; i < NUM_SEGS && int3(segptr[i].lsn) != 0; i++)
		{
			accum_size += int2(segptr[i].num) * path->bps;
			if (accum_size > path->filepos)
			{
				/* this is the sector! */
				accum_size -= int2(segptr[i].num) * path->bps;
				break;
			}
		}


		/* 8. make out-of-loop check to insure we exited from the for()
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


		/* 9. Start copying the user supplied data into the file for 'bytes_left' bytes.
		 *
		 * i == segment entry to start
		 */

		bytes_left = *size;

		while (bytes_left > 0 && i != NUM_SEGS
		       && int3(segptr[i].lsn) != 0)
		{
			accum_size += int2(segptr[i].num) * path->bps;

			/* 1. Compute the segment size. */

			seg_size_bytes = int2(segptr[i].num) * path->bps;


			/* 2. Compute write size for this segment. */

			write_size = accum_size - path->filepos;

			if (write_size > bytes_left)
			{
				write_size = bytes_left;
			}


			/* 3. Write at the offset within the segment. */

			_os9_lsn_write(path, int3(segptr[i].lsn),
				       path->filepos - (accum_size - seg_size_bytes),
				       buf_ptr, write_size);
			buf_ptr += write_size;
			path->filepos += write_size;
			bytes_left -= write_size;
			i++;
		}


		/* 10. Update fd_siz if necessary. */

		if (path->filepos > int4(fd_sector.fd_siz))
		{
			/* 1. Update file size. */

			_int4(path->filepos, fd_sector.fd_siz);
		}


		/* 11. TODO - Update modification date/time */


		/* 12. Write updated file descriptor back to image file */

		_os9_lsn_fseek(path, path->pl_fd_lsn);
		fwrite(&fd_sector, 1, sizeof(fd_stats), path->fd);
	}

	return ec;
}



static int _os9_extendSegList(os9_path_id path, Fd_seg segptr, int *delta)
{
	error_code ec = 0;
	int i = 0, newNum, newLSN;

	/* Is segment list empty */
	if (int3(segptr[0].lsn) != 0)
	{
		/* Find last segment */
		for (i = 1; i < NUM_SEGS; i++)
		{
			if (int3(segptr[i].lsn) == 0)
			{
				break;
			}
		}

		if (i == NUM_SEGS)
		{
			return EOS_SF;
		}

		i--;		/* adjust index to point to last segment */

		/* Add a cluster to segment length */
		newNum = int2(segptr[i].num) + path->spc;

		/* Did short value overflow? */
		if (newNum < 0x10000)
		{
			/* Calculate LSN for new sector */
			newLSN = int3(segptr[i].lsn) + newNum - 1;

			/* Is it allocated? Also check if it is in the same
			   allocation map sector (due to OS-9 6x09 RBF limitation) */
			if (newLSN / path->spc / 2048 ==
			    int3(segptr[i].lsn) / path->spc / 2048
			    && !_os9_ckbit(path->bitmap, newLSN / path->spc))
			{
				/* Hey we got our extra cluster! */
				_os9_allbit(path->bitmap,
					    (newLSN / path->spc), 1);
				_int2(newNum, segptr[i].num);

				*delta = path->cs;
				return 0;
			}
		}

		i++;		/* adjust index to point to unused segment */
	}

	/* Time for a new segment */
	/* Is there room for another segment? */
	if (i < NUM_SEGS)
	{
		int cluster, size;

		/* Get new segment. Function will also allocate the sector */
		ec = _os9_getSASSegment(path, &cluster, &size);
		if (ec == 0)
		{
			_int3(cluster, segptr[i].lsn);
			_int2(size, segptr[i].num);
			*delta = path->bps * size;
		}
		else
		{
			ec = EOS_DF;
		}
	}
	else
	{
		ec = EOS_SF;
	}

	return ec;
}



error_code _os9_writedir(os9_path_id path, os9_dir_entry * dirent)
{
	error_code ec = 0;

	if ((path->mode & FAM_DIR) == 0)
	{
		/* 1. Must be a directory. */
		ec = EOS_BMODE;
	}
	else
	{
		u_int size = sizeof(os9_dir_entry);

		/* 1. Temporarily turn off FAM_DIR so that read won't fail. */
		path->mode &= ~FAM_DIR;
		ec = _os9_write(path, dirent, &size);
		path->mode |= FAM_DIR;
	}

	return ec;
}

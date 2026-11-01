/********************************************************************
 * libdragondosread.c - Read files
 ********************************************************************/
#include <string.h>

#include "libdragondosint.h"


error_code _dragondos_read(dragondos_path_id path, void *buffer, u_int *size)
{
	u_char *out = buffer;
	u_int wanted = *size;
	u_int done = 0;

	*size = 0;
	if (path->israw || !(path->mode & FAM_READ))
	{
		return EOS_BMODE;
	}
	if (wanted == 0)
	{
		return 0;
	}
	if (path->filepos >= path->filesize)
	{
		return EOS_EOF;
	}
	if (wanted > path->filesize - path->filepos)
	{
		wanted = path->filesize - path->filepos;
	}

	while (done < wanted)
	{
		u_char sector[DRAGONDOS_SECTOR_SIZE];
		u_int offset = path->filepos % DRAGONDOS_SECTOR_SIZE;
		u_int chunk = DRAGONDOS_SECTOR_SIZE - offset;
		u_int lsn;
		error_code ec = _dd_sector_lsn(path, path->filepos / DRAGONDOS_SECTOR_SIZE, &lsn);

		if (ec == 0)
		{
			ec = _dd_read_lsn(path, lsn, sector);
		}
		if (ec != 0)
		{
			*size = done;
			return ec;
		}

		if (chunk > wanted - done)
		{
			chunk = wanted - done;
		}
		memcpy(out + done, sector + offset, chunk);
		done += chunk;
		path->filepos += chunk;
	}

	*size = done;
	return 0;
}

error_code _dragondos_readln(dragondos_path_id path, void *buffer, u_int *size)
{
	u_char *out = buffer;
	u_int wanted = *size;
	u_int done = 0;

	*size = 0;
	while (done < wanted)
	{
		u_int one = 1;
		error_code ec = _dragondos_read(path, out + done, &one);

		if (ec != 0)
		{
			*size = done;
			return (ec == EOS_EOF && done != 0) ? 0 : ec;
		}
		if (out[done++] == '\r')
		{
			break;
		}
	}

	*size = done;
	return 0;
}

error_code _dragondos_seek(dragondos_path_id path, long offset, int mode)
{
	long long position;

	if (path->israw)
	{
		return EOS_BMODE;
	}

	switch (mode)
	{
	case SEEK_SET:
		position = offset;
		break;
	case SEEK_CUR:
		position = (long long)path->filepos + offset;
		break;
	case SEEK_END:
		position = (long long)path->filesize + offset;
		break;
	default:
		return EOS_IA;
	}

	if (position < 0 || position > DD_MAX_LSN * DRAGONDOS_SECTOR_SIZE)
	{
		return EOS_SE;
	}

	path->filepos = (u_int)position;
	return 0;
}

error_code _dragondos_gs_size(dragondos_path_id path, u_int *size)
{
	if (path->israw)
	{
		return EOS_BMODE;
	}
	*size = path->filesize;
	return 0;
}

error_code _dragondos_gs_pos(dragondos_path_id path, u_int *pos)
{
	if (path->israw)
	{
		return EOS_BMODE;
	}
	*pos = path->filepos;
	return 0;
}

error_code _dragondos_gs_eof(dragondos_path_id path)
{
	if (path->israw)
	{
		return EOS_BMODE;
	}
	return path->filepos >= path->filesize ? EOS_EOF : 0;
}

error_code _dragondos_gs_fd(dragondos_path_id path, dragondos_file_stat *stat)
{
	dragondos_dir_entry *entry;
	error_code ec;

	if (path->israw)
	{
		return EOS_BMODE;
	}

	ec = _dd_entry(path, path->entry_index, &entry);
	if (ec != 0)
	{
		return ec;
	}

	stat->protected_file = (entry->flags & DRAGONDOS_FLAG_PROTECTED) != 0;
	stat->file_size = path->filesize;
	return 0;
}

error_code _dragondos_gs_sector(dragondos_path_id path, u_int lsn, void *buffer)
{
	return _dd_read_lsn(path, lsn, buffer);
}

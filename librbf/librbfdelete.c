/********************************************************************
 * delete.c - Delete file entry for OS-9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "cocotypes.h"
#include "os9path.h"
#include "cococonv.h"
#include "util.h"


u_char DecrementLinkCount(os9_path_id path, int fd_lsn);
static int _os9_freefile(char *filePath, u_char * bitmap);


error_code _os9_delete_directory(char *pathlist)
{
	error_code ec;
	os9_path_id fold_path;
	fd_stats fdbuf;

	/* open a path to the device */
	ec = _os9_open(&fold_path, pathlist, FAM_READ | FAM_DIR);

	if (ec != 0)
	{
		return ec;
	}

	while (_os9_gs_eof(fold_path) == 0)
	{
		u_int i = 0;
		os9_dir_entry dentry;
		char *dirpath;
		os9_path_id path2;

		ec = _os9_readdir(fold_path, &dentry);

		if (ec != 0)
		{
			return ec;
		}

		OS9StringToCString(dentry.name);

		/* Skip over dot directories and empty entries. */
		if (dentry.name[0] == '\0')
			continue;
		if (strcmp((char *) dentry.name, ".") == 0)
			continue;
		if (strcmp((char *) dentry.name, "..") == 0)
			continue;

		dirpath =
			malloc(strlen((char *) pathlist) +
			       strlen((char *) dentry.name) + 2);
		if (dirpath == NULL)
		{
			return 1;
		}

		_os9_close(fold_path);

		strcpy(dirpath, pathlist);
		strcat(dirpath, "/");
		strcat(dirpath, (char *) dentry.name);

		/* Determine if file is really another directory */
		ec = _os9_open(&path2, dirpath, FAM_DIR | FAM_READ);
		if (ec == 0)
		{
			/* Yup it is a directory, we need to delete it */
			_os9_close(path2);
			ec = _os9_delete_directory(dirpath);

			if (ec != 0)
			{
				/* Error */
				free(dirpath);
				return ec;
			}
		}

		/* Delete file */
		ec = _os9_delete(dirpath);
		free(dirpath);

		/* Open orginal directory */
		ec = _os9_open(&fold_path, pathlist, FAM_WRITE | FAM_DIR);
		ec = _os9_seek(fold_path, i * sizeof(fd_stats), SEEK_SET);

		/* Incement directory entry count */
		i++;
	}

	_os9_close(fold_path);

	/* All directory entries have been deleted.
	   Turn off directory attribute, and delete directory file */

	/* Reopen in write mode */
	ec = _os9_open(&fold_path, pathlist, FAM_WRITE | FAM_DIR);

	if (ec != 0)
	{
		return ec;
	}

	ec = _os9_gs_fd(fold_path, sizeof(fd_stats), &fdbuf);
	if (ec != 0)
	{
		_os9_close(fold_path);
		return ec;
	}
	fdbuf.fd_att &= ~FAP_DIR;
	ec = _os9_ss_fd(fold_path, sizeof(fd_stats), &fdbuf);
	if (ec != 0)
	{
		_os9_close(fold_path);
		return ec;
	}

	ec = _os9_close(fold_path);
	if (ec != 0)
	{
		return ec;
	}

	ec = _os9_delete(pathlist);

	return ec;
}


error_code _os9_delete(char *pathlist)
{
	error_code ec = 0;
	os9_path_id parent_path;
	char *filename = NULL;
	int deleted = 0;


	/* 1. Determine is path is a folder. */

	ec = _os9_open(&parent_path, pathlist, FAM_DIR | FAM_READ);

	if (ec == 0)
	{
		/* Cannot use this to delete directories */
		/* You must remove the contents of the directory and change
		 * the directory file into a regular file
		 */

		_os9_close(parent_path);

		return EOS_IC;
	}


	/* 2. Open parent directory. */

	ec = _os9_open_parent_directory(&parent_path, pathlist,
					FAM_DIR | FAM_WRITE, &filename);

	if (ec != 0)
	{
		if (filename != NULL)
			free(filename);
		return (ec);
	}


	/* 3. Return on illegal filename. */

	if (!strcasecmp(filename, "."))
	{
		free(filename);
		_os9_close(parent_path);

		return EOS_IA;
	}

	if (!strcasecmp(filename, ".."))
	{
		free(filename);
		_os9_close(parent_path);

		return EOS_IA;
	}


	/* 4. Start reading directory file and search for match. */

	while (_os9_gs_eof(parent_path) == 0)
	{
		os9_dir_entry dentry;
		char fname[32];


		ec = _os9_readdir(parent_path, &dentry);

		if (ec != 0)
		{
			break;
		}

		strncpy(fname, (char *) dentry.name, 29);

		OS9StringToCString((u_char *) fname);

		if (!strcasecmp(fname, filename))
		{
			/* Decrement link count in file descriptor */

			if (DecrementLinkCount(parent_path, int3(dentry.lsn))
			    < 1)
			{
				/* Only deallocate file if link count is zero */

				/* Deallocate any bits used by the file */

				ec = _os9_freefile(pathlist,
						   parent_path->bitmap);

				/* Deallocate the bit used for the file descriptor */

				_os9_delbit(parent_path->bitmap,
					    int3(dentry.lsn) /
					    parent_path->spc, 1);
			}


			/* Back up file pointer in preparation of updating directory entry */

			_os9_seek(parent_path, -(int) sizeof(dentry),
				  SEEK_CUR);


			/* Putting a NULL in the first charcter position identifies the file
			   as deleted */

			dentry.name[0] = '\0';


			/* Write the directory entry back to the image */

			ec = _os9_writedir(parent_path, &dentry);


			/* Flag that the file has been deleted. */

			deleted = 1;

			break;
		}
	}

	free(filename);
	_os9_close(parent_path);

	if (deleted == 0)
	{
		ec = EOS_PNNF;
	}


	return ec;
}



u_char DecrementLinkCount(os9_path_id path, int fd_lsn)
{
	fd_stats fdbuf;
	u_char result;


	_os9_lsn_fseek(path, fd_lsn);
	fread(&fdbuf, 1, sizeof(fd_stats), path->fd);

	result = fdbuf.fd_lnk = fdbuf.fd_lnk - 1;

	_os9_lsn_fseek(path, fd_lsn);
	fwrite(&fdbuf, 1, sizeof(fd_stats), path->fd);


	return result;
}



static int _os9_freefile(char *filePath, u_char * bitmap)
{
	os9_path_id path;
	fd_stats fdbuf;
	Fd_seg seg;
	unsigned int spc;
	int i;
	int ec = 0;


	ec = _os9_open(&path, filePath, FAM_READ);
	ec = _os9_gs_fd(path, sizeof(fd_stats), &fdbuf);

	spc = path->spc;

	ec = _os9_close(path);

	seg = fdbuf.fd_seg;

	for (i = 0; i < NUM_SEGS; i++)
	{
		if (int3(seg[i].lsn) == 0)
		{
			break;
		}

		ec = _os9_delbit(bitmap, int3(seg[i].lsn) / spc,
				 int2(seg[i].num) / spc);

		if (ec != 0)
		{
			return ec;
		}
	}


	return ec;
}

/********************************************************************
 * rename.c - Rename file entry for OS-9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <cocotypes.h>
#include <os9path.h>
#include <cococonv.h>
#include <cocosys.h>
#include <util.h>


error_code _os9_rename(char *pathlist, char *new_name)
{
	os9_dir_entry dentry;

	return _os9_rename_ex(pathlist, new_name, &dentry);
}


error_code _os9_rename_ex(char *pathlist, char *new_name,
			  os9_dir_entry * dentry)
{
	error_code ec = 0;
	os9_path_id parent_path;
	char *filename = NULL;

	if (strcasecmp(new_name, ".") == 0
	    || (new_name[0] == '.' && new_name[1] == '.'))
	{
		return (EOS_IA);
	}

	ec = _os9_prsnam(new_name);

	if (ec != 0)
	{
		return ec;
	}

	ec = _os9_open_parent_directory(&parent_path, pathlist,
					FAM_DIR | FAM_WRITE, &filename);

	if (ec != 0)
	{
		if (filename != NULL)
			free(filename);
		return (ec);
	}

	/* Return on illegal filename */

	if (strcasecmp(filename, ".") == 0 || strcasecmp(filename, "..") == 0)
	{
		free(filename);
		_os9_close(parent_path);

		return (EOS_IA);
	}

	/* See if another file in this directory has the same name as our destination */

	while ((ec = _os9_gs_eof(parent_path)) == 0)
	{
		int size;
		u_char fname[32];

		size = sizeof(dentry);
		ec = _os9_readdir(parent_path, dentry);

		if (ec != 0 || size != sizeof(dentry))
		{
			break;
		}

		memcpy(fname, dentry->name, 29);

		OS9StringToCString(fname);

		if (strcasecmp((char *) fname, new_name) == 0
		    && strcasecmp((char *) fname, filename) != 0)
		{
			ec = EOS_FAE;

			break;
		}
	}

	if (ec == EOS_FAE)
	{
		free(filename);
		_os9_close(parent_path);

		return ec;
	}

	_os9_seek(parent_path, 0, SEEK_SET);

	/* Start reading directory file and search for match */

	while ((ec = _os9_gs_eof(parent_path)) == 0)
	{
		int size;
		u_char fname[32];	/* while dentry->name is 29 wide */

		size = sizeof(dentry);
		ec = _os9_readdir(parent_path, dentry);

		if (ec != 0 || size != sizeof(dentry))
		{
			break;
		}

		memcpy(fname, dentry->name, sizeof(dentry->name));

		OS9StringToCString(fname);

		if (strcasecmp((char *) fname, filename) == 0)
		{
			/* Found the source, rename it */
			/* Via the larger buffer so that C string zero can fit */
			strcpy((char *) fname, new_name);	/* size is verified above */
			CStringToOS9String(fname);
			memcpy(dentry->name, fname, sizeof(dentry->name));

			/* Back up file pointer in preparation of updating directory entry */
			_os9_seek(parent_path, -(int) sizeof(*dentry),
				  SEEK_CUR);

			/* Write the directory entry back to the image */
			ec = _os9_writedir(parent_path, dentry);

			break;
		}
	}

	// if we get an end-of-file error, it means that the file doesn't exist -- return the right error in that case.
	if (ec == EOS_EOF)
	{
		ec = EOS_PNNF;
	}

	free(filename);
	_os9_close(parent_path);

	return (ec);
}

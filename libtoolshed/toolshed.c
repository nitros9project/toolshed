/********************************************************************
 * $Id$
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <direct.h>
#endif

#include <toolshed.h>

const char *OES_SUC_str = "SUCCESS!";
const char *EOS_FNA_str = "the file's permissions make it inaccessible to you";
const char *EOS_EOF_str = "input past end-of-file";
const char *EOS_FAE_str = "file already exists";
const char *EOS_BPNAM_str = "badly formed pathname";
const char *EOS_PNNF_str = "pathname not found";
const char *EOS_WRITE_str = "error writing to file";
const char *EOS_DF_str = "disk is filled to capacity";
const char *EOS_PADROM_str = "file is larger than pad size";
const char *EOS_WT_str = "attempt to read an incompatible media";
const char *EOS_MF_str = "memory full";
const char *EOS_CRC_str = "CRC error";
const char *EOS_UNKN_str = "unknown error";
const char *EOS_IA_str = "illegal filename";
const char *EOS_IC_str = "illegal change to directory file";
const char *EOS_PTHFUL_str = "The file cannot be opened because the system path table is currently full";
const char *EOS_BMODE_str = "attempt to perform I/O function of which the device or file is incapable";
const char *EOS_SF_str = "file is too fragmented to be expanded further";
const char *EOS_SE_str = "physical seek to non-existant sector";

const char *TSReportError(error_code te)
{
	switch (te)
	{
	case 0:
		return OES_SUC_str;
		break;

	case EOS_FNA:
		return EOS_FNA_str;
		break;

	case EOS_EOF:
		return EOS_EOF_str;
		break;

	case EOS_FAE:
		return EOS_FAE_str;
		break;

	case EOS_BPNAM:
		return EOS_BPNAM_str;
		break;

	case EOS_PNNF:
		return EOS_PNNF_str;
		break;

	case EOS_WRITE:
		return EOS_WRITE_str;
		break;

	case EOS_DF:
		return EOS_DF_str;
		break;

	case EOS_PADROM:
		return EOS_PADROM_str;
		break;

	case EOS_WT:
		return EOS_WT_str;
		break;

	case EOS_MF:
		return EOS_MF_str;
		break;

	case EOS_CRC:
		return EOS_CRC_str;
		break;

	case EOS_IA:
		return EOS_IA_str;
		break;

	case EOS_IC:
		return EOS_IC_str;
		break;

	case EOS_PTHFUL:
		return EOS_PTHFUL_str;
		break;

	case EOS_BMODE:
		return EOS_BMODE_str;
		break;

	case EOS_SF:
		return EOS_SF_str;
		break;

	case EOS_SE:
		return EOS_SE_str;
		break;

	default:
		return EOS_UNKN_str;
		break;
	}
}


int TSIsDirectory(char *pathlist)
{
	error_code ec = 0;
	coco_file_stat statbuf;

	ec = _coco_gs_fd_pathlist(pathlist, &statbuf);
	if (ec == 0 && statbuf.attributes & FAM_DIR)
	{
		return 1;
	}

	return 0;
}


error_code TSRename(char *pathlist, char *new_name)
{
	error_code ec = 0;

	ec = _coco_rename(pathlist, new_name);

	return ec;
}



error_code TSDelete(char *pathlist)
{
	error_code ec = 0;

	ec = _coco_delete(pathlist);

	if (ec != 0)
	{
		ec = _coco_delete_directory(pathlist);
	}

	return ec;
}



error_code TSPadROM(char *pathlist, int padSize, char padChar, int padAtStart)
{
	error_code ec = 0;
	coco_path_id path;
	int j;
	u_int fileSize;


	ec = _coco_open(&path, pathlist, FAM_READ | FAM_WRITE);

	if (ec != 0)
	{
		return ec;
	}


	ec = _coco_gs_size(path, &fileSize);

	if (ec != 0)
	{
		_coco_close(path);

		return ec;
	}

	if (padSize == fileSize)
	{
		_coco_close(path);

		return 0;
	}

	if (padSize < fileSize)
	{
		_coco_close(path);

		return EOS_PADROM;
	}

	if (padAtStart == 0)
	{
		_coco_seek(path, fileSize, SEEK_SET);

		for (j = 0; j < padSize - fileSize; j++)
		{
			u_int size = 1;

			_coco_write(path, &padChar, &size);
		}
	}
	else
	{
		// read contents
		char *contents = malloc(fileSize);
		if (contents != NULL)
		{
			u_int size = fileSize;

			ec = _coco_read(path, contents, &size);

			if (ec == 0)
			{
				_coco_seek(path, 0, SEEK_SET);

				for (j = 0; j < padSize - fileSize; j++)
				{
					u_int size = 1;

					_coco_write(path, &padChar, &size);
				}

				size = fileSize;

				_coco_write(path, contents, &fileSize);
			}

			free(contents);
			contents = NULL;
		}
	}

	_coco_close(path);


	return 0;
}



error_code TSRBFAttrGet(char *p, char *attr, char *strattr)
{
	error_code ec = 0;
	os9_path_id path;

	/* open a path to the device */
	ec = _os9_open(&path, p, FAM_READ);

	if (ec != 0)
	{
		ec = _os9_open(&path, p, FAM_READ | FAM_DIR);
		if (ec != 0)
		{
			return (ec);
		}
	}

	{
		fd_stats fdbuf;
		int size = sizeof(fdbuf);

		_os9_gs_fd(path, size, &fdbuf);

		*attr = fdbuf.fd_att;

		if (strattr != NULL)
		{
			OS9AttrToString(*attr, strattr);
		}
	}

	_os9_close(path);

	return (0);
}



error_code TSRBFAttrSet(char *file, int attrSetMask, int attrResetMask,
			char *attr, char *strattr)
{
	error_code ec = 0;
	os9_path_id path;

	/* open a path to the device */
	ec = _os9_open(&path, file, FAM_WRITE);

	if (ec != 0)
	{
		ec = _os9_open(&path, file, FAM_WRITE | FAM_DIR);
		if (ec != 0)
		{
			return (ec);
		}
	}

	{
		fd_stats fdbuf;
		int size = sizeof(fdbuf);

		ec = _os9_gs_fd(path, size, &fdbuf);

		if (attrSetMask != 0)
		{
			fdbuf.fd_att |= attrSetMask;
		}

		if (attrResetMask != 0)
		{
			fdbuf.fd_att &= ~attrResetMask;
		}

		ec = _os9_ss_fd(path, size, &fdbuf);

		*attr = fdbuf.fd_att;

		if (strattr != NULL)
		{
			OS9AttrToString(*attr, strattr);
		}
	}

	_os9_close(path);

	return (0);
}


error_code TSMoveFile(char *srcfile, char *dstfile)
{
	error_code ec = 0;
	char buff[512];

	/* determine if srcfile is a directory, delete the make */
	if (TSIsDirectory(srcfile) == 1)
	{
		TSMakeDirectory(dstfile);
		TSDelete(srcfile);
	}
	else if (TSCopyFile(srcfile, dstfile, 0, 1, 0, 0, buff, 512) == 0)
	{
		TSDelete(srcfile);
	}

	return ec;
}


error_code TSCopyFile(char *srcfile, char *dstfile, int eolTranslate,
		      int rewrite, int owner, int owner_set, char *buffer,
		      u_int buffer_size)
{
	error_code ec = 0;
	coco_path_id path;
	coco_path_id destpath;
	coco_file_stat fdesc;
	int mode = FAM_NOCREATE | FAM_WRITE;
	coco_file_stat fstat;


	/* 1. Set mode based on rewrite. */

	if (rewrite == 1)
	{
		mode &= ~FAM_NOCREATE;
	}


	/* 2. Open a path to the srcfile. */

	ec = _coco_open(&path, srcfile, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}


	/* 3. Attempt to create the destfile. */

	fstat.perms = FAP_PREAD | FAP_READ | FAP_WRITE;
	ec = _coco_create(&destpath, dstfile, mode, &fstat);

	if (ec != 0)
	{
		_coco_close(path);

		return ec;
	}


	while (_coco_gs_eof(path) == 0)
	{
		char *newBuffer;
		u_int newSize;
		u_int size = buffer_size;

		ec = _coco_read(path, buffer, &size);

		if (ec != 0)
		{
			break;
		}

		if (eolTranslate == 1)
		{
			if (path->type == NATIVE && destpath->type != NATIVE)
			{
				/* source is native, destination is OS-9 or DECB */

				NativeToCoCo(buffer, size, &newBuffer,
					     &newSize);

				ec = _coco_write(destpath, newBuffer,
						 &newSize);

				free(newBuffer);
				newBuffer = NULL;
			}
			else if (path->type != NATIVE
				 && destpath->type == NATIVE)
			{
				/* source is OS-9 or DECB, destination is native */

				CoCoToNative(buffer, size, &newBuffer,
					     &newSize);

				ec = _coco_write(destpath, newBuffer,
						 &newSize);

				free(newBuffer);
				newBuffer = NULL;
			}
		}
		else
		{
			/* One-to-one writing of the data -- no translation needed. */

			ec = _coco_write(destpath, buffer, &size);
		}

		if (ec != 0)
		{
			break;
		}
	}


	/* Copy meta data from file descriptor of source to destination */

	_coco_gs_fd(path, &fdesc);

	if ((owner_set == 1) || (path->type == NATIVE))
	{
		fdesc.user_id = owner % 65536;
		fdesc.group_id = owner / 65536;
	}

	_coco_close(path);

	/* Close the dest file before setting metadata. fclose() flushes
	 * buffered writes and causes the OS to update mtime, which would
	 * overwrite any utime() call made before it. Instead, close first,
	 * then reopen briefly just to apply the metadata (mtime, perms). */
	_coco_close(destpath);

	/* --- Intercept for Reproducible Builds --- */
	{
		char *sde_env = getenv("SOURCE_DATE_EPOCH");
		if (sde_env != NULL)
		{
			char *endptr;
			time_t sde_val = (time_t)strtoll(sde_env, &endptr, 10);
			if (*endptr == '\0' && sde_val >= 0)
			{
				fdesc.create_time = sde_val;
				fdesc.last_modified_time = sde_val;
			}
		}
	}

	{
		coco_path_id metapath;
		if (_coco_open(&metapath, dstfile, FAM_READ | FAM_WRITE) == 0)
		{
			_coco_ss_fd(metapath, &fdesc);
			_coco_close(metapath);
		}
	}


	return ec;
}


/*
 * Converts a buffer containing native EOLs to one with OS-9 EOLs.
 *
 * The caller must free the returned buffer in 'newBuffer' once
 * finished with the buffer.
 */
void NativeToCoCo(char *buffer, u_int size, char **newBuffer, u_int * newSize)
{
	EOL_Type eolMethod;
	int i;


	eolMethod = DetermineEOLType(buffer, size);

	switch (eolMethod)
	{
	case EOL_UNIX:
		/* Change all occurences of 0x0A to 0x0D */

		*newBuffer = (char *) malloc(size);
		if (*newBuffer == NULL)
		{
			return;
		}

		memcpy(*newBuffer, buffer, size);

		*newSize = size;

		for (i = 0; i < size; i++)
		{
			if ((*newBuffer)[i] == 0x0A)
			{
				(*newBuffer)[i] = 0x0D;
			}
		}

		break;

	case EOL_DOS:
		/* Things are a bit more involved here. */

		/* We will strip all 0x0As out of the buffer, leaving the 0x0Ds. */

		{
			u_int dosEOLCount = 0;
			char *newP;
			int i;


			/* 1. First we count up the number of 0x0A line endings. */

			for (i = 0; i < size; i++)
			{
				if (buffer[i] == 0x0A)
				{
					dosEOLCount++;
				}
			}


			/* 2. Now we allocate a buffer to hold the current size -
			   'dosEOLCount' bytes.
			 */

			*newSize = size - dosEOLCount;

			*newBuffer = (char *) malloc(*newSize);

			if (*newBuffer == NULL)
			{
				return;
			}

			newP = *newBuffer;

			for (i = 0; i < size; i++)
			{
				if (buffer[i] != 0x0A)
				{
					*newP = buffer[i];
					newP++;
				}
			}
		}
		break;

	default:
		/* No eols, binary copy */

		*newBuffer = (char *) malloc(size);
		if (*newBuffer == NULL)
		{
			return;
		}

		memcpy(*newBuffer, buffer, size);

		*newSize = size;

	}


	return;
}


void CoCoToNative(char *buffer, u_int size, char **newBuffer, u_int * newSize)
{
#ifdef WIN32
	u_int dosEOLCount = 0;
	char *newP;
	int i;


	/* Things are a bit more involved here. */

	/* We will add 0x0As after all 0x0Ds. */


	/* 1. First we count up the number of 0x0D OS-9 line endings. */

	for (i = 0; i < size; i++)
	{
		if (buffer[i] == 0x0D)
		{
			dosEOLCount++;
		}
	}


	/* 2. Now we allocate a buffer to hold the current size +
	   'dosEOLCount' bytes.
	 */

	*newSize = size + dosEOLCount;
	*newBuffer = (char *) malloc(*newSize);

	if (*newBuffer == NULL)
	{
		return;
	}

	newP = *newBuffer;

	for (i = 0; i < size; i++)
	{
		*newP = buffer[i];
		newP++;

		if (buffer[i] == 0x0D)
		{
			*newP = 0x0A;
			newP++;
		}
	}
#else
	int i;


	/* Change all occurences of 0x0D to 0x0A */

	*newBuffer = (char *) malloc(size);
	if (*newBuffer == NULL)
	{
		return;
	}

	memcpy(*newBuffer, buffer, size);

	*newSize = size;

	for (i = 0; i < size; i++)
	{
		if ((*newBuffer)[i] == 0x0D)
		{
			(*newBuffer)[i] = 0x0A;
		}
	}
#endif


	return;
}


/*
 * Scan a buffer to determine the type of end-of-line termination it has.
 *
 * Returns EOL_DOS, EOL_UNIX or EOL_os9
 */
EOL_Type DetermineEOLType(char *buffer, int size)
{
	EOL_Type eol = 0;
	int i;


	/* Scan to determine EOL ending type */

	for (i = 0; i < size; i++)
	{
		if (i < size - 1
		    && (buffer[i] == 0x0D && buffer[i + 1] == 0x0A))
		{
			/* We have DOS/Windows line endings (0D0A)... */
			eol = EOL_DOS;

			break;
		}

		/* Note we could have a buffer starting with NL (0x0A)
		 * even if it is a CR,NL (0x0D0A) file */
		if (buffer[i] == 0x0A)
		{
			/* We have unix line endings. */
			eol = EOL_UNIX;

			if (i > 0)
				break;
			else
				continue;
		}

		if (buffer[i] == 0x0D)
		{
			/* We have OS-9 line endings. */
			eol = EOL_OS9;

			break;
		}
	}


	return eol;
}


int TSMakeDirectory(char *p)
{
	error_code ec = 0;
	char *subPath;
	int i = 0, length = strlen(p);


	/* 1. Determine if there is an OS-9 pathlist. */

	if (strchr(p, ',') == NULL)
	{
		/* 1. Call the native file system makdir */
#ifndef WIN32
		ec = mkdir(p,
			   S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#else
		ec = _mkdir(p);
#endif
	}
	else
	{
		/* 1. Make a copy of the passed path into 'subPath'. */

		subPath = malloc(length + 1);

		strcpy(subPath, p);


		/* 2. Compute index to char past the native delimiter. */

		i = (strchr(p, ',') - p) + 1;


		/* BUG FIX: go past initial / if there */
		if (subPath[i] == '/')
		{
			i++;
		}

		/* 3. Walk path and create directory entries as we go */

		do
		{
			if (subPath[i] == 0)
			{
				ec = _os9_makdir(subPath);
				free(subPath);
				return ec;
			}

			else if (subPath[i] == '/')
			{
				subPath[i] = 0;
				ec = _os9_makdir(subPath);
				subPath[i] = '/';
			}

			i++;
		}
		while (i <= length);

		free(subPath);
	}


	return ec;
}



error_code TSDECBFree(char *pathlist, u_int * free_granules)
{
	error_code ec = 0;
	int i;
	char *decbpathlist;
	decb_path_id path;


	/* 1. Make a copy of the pathlist. */

	decbpathlist = malloc(strlen(pathlist) + 2);
	strcpy(decbpathlist, pathlist);


	/* 2. If a comma isn't present int he string, then add it so that the path is opened as a non-native path. */

	if (strchr(decbpathlist, ',') == NULL)
	{
		strcat(decbpathlist, ",");
	}


	/* 3. Open a path to the device. */

	ec = _decb_open(&path, decbpathlist, FAM_READ);

	free(decbpathlist);

	if (ec != 0)
	{
		return ec;
	}


	/* 4. Walk the FAT. */
	*free_granules = 0;
	for (i = 0; i < 256; i++)
	{
		if (i >= path->granule_count)
			break;

		if (path->FAT[i] == 0xFF)
		{
			(*free_granules)++;
		}
	}


	/* 6. Close the path. */

	_decb_close(path);


	/* 7. Return status. */

	return 0;
}


error_code TSRBFFree(char *file, char *dname, u_int * month, u_int * day,
		     u_int * year, u_int * bps, u_int * total_sectors,
		     u_int * bytes_free, u_int * free_sectors,
		     u_int * largest_free_block, u_int * sectors_per_cluster,
		     u_int * largest_count, u_int * sector_count)
{
	error_code ec = 0;
	int i;
	char os9pathlist[256];
	os9_path_id path;
	int bytes_in_bitmap;
	lsn0_sect sector0;
	u_int size;
	u_int total_clusters;
	u_int cluster_count = 0;
	u_int free_clusters = 0;

	*bytes_free = 0;
	*free_sectors = 0;
	*largest_free_block = 0;
	*sectors_per_cluster = 0;
	*total_sectors = 0;
	*largest_count = 0;
	*sector_count = 0;

	strcpy(os9pathlist, file);

	/* if the user forgot to add the ',', do it for them */
	if (strchr(os9pathlist, ',') == NULL)
	{
		strcat(os9pathlist, ",.");
	}

	strcat(os9pathlist, "@");

	/* open a path to the device */
	ec = _os9_open(&path, os9pathlist, FAM_READ);
	if (ec != 0)
	{
		return (ec);
	}

	*bps = path->bps;

	/* seek to the beginning of the disk */
	_os9_seek(path, 0, SEEK_SET);

	/* read LSN0 */
	size = sizeof(sector0);
	_os9_read(path, &sector0, &size);

	/* get the number of bytes in the bitmap and compute bitmap sectors */
	*sectors_per_cluster = int2(sector0.dd_bit);
	bytes_in_bitmap = int2(sector0.dd_map);
	*total_sectors = int3(sector0.dd_tot);
	total_clusters = *total_sectors / *sectors_per_cluster;

	/* walk bitmap bit by bit */
	for (i = 0; i < bytes_in_bitmap * 8; i++)
	{
		cluster_count++;
		if (cluster_count > total_clusters)
			break;

		if (_os9_ckbit(path->bitmap, i))
		{
			/* bit is set, cluster not free */
			if (*largest_count > *largest_free_block)
			{
				*largest_free_block = *largest_count;
				*largest_count = 0;
			}
		}
		else
		{
			/* bit is clear, sector is free */
			(*largest_count)++;
			free_clusters++;
		}
	}

	/* one last check on largest free block (here if last sector is free) */
	if (*largest_count > *largest_free_block)
	{
		*largest_free_block = *largest_count;
	}

	*sector_count = cluster_count * *sectors_per_cluster;
	*free_sectors = free_clusters * *sectors_per_cluster;
	*largest_free_block *= *sectors_per_cluster;

	*bytes_free = *free_sectors * *bps;

	strcpy(dname, (char *) sector0.dd_nam);
	OS9StringToCString((u_char *) dname);

	*month = sector0.dd_dat[1];
	*day = sector0.dd_dat[2];
	*year = sector0.dd_dat[0] + 1900;

	_os9_close(path);

	return (0);
}

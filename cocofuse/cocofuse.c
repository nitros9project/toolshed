/********************************************************************
 * cocofuse.c - FUSE compatible file system interface for RBF/Disk Basic
 *
 * $Id$
 ********************************************************************/
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <cocopath.h>

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION  26

#include <toolshed.h>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#endif

#ifdef DEBUG
#include <syslog.h>
#define LOG_LEVEL LOG_ALERT
#endif

#ifdef __APPLE__
#include <unistd.h>
#include <fuse_t/fuse.h>
#else
#include <fuse.h>
#endif

static int coco_access(const char *path, int mode);
static int coco_statfs(const char *path, struct statvfs *stbuf);
static int coco_fgetattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi);
static int coco_getattr(const char *path, struct stat *stbuf);
static int coco_chmod(const char *path, mode_t mode);
static int coco_truncate(const char *path, off_t size);
static int coco_open(const char *path, struct fuse_file_info *fi);

/* DSK image filename pointer */
static char *dsk = NULL;


/*
 *
 */
char *dsk_path(const char *path)
{
	char *buff = malloc(strlen(dsk) + strlen(path) + 2);
	strcpy(buff, dsk);
	strcat(buff, ",");
	strcat(buff, path);
	return buff;
}

/*
 *
 */
static int coco_access(const char *path, int mode)
{
	error_code ec = 0;

	struct stat stbuf;
	struct fuse_file_info fi;

	if ((ec = -CoCoToUnixError(coco_fgetattr(path, &stbuf, &fi))) == 0)
	{
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_access(%s, %d) = %d", path, mode, ec);
#endif
	return ec;
}

/*
 * coco_statfs - returns status of the file system
 */
static int coco_statfs(const char *path, struct statvfs *stbuf)
{
	_path_type type;
	char *buff;
	char dname[32];
	u_int month, day, year, bps, total_sectors, bytes_free, free_sectors;
	u_int largest_free_block, sectors_per_cluster, largest_count,
		sector_count;

	buff = dsk_path(path);
	_coco_identify_image(buff, &type);

	/* Here we revert to RBF or Disk BASIC to get details about the disk */
	switch (type)
	{
	case OS9:
		if (TSRBFFree
		    (buff, dname, &month, &day, &year, &bps, &total_sectors,
		     &bytes_free, &free_sectors, &largest_free_block,
		     &sectors_per_cluster, &largest_count,
		     &sector_count) == 0)
		{
			stbuf->f_bsize = bps;	/* file system block size */
			stbuf->f_frsize = stbuf->f_bsize;	/* fragment size */
			stbuf->f_blocks = total_sectors;	/* size of fs in f_frsize units */
			stbuf->f_bfree = free_sectors;	/* # free blocks */
			stbuf->f_bavail = free_sectors;	/* # free blocks for unprivileged users */
			stbuf->f_files = 1000;	/* # inodes */
			stbuf->f_ffree = 1000;	/* # free inodes */
			stbuf->f_favail = 1000;	/* # free inodes for unprivileged users */
			stbuf->f_fsid = 6809;	/* file system ID */
			stbuf->f_flag = 0;	/* mount flags */
			stbuf->f_namemax = 28;	/* maximum filename length */
		}
		break;

	case DECB:
		{
			/* TODO: Put values here that make sense */
			stbuf->f_bsize = 256;	/* file system block size */
			stbuf->f_frsize = stbuf->f_bsize;	/* fragment size */
			stbuf->f_blocks = 612;	/* size of fs in f_frsize units */
			stbuf->f_bfree = 612;	/* # free blocks */
			stbuf->f_bavail = 612;	/* # free blocks for unprivileged users */
			stbuf->f_files = 1000;	/* # inodes */
			stbuf->f_ffree = 1000;	/* # free inodes */
			stbuf->f_favail = 1000;	/* # free inodes for unprivileged users */
			stbuf->f_fsid = 6809;	/* file system ID */
			stbuf->f_flag = 0;	/* mount flags */
			stbuf->f_namemax = 11;	/* maximum filename length */
		}
		break;

	default:
		break;
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_statfs(%s) = %d", path, type);
#endif

	free(buff);
	return 0;
}

/*
 * coco_fgetattr - returns file attributes
 *
 * Notes: code in this routine coverts coco_file_stat values into
 * values appropriate for the struct stat native to FUSE.
 */
static int coco_fgetattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	error_code ec;
	char *buff;
	coco_path_id p;

	buff = dsk_path(path);

	/* open as file first */
	if ((ec = -CoCoToUnixError(_coco_open(&p, buff, FAM_READ))) != 0)
	{
		/* if failure, open as directory */
		if ((ec =
		     -CoCoToUnixError(_coco_open
				      (&p, buff, FAM_READ | FAM_DIR))) != 0)
		{
#ifdef DEBUG
			syslog(LOG_LEVEL, "coco_fgetattr(%s) = %d", path, ec);
#endif
			return ec;
		}

		memset(stbuf, 0, sizeof(struct stat));

		coco_file_stat fdbuf;

		/* Disk BASIC check -- strip off S_IFDIR from mode */
		if (p->type == DECB)
		{
			stbuf->st_mode &= ~S_IFDIR;
			stbuf->st_mode = S_IFREG;
		}

		if ((ec = -CoCoToUnixError(_coco_gs_fd(p, &fdbuf))) == 0)
		{
			uint32_t filesize;

			stbuf->st_mode |= CoCoToUnixPerms(fdbuf.attributes);

			stbuf->st_nlink = 1;

			if (_coco_gs_size(p, &filesize) != 0)
			{
				filesize = 0;
			}
			stbuf->st_size = int4((u_char *) & filesize);
#ifdef __linux__
			stbuf->st_ctime = fdbuf.create_time;
			stbuf->st_mtime = fdbuf.last_modified_time;
#else
			stbuf->st_ctimespec.tv_sec = fdbuf.create_time;
			stbuf->st_mtimespec.tv_sec = fdbuf.last_modified_time;
#endif
			stbuf->st_uid = getuid();
			stbuf->st_gid = getgid();
		}

		_coco_close(p);
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_fgetattr(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}

/*
 * coco_getattr - returns file attributes
 *
 * Notes: code in this routine coverts coco_file_stat values into
 * values appropriate for the struct stat native to FUSE.
 */
static int coco_getattr(const char *path, struct stat *stbuf)
{
	error_code ec = 0;
	coco_file_stat fdbuf;
	char *buff;

	memset(stbuf, 0, sizeof(struct stat));
	buff = dsk_path(path);

	if ((ec = -CoCoToUnixError(_coco_gs_fd_pathlist(buff, &fdbuf))) == 0)
	{
		u_int filesize;

		stbuf->st_mode |= CoCoToUnixPerms(fdbuf.attributes);

		stbuf->st_nlink = 1;

		if (_coco_gs_size_pathlist(buff, &filesize) == 0)
		{
			stbuf->st_size = filesize;
		}

#ifdef __linux__
		stbuf->st_ctime = fdbuf.create_time;
		stbuf->st_mtime = fdbuf.last_modified_time;
#else
		stbuf->st_ctimespec.tv_sec = fdbuf.create_time;
		stbuf->st_mtimespec.tv_sec = fdbuf.last_modified_time;
#endif
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_getattr(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


/*
 * coco_mkdir - make a directory (OS-9 only)
 */
static int coco_mkdir(const char *path, mode_t mode)
{
	error_code ec;
	char *buff;

	buff = dsk_path(path);
	ec = -CoCoToUnixError(_coco_makdir(buff));

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_makdir(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


/*
 * coco_unlink - removes the file specified in the path
 */
static int coco_unlink(const char *path)
{
	error_code ec;
	char *buff;

	buff = dsk_path(path);
	ec = -CoCoToUnixError(_coco_delete(buff));

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_unlink(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


/*
 * coco_rmdir - removes the directory specified in the path
 */
static int coco_rmdir(const char *path)
{
	error_code ec = 0;
	char *buff;

	buff = dsk_path(path);
	ec = -CoCoToUnixError(_coco_delete_directory(buff));	//, CoCoToUnixPerm(mode));
#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_rmdir(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


/*
 * coco_rename - renames a file on a path
 *
 * Note: both paths are full pathlists.  It is important to determine if
 * the rename is in the same directory.  If not, then the source file must
 * be deleted (i.e. an mv command is being performed).
 */
static int coco_rename(const char *path, const char *newname)
{
	error_code ec = 0;
	char *p1, *p2;
	char *buff;
	int renameonly = 0;

	/* 1. Determine if rename is in same dir.
	 *    - If so just rename.
	 *    - If not, rename then delete orginal.
	 */
	p1 = strrchr((char *)path, '/');
	p2 = strrchr((char *)newname, '/');

	if (p1 == NULL || p2 == NULL)
	{
		ec = -1;
#ifdef DEBUG
		syslog(LOG_LEVEL, "coco_rename(%s) = %d", path, ec);
#endif
		return ec;
	}

	*p1 = '\0';
	*p2 = '\0';

	if (strcmp(path, newname) == 0)
	{
		renameonly = 1;
	}

	if (renameonly == 1)
	{
		/* RENAME IN SAME FOLDER! */
		*p1 = '/';
		*p2 = '/';

		buff = dsk_path(path);
		ec = -CoCoToUnixError(_coco_rename(buff, p2 + 1));
		free(buff);
	}
	else
	{
		/* TODO: MOVE THEN DELETE! */
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_rename(%s) = %d", path, ec);
#endif

	return ec;
}


/*
 * coco_chmod - changes attributes of a file
 */
static int coco_chmod(const char *path, mode_t mode)
{
	error_code ec;
	char *buff;
	coco_path_id p;

	buff = dsk_path(path);
	if ((ec = -CoCoToUnixError(_coco_open(&p, buff, FAM_WRITE))) == 0)
	{
		ec = -CoCoToUnixError(_coco_ss_attr
				      (p, UnixToCoCoPerms(mode)));
		_coco_close(p);
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_chmod(%s, $%X) = %d", path, mode, ec);
#endif

	free(buff);
	return ec;
}


/*
 * coco_truncate - truncates a file to a specific size
 */
static int coco_truncate(const char *path, off_t size)
{
	error_code ec = 0;
	char *buff;
	coco_path_id p;

	buff = dsk_path(path);
	ec = -CoCoToUnixError(_coco_open(&p, buff, FAM_WRITE));
	if (ec == 0)
	{
		ec = -CoCoToUnixError(_coco_ss_size(p, size));
		_coco_close(p);
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_truncate(%s, %lld) = %d", path,
	       (long long) size, ec);
#endif

	free(buff);
	return ec;
}


static int coco_open(const char *path, struct fuse_file_info *fi)
{
	error_code ec;
	coco_path_id p;
	char *buff;
	int mflags = FAM_READ;

	buff = dsk_path(path);

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
	{
		mflags |= FAM_WRITE;
	}
	if ((ec = -CoCoToUnixError(_coco_open(&p, buff, mflags))) == 0)
	{
		fi->fh = (uint64_t) (intptr_t) p;
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_open(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


static int coco_read(const char *path, char *buf, size_t size, off_t offset,
		     struct fuse_file_info *fi)
{
	error_code ec;
	uint32_t _size = size;

	coco_path_id p = (coco_path_id) (intptr_t) fi->fh;
	_coco_seek(p, offset, SEEK_SET);
	if ((ec = -CoCoToUnixError(_coco_read(p, buf, &_size))) != 0)
	{
#ifdef DEBUG
		syslog(LOG_LEVEL, "coco_read(%s, %p, %lld) = %d", path, buf,
		       (long long) size, ec);
#endif
		return ec;
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_read(%s, %p, %lld) = %d", path, buf,
	       (long long) size, ec);
#endif

	return size;
}


static int coco_write(const char *path, const char *buf, size_t size,
		      off_t offset, struct fuse_file_info *fi)
{
	error_code ec;
	uint32_t _size = size;

	coco_path_id p = (coco_path_id) (intptr_t) fi->fh;
	_coco_seek(p, offset, SEEK_SET);
	if ((ec =
	     -CoCoToUnixError(_coco_write(p, (char *) buf, &_size))) != 0)
	{
#ifdef DEBUG
		syslog(LOG_LEVEL, "coco_write(%s, %p, %lld) = %d", path, buf,
		       (long long) size, ec);
#endif
		return ec;
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_write(%s, %p, %lld) = %d", path, buf,
	       (long long) size, ec);
#endif

	return size;
}


/*
 * coco_release - releases a previously opened path
 *
 * Notes: the path opened in coco_open is simply closed, which releases
 * internal file handles and memory.
 */
static int coco_release(const char *path, struct fuse_file_info *fi)
{
	error_code ec;

	ec = -CoCoToUnixError(_coco_close((coco_path_id) (intptr_t) fi->fh));

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_release(%s) = %d", path, ec);
#endif

	return ec;
}


static int coco_create(const char *path, mode_t perms,
		       struct fuse_file_info *fi)
{
	error_code ec = 0;
	coco_path_id p;
	char *buff;
	coco_file_stat fstat;

	int mflags = FAM_READ | FAM_WRITE;
	fstat.perms = FAP_READ | FAP_WRITE;

	buff = dsk_path(path);

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
	{
		fstat.perms |= FAM_WRITE;
	}

	if ((ec =
	     -CoCoToUnixError(_coco_create(&p, buff, mflags, &fstat))) == 0)
	{
		fi->fh = (uint64_t) (intptr_t) p;
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_create(%s, $%X) = %d", path, perms, ec);
#endif

	free(buff);
	return ec;
}


static int coco_opendir(const char *path, struct fuse_file_info *fi)
{
	error_code ec;
	coco_path_id p;
	char *buff;
	int mflags = FAM_READ;

	buff = dsk_path(path);

	mflags |= FAM_DIR;

	if ((fi->flags & O_ACCMODE) != O_RDONLY)
	{
		mflags |= FAM_WRITE;
	}
	if ((ec = -CoCoToUnixError(_coco_open(&p, buff, mflags))) == 0)
	{
		fi->fh = (uint64_t) (intptr_t) p;
	}

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_opendir(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


static int coco_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	error_code ec = 0;
	coco_path_id p;
	coco_dir_entry e;
	char *buff;

#ifndef USE_PATH
	buff = dsk_path(path);
	if (_coco_open(&p, buff, FAM_READ | FAM_DIR) != 0)
	{
		/* DECB doesn't use FAM_DIR */
		if (_coco_open(&p, buff, FAM_READ) != 0)
		{
			free(buff);
			return -ENOENT;
		}
	}
#else
	p = (coco_path_id) (intptr_t) fi->fh;
#endif

	while (_coco_readdir(p, &e) == 0)
	{
		switch (e.type)
		{
		case OS9:
			if (e.dentry.os9.name[0] != '\0')
			{
				/* entry is not empty, add it */
				filler(buf,
				       (char *) OS9StringToCString(e.dentry.
								   os9.name),
				       NULL, 0);
			}
			break;

		case DECB:
			if (e.dentry.decb.filename[0] != 0
			    && e.dentry.decb.filename[0] != 255)
			{
				u_char cstring[24];

				DECBStringToCString(e.dentry.decb.filename,
						    e.dentry.decb.
						    file_extension, cstring);
				filler(buf, (char *) cstring, NULL, 0);
			}
			break;

		default:
			break;
		}
	}

#if USE_PATH
	_coco_close(p);
#endif

#ifdef DEBUG
	syslog(LOG_LEVEL, "coco_readdir(%s) = %d", path, ec);
#endif

	free(buff);
	return ec;
}


static int coco_utimens(const char *path, const struct timespec *tv)
{
	return 0;
}

#ifndef COCOFUSE_MAC
static struct fuse_operations coco_filesystem_operations = {
	.statfs = coco_statfs,
	.access = coco_access,
	.truncate = coco_truncate,
	.getattr = coco_getattr,
	.mkdir = coco_mkdir,
	.unlink = coco_unlink,
	.rmdir = coco_rmdir,
	.rename = coco_rename,
	.chmod = coco_chmod,
	.readdir = coco_readdir,
	.open = coco_open,
	.read = coco_read,
	.write = coco_write,
	.release = coco_release,
	.create = coco_create,
	.opendir = coco_opendir,
	.releasedir = coco_release,
	.utimens = coco_utimens
};

void usage(const char *name)
{
	printf("cocofuse from Toolshed " TOOLSHED_VERSION "\n");
	printf("Usage: %s: dskimage mountpoint [FUSE options]\n", name);
}


char *make_absolute(const char *path)
{
	char *abs_path;

	if (path[0] == '/')
	{
		/* absolute path - use as-is */
		abs_path = strdup(path);
	}
	else
	{
		/* relative path */
		char *pwd = getcwd(NULL, 0);
		/* Allow one for terminating null and 1 for separator
		   slash */
		abs_path = malloc(strlen(pwd) + strlen(path) + 2);
		strcpy(abs_path, pwd);
		strcat(abs_path, "/");
		strcat(abs_path, path);
		free(pwd);
	}
	return abs_path;
}

int main(int argc, char **argv)
{
	int rc;

	if (argc < 3)
	{
		usage(argv[0]);
		return 1;
	}

	dsk = make_absolute(argv[1]);
	if (!dsk)
	{
		fprintf(stderr, "Disk image path too long\n");
		rc = 1;
	}
	else
	{
#ifdef DEBUG
		openlog("cocofuse", LOG_PID, LOG_DAEMON);
#endif
		argv[1] = argv[0];
		rc = fuse_main(argc - 1, &argv[1],
			       &coco_filesystem_operations, NULL);
#ifdef DEBUG
		closelog();
#endif
	}

	free(dsk);
	return rc;
}

#endif /* COCOFUSE_MAC */

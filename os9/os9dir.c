/********************************************************************
 * os9dir.c - Directory utility for OS-9
 *
 * $Id$
 ********************************************************************/
#include <ctype.h>
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <queue.h>


/* These are tweakable parameters which affect the output of a
 * non-extended directory listing.
 */
#define	DIR_COLWIDTH	16	/* width of a non-extended directory entry column */
#define	DIR_COLS		5	/* number of non-extended directory entries per line */
#define	OS9DIR_PATHMAX	1024	/* the longest pathlist listed; it bounds the depth of -r */

static int do_dir(char **argv, char *p, NodeType *q_visited);

/* globals */
static int extended = 0, dotfiles = 0, recurse = 0;

/* Help Message */
static char const *const helpMessage[] = {
	"Syntax: dir {[<opts>]} {<dir> [<...>]} {[<opts>]}\n",
	"Usage:  Display the contents of a directory.\n",
	"Options:\n",
	"     -a    show all files\n",
	"     -e    extended directory\n",
	"     -r    recurse directories\n",
	NULL
};


int os9dir(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	NodeType q_recursion = NULL;

	if (argv[1] == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'a':
					dotfiles = 1;
					break;

				case 'e':
					extended = 1;
					break;

				case 'r':
					recurse = 1;
					break;

				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (0);
				}
			}
		}
	}

	/* walk command line for pathnames */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			p = argv[i];
		}

		ec = do_dir(argv, p, &q_recursion);

		/* A directory met again under one argument is a loop or a
		   second link; under another argument it is listed afresh. */
		while (q_recursion != NULL)
		{
			qDeleteNode(&q_recursion, q_recursion);
		}

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);
			return (ec);
		}

		_coco_print_other_filesystems(p, COCO_FS_OS9);
	}

	return (0);
}


/* Print a filename, escaping what can't be printed, and say how wide it was. */
static int print_name(const char *name)
{
	int width = 0;

	for (; *name != '\0'; name++)
	{
		if (isprint((unsigned char) *name))
		{
			putchar(*name);
			width++;
		}
		else
		{
			width += printf("\\%o", (unsigned char) *name);
		}
	}

	return width;
}


/* Make a C string of a directory entry's name.  Nothing promises the last
   character has its top bit set, so the copy has room for an ending. */
static void entry_name(const os9_dir_entry *dentry, char name[D_NAMELEN + 1])
{
	memcpy(name, dentry->name, D_NAMELEN);
	name[D_NAMELEN] = '\0';
	OS9StringToCString((u_char *) name);
}


/* q_visited holds the file descriptor of every directory listed so far, so
 * one reached again, through a damaged entry or a second link to it, is
 * reported rather than listed over and over.  Entries that can't be read
 * don't stop the rest being listed; the last error is returned.
 */
static int do_dir(char **argv, char *p, NodeType *q_visited)
{
	error_code ec = 0;
	int col_count = 0;
	char os9pathlist[OS9DIR_PATHMAX];
	os9_path_id path;
	NodeType q_head = NULL;
	char filepath[OS9DIR_PATHMAX];
	error_code status = 0;
	size_t length;


	/* 1. Copy the passed pathlist into our own local buffer. */

	length = strlen(p);

	if (length == 0 || length + 2 >= sizeof(os9pathlist))
	{
		fprintf(stderr, "%s: bad pathname '%s'\n", argv[0], p);

		return (EOS_BPNAM);
	}

	memcpy(os9pathlist, p, length + 1);


	/* 2. If last char is ',', then add '.' */

	if (os9pathlist[length - 1] == ',')
	{
		strcat(os9pathlist, ".");
	}
	else if (strchr(os9pathlist, ',') == NULL)
	{
		/* 1. There is no ',' anywhere in the pathlist.  Add it. */

		strcat(os9pathlist, ",");
	}


	/* 3. Open a path to the device. */

	ec = _os9_open(&path, os9pathlist, FAM_READ | FAM_DIR);

	if (ec == EOS_FNA)
	{
		/* 1. This might be a file... if it is, see if we have ',.' at
		 *    end.  If not, add it and try once more to open the
		 *    file as a disk image.
		 */

		length = strlen(os9pathlist);

		if (!(length >= 2 && os9pathlist[length - 2] == ',' && os9pathlist[length - 1] == '.')
		    && length + 2 < sizeof(os9pathlist))
		{
			strcat(os9pathlist, ",.");
			ec = _os9_open(&path, os9pathlist, FAM_READ | FAM_DIR);
		}
	}

	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n", argv[0], ec,
			os9pathlist);

		return (ec);
	}

	if (qCheckDuplicateNode(*q_visited, &(path->pl_fd_lsn),
				sizeof(path->pl_fd_lsn)))
	{
		fprintf(stderr, "\n%s: already listed the directory at file descriptor sector %X: %s\n",
			argv[0], path->pl_fd_lsn, os9pathlist);
		_os9_close(path);

		return (EOS_WT);
	}

	if (qAddNode(q_visited, &(path->pl_fd_lsn), sizeof(path->pl_fd_lsn)) != 0)
	{
		_os9_close(path);

		return (EOS_MF);
	}

	printf("\n                           Directory of %s\n", os9pathlist);


	/* 5. If directory is extended, put out header. */

	if (extended == 1)
	{
		printf(" Owner    Last modified    Attributes Sector Bytecount Name\n");
		printf("-------   ---------------  ---------- ------ --------- ----\n");
	}

	/* _os9_readdir stops at the end of the directory or of its segment
	   list, whatever its file descriptor claims its size is. */

	while (_os9_gs_eof(path) == 0)
	{
		os9_dir_entry dentry;
		fd_stats fdbuf;
		char filename[D_NAMELEN + 1];
		int recurse_here = 0;

		memset(&dentry, 0, sizeof(dentry));

		ec = _os9_readdir(path, &dentry);

		if (ec != 0)
		{
			if (ec != EOS_EOF)
			{
				fprintf(stderr, "%s: error %d reading the directory '%s'\n",
					argv[0], ec, os9pathlist);
				status = ec;
			}
			break;
		}

		entry_name(&dentry, filename);

		if (filename[0] == '\0'
		    || (filename[0] == '.' && dotfiles == 0))
		{
			/* skip over deleted entries & dot files */
			continue;
		}

		if (extended == 1 || recurse == 1)
		{
			/* EXTENDED directory output */

			/* read the file desc, which needs no permission on the file */
			length = snprintf(filepath, sizeof(filepath), "%s/%s", os9pathlist, filename);

			ec = _os9_lsn_get(path, int3(dentry.lsn), 0, &fdbuf, sizeof(fd_stats));
			if (ec != 0)
			{
				fprintf(stderr, "%s: error %d reading the file descriptor of '%s'\n",
					argv[0], ec, filepath);
				status = ec;
				continue;
			}

			/* check directory attribute for recursion */
			recurse_here = recurse == 1 && (fdbuf.fd_att & (1 << 7)) != 0 &&
				/* we don't want to recurse on . and .. */
				strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0;

			if (recurse_here && length >= sizeof(filepath))
			{
				fprintf(stderr, "%s: pathname too long to list '%s'\n",
					argv[0], filepath);
				status = EOS_BPNAM;
				recurse_here = 0;
			}

			if (extended == 1)
			{
				/* print owner ID */
				printf("%3d.%-3d  ", fdbuf.fd_own[0],
				       fdbuf.fd_own[1]);

				/* print last modified date/time */
				printf(" %04d/%02d/%02d %02d%02d   ",
				       1900 + fdbuf.fd_dat[0],
				       fdbuf.fd_dat[1], fdbuf.fd_dat[2],
				       fdbuf.fd_dat[3], fdbuf.fd_dat[4]);

				/* print attributes */
				{
					char attrs[9];

					OS9AttrToString(fdbuf.fd_att, attrs);
					printf("%s", attrs);
				}

				/* print fd sector, file size */
				printf("%8X  %8u ",
				       int3(dentry.lsn),
				       int4(fdbuf.fd_siz));

				/* print escaped filename */
				print_name(filename);

				printf("\n");
			}
		}

		if (extended == 0)
		{
			/* REGULAR NON-EXTENDED directory output */
			int remainder;

			/* print filename */
			remainder = DIR_COLWIDTH - 1 - print_name(filename);

			if (++col_count == DIR_COLS)
			{
				printf("\n");
				col_count = 0;
			}
			else
			{
				do
				{
					printf(" ");
				}
				while (remainder-- > 0);
			}
		}

		if (recurse_here && qAddNode(&q_head, filepath, strlen(filepath) + 1) != 0)
		{
			status = EOS_MF;
		}
	}

	/* necessary to properly terminate a non-extended directory output listing */
	if (extended == 0 && col_count <= DIR_COLS)
	{
		printf("\n");
	}

	/* Only one directory is open at a time, however deep the tree. */
	_os9_close(path);

	{
		NodeType node = q_head;
		NodeType deadNode;

		while (node != NULL)
		{
			ec = do_dir(argv, (char *) node->data, q_visited);
			if (ec != 0)
			{
				status = ec;
			}
			deadNode = node;
			node = qGetNextNode(node);
			qDeleteNode(&q_head, deadNode);
		}
	}

	return status;
}

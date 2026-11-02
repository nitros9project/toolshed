/********************************************************************
 * os9dcheck.c - Disk file structure utility for OS-9
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <string.h>
#include <math.h>

static char *strcatdup(char *orig, char *cat1, char *cat2);
static error_code ParseFDSegList(fd_stats * fd, u_int dd_tot, char *path,
				 unsigned char *secondaryBitmap);
static error_code BuildSecondaryAllocationMap(os9_path_id os9_path,
					      int dir_lsn, char *path,
					      unsigned char *secondaryBitmap);
static error_code CompareAllocationMap(unsigned char *primaryAlloMap,
				       unsigned char *secondaryBitmap,
				       int dd_map, int cluster_size);
static void PathlistsForQuestionableClusters();
static void FreeQuestionableMemory();
static void AddQuestionableCluster(int cluster);
static void AddPathToBit(int lsn, char *path);
static int do_dcheck(char **argv, char *p);
static void PathlistsForQuestionableClusters(void);
static void FreeQuestionableMemory(void);


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dcheck {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Verify the file structure of a disk image.\n",
	"Options:\n",
	"     -s    check the number of directories and files and display\n",
	"            the results. This option causes dcheck to check only\n",
	"            the file descriptors for accuracy\n",
	"     -b    suppress listing of unused clusters (clusters allocated\n",
	"            but not in file structure)\n"
		"     -p    print pathlists of questionable clusters\n",
	NULL
};

int gFolderCount, gFileCount, gPreAllo,	/* Incremented when trying to allocated an already allocated cluster    */
  gFnotA,			/* Incremented when cluster is used and not in allocattion map                  */
  gAnotF,			/* Incremented when an allocated bit is not in the file structure               */
  gBadFD;			/* Incremented when a file descriptor has an out of range file segment  */

int sOption, bOption, pOption;	/* Flags for command line options */

typedef struct qCluster_t
{
	struct qCluster_t *next;
	int lsn;
} qCluster_t;

typedef struct qBitPath_t
{
	struct qBitPath_t *next;
	int lsn;
	char *path;
} qBitPath_t;

static qCluster_t *qCluster;	/* This is an array of clusters that are reported unusual */
static qBitPath_t *gBitPaths;	/* Every allocated bit has its path (sometimes more that one) */

int os9dcheck(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;

	sOption = bOption = pOption = 0;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 's':
					sOption = 1;
					break;
				case 'b':
					bOption = 1;
					break;
				case 'p':
					pOption = 1;
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

		ec = do_dcheck(argv, p);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);
			return (ec);
		}
	}

	if (argv[1] == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (0);
}



static int do_dcheck(char **argv, char *p)
{
	error_code ec = 0;
	os9_path_id os9_path;
	int cluster_size;
	unsigned char *secondaryBitmap;
	char *newName;
	char os9pathlist[256];
	double size;

	if (strchr(p, ',') != 0)
	{
		fprintf(stderr,
			"Cannot disk check an OS-9 file, only OS-9 disks.\n");
		return 1;
	}

	gFolderCount = 0;
	gFileCount = 0;
	gPreAllo = 0;
	gFnotA = gAnotF = 0;
	gBadFD = 0;

	strcpy(os9pathlist, p);

	/* if the user forgot to add the ',', do it for them */
	if (strchr(os9pathlist, ',') == NULL)
	{
		strcat(os9pathlist, ",.");
	}

	strcat(os9pathlist, "@");

	/* open a path to the device */
	ec = _os9_open(&os9_path, os9pathlist, FAM_READ);

	if (ec == 0 && os9_path->bitmap_ec != 0)
	{
		ec = os9_path->bitmap_ec;

		_os9_close(os9_path);
	}
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n", argv[0], ec,
			os9pathlist);
		return (ec);
	}

	OS9StringToCString(os9_path->lsn0->dd_nam);
	printf("Volume - '%s' in file: %s\n", os9_path->lsn0->dd_nam, p);
	printf("$%4.4X bytes in allocation map\n",
	       int2(os9_path->lsn0->dd_map));

	cluster_size = int2(os9_path->lsn0->dd_bit);

	if (cluster_size == 0)
	{
		printf("Disk format error: Sectors per cluster cannot be zero.\n");
		return -1;
	}

	if (cluster_size == 1)
		printf("%d sector per cluster\n", cluster_size);
	else
		printf("%d sectors per cluster\n", cluster_size);

	printf("$%6.6X total sectors on media\n",
	       int3(os9_path->lsn0->dd_tot));
	printf("Sector $%6.6X is start of root directory file descriptor\n",
	       int3(os9_path->lsn0->dd_dir));

/* Secondary Allocation map is expanded to assume a cluster size of one.
   This allows us to track wether a partial cluster is (incorrectly) allocated.
   It also makes it easier to determine if sectors are allocated multiple times.
*/
	secondaryBitmap =
		(unsigned char *) malloc((int3(os9_path->lsn0->dd_tot) + 7) /
					 8);

	if (secondaryBitmap == NULL)
	{
		printf("Failed to allocate memory for the secondary bitmap.\n");
		return -1;
	}

	memset(secondaryBitmap, 0, (int3(os9_path->lsn0->dd_tot) + 7) / 8);

	/* Allocate LSN0 in secondary bitmap */
	_os9_allbit(secondaryBitmap, 0, 1);

	/* Allocate primary bitmap sectors in secondary bitmap */

	size = (double) os9_path->bitmap_bytes / (double) os9_path->bps;

	_os9_allbit(secondaryBitmap, 1, ceil(size));

	/* Setup questionable cluster array */
	qCluster = NULL;
	gBitPaths = NULL;

	printf("Building secondary allocation map...\n");
	newName = strcatdup(p, ",.", "");
	BuildSecondaryAllocationMap(os9_path, int3(os9_path->lsn0->dd_dir),
				    newName, secondaryBitmap);

	printf("Comparing primary and secondary allocation maps...\n");
	CompareAllocationMap(os9_path->bitmap, secondaryBitmap,
			     int3(os9_path->lsn0->dd_tot), cluster_size);

	if (pOption == 1)
	{
		if (qCluster != NULL)
		{
			printf("\nPathlists for questionable clusters\n");
			PathlistsForQuestionableClusters();
		}
	}
	else
	{
		FreeQuestionableMemory();
	}

	free(secondaryBitmap);

	if (sOption == 0)
	{
		printf("\n%d previously allocated cluster found\n", gPreAllo);
		printf("%d clusters in file structure but not in allocation map\n", gFnotA);
		printf("%d clusters in allocation map but not in file structure\n", gAnotF);
		printf("%d bad file decriptor sector\n", gBadFD);

		if (gPreAllo > 0 || gFnotA > 0 || gBadFD > 0)
		{
			printf("\n'%s' file structure is NOT intact\n",
			       os9_path->lsn0->dd_nam);
		}
		else
		{
			printf("\n'%s' file structure is intact\n",
			       os9_path->lsn0->dd_nam);
		}
	}

	_os9_close(os9_path);

	if (gFolderCount == 1)
	{
		printf("1 directory\n");
	}
	else
	{
		printf("%d directories\n", gFolderCount);
	}

	printf("%d files\n", gFileCount);

	return (ec);
}


/* This function will drill down into a directory file and fillout a secondary allocation bitmap.
   It is recursive, so whenever a directory is encoundered it will call itself. It will also compare
   the map it creates with the map suppilied that represents the map on disk. */

static error_code BuildSecondaryAllocationMap(os9_path_id os9_path,
					      int dir_lsn, char *path,
					      unsigned char *secondaryBitmap)
{
	error_code ec = 0;
	fd_stats *dir_fd, *file_fd;
	u_int dd_tot, fd_siz, count, i, j, k;
	os9_dir_entry *dEnt;	/* Each entry is 32 bytes long */
	Fd_seg theSeg;
	char *newPath;
	int bps = os9_path->bps;

	/* Check if this directory has already been drilled into */

	if (_os9_ckbit(secondaryBitmap, dir_lsn) != 0)
	{
		/* Whoops, it is already allocated! */
		printf("Directory %s has a circular reference. Skipping\n",
		       path);
		AddQuestionableCluster(dir_lsn);
		return (0);
	}

	/* Allocate directory file descriptor LSN in secondary allocation map */

	_os9_allbit(secondaryBitmap, dir_lsn, 1);
	gFolderCount++;

	dd_tot = int3(os9_path->lsn0->dd_tot);

	dir_fd = (fd_stats *) malloc(bps);
	if (dir_fd == NULL)
	{
		printf("Out of memory, terminating (001).\n");
		exit(-1);
	}

	if (read_lsn(os9_path, dir_lsn, dir_fd) != bps)
	{
		printf("Sector wrong size, terminating (001).\n");
		printf("LSN: %d\n", dir_lsn);
		exit(-1);
	}

	/* Parse segment list of directory, report any problems */
	ec = ParseFDSegList(dir_fd, dd_tot, path, secondaryBitmap);

	if (ec != 0)
	{
		printf("File descriptor for directory %s is bad. Will not open directory file.\n", path);
		ec = 0;
	}
	else
	{
		/* Now open directory file and parse contents */
		fd_siz = int4(dir_fd->fd_siz);
		count = 0;
		i = 0;

		while (i < NUM_SEGS && int3(dir_fd->fd_seg[i].lsn) != 0)
		{
			theSeg = &(dir_fd->fd_seg[i]);

			if (count > fd_siz)
			{
				break;
			}

			if (int2(theSeg->num) > dd_tot)
			{
				printf("File: %s contains a bad segment (%d > %d)\n", path, int2(theSeg->num), dd_tot);
				gBadFD++;
				break;
			}

			for (j = 0; j < int2(theSeg->num); j++)
			{
				if (count > fd_siz)
					break;

				if (int3(theSeg->lsn) + j > dd_tot)
				{
					printf("File: %s, contains bad LSN (%d > %d)\n", path, int3(theSeg->lsn) + j, dd_tot);
					count += 256;
					gBadFD++;
					break;
				}

				dEnt = (os9_dir_entry *) malloc(bps);
				if (dEnt == NULL)
				{
					printf("Out of memory, terminating (002).\n");
					exit(-1);
				}

				if (read_lsn
				    (os9_path, int3(theSeg->lsn) + j,
				     dEnt) != bps)
				{
					int temp = int3(theSeg->lsn) + j;

					printf("Sector wrong size, terminating (002).\nLSN: %d\n", temp);
					exit(-1);
				}

				for (k = 0; k < (bps / sizeof(os9_dir_entry));
				     k++)
				{
					count += sizeof(os9_dir_entry);
					if (count > fd_siz)
					{
						break;
					}

					if (dEnt[k].name[0] == 0)
					{
						continue;
					}

					OS9StringToCString(dEnt[k].name);

					if (strcmp((char *) dEnt[k].name, ".")
					    == 0)
					{
						continue;
					}
					if (strcmp
					    ((char *) dEnt[k].name,
					     "..") == 0)
					{
						continue;
					}

					newPath =
						strcatdup(path, "/",
							  (char *) dEnt[k].
							  name);

					if (int3(dEnt[k].lsn) > dd_tot)
					{
						printf("File: %s, contains bad LSN\n", newPath);
						free(newPath);
						continue;
					}

					file_fd = (fd_stats *) malloc(bps);
					if (file_fd == NULL)
					{
						printf("Out of memory, terminating (003).\n");
						exit(-1);
					}

					if (read_lsn
					    (os9_path, int3(dEnt[k].lsn),
					     file_fd) != bps)
					{
						printf("Sector wrong size, terminating (003).\n");
						printf("LSN: %d\n",
						       int3(dEnt[k].lsn));
						exit(-1);
					}

					/* If actually a directory? */
					if ((file_fd->fd_att & FAP_DIR) == 0)
					{
						/* No, file is a file */
						_os9_allbit(secondaryBitmap,
							    int3(dEnt[k].lsn),
							    1);

						gFileCount++;
						ParseFDSegList(file_fd,
							       dd_tot,
							       newPath,
							       secondaryBitmap);
					}
					else
					{
						/* Yes, go do directory */
						ec = BuildSecondaryAllocationMap(os9_path, int3(dEnt[k].lsn), newPath, secondaryBitmap);
						if (ec != 0)
						{
							return (ec);
						}
					}

					free(file_fd);
					free(newPath);
				}

				free(dEnt);
			}

			i++;
		}
	}

	free(dir_fd);

	return (ec);
}

static error_code ParseFDSegList(fd_stats * fd, u_int dd_tot, char *path,
				 unsigned char *secondaryBitmap)
{
	error_code ec = 0;
	u_int i = 0, j, once;
	Fd_seg theSeg;
	u_int num, curLSN;

	while (i < NUM_SEGS && int3(fd->fd_seg[i].lsn) != 0)
	{
		u_int mapSector;

		theSeg = &(fd->fd_seg[i]);
		num = int2(theSeg->num);

		mapSector = int3(theSeg->lsn) / 2048;

		if ((int3(theSeg->lsn) + num) > dd_tot)
		{
			printf("*** Bad FD segment ($%6.6X-$%6.6X) for file: %s (Segement index: %d)\n", int3(theSeg->lsn), int3(theSeg->lsn) + num, path, i);
			gBadFD++;
			i++;
			continue;
		}

		for (j = 0; j < num; j++)
		{
			once = 0;
			curLSN = int3(theSeg->lsn) + j;

			if (curLSN / 2048 != mapSector)
			{
				printf("*** FD segment %u of path %s uses multiple allocation map sectors\n", i, path);
				mapSector = curLSN / 2048;
			}

			/* check for segment elements out of bounds */
			if (curLSN > dd_tot)
			{
				if (once == 0)
				{
					printf("*** Bad FD segment ($%6.6X-$%6.6X) for file: %s (Segement index: %d)\n", int3(theSeg->lsn), int3(theSeg->lsn) + num, path, i);
					gBadFD++;
					once = 1;
					ec = 1;
				}
			}
			else
			{
				/* Record path to this bit */
				AddPathToBit(curLSN, path);

				/* Check if bit is already allocated */
				if (_os9_ckbit(secondaryBitmap, curLSN) != 0)
				{
					/* Whoops, it is already allocated! */
					printf("Sector $%6.6X was previously allocated\n", curLSN);
					AddQuestionableCluster(curLSN);
					gPreAllo++;
				}
				else
				{
					/* Allocate bit and move on */
					_os9_allbit(secondaryBitmap, curLSN,
						    1);
				}
			}
		}

		i++;
	}

	return ec;
}

static error_code CompareAllocationMap(unsigned char *primaryAlloMap,
				       unsigned char *secondaryBitmap,
				       int dd_map, int cluster_size)
{
	error_code ec = 0;
	int i, j, LSN;

	for (i = 0; i < (dd_map / cluster_size); i++)
	{
		int p, s;

		p = _os9_ckbit(primaryAlloMap, i);

		for (j = 0; j < cluster_size; j++)
		{
			LSN = i * cluster_size + j;

			s = _os9_ckbit(secondaryBitmap, LSN);

			if (p != s)
			{
				if (p == 0)
				{
					printf("Logical sector %d ($%6.6X) of cluster %d ($%6.6X) in file structure but not in allocation map\n", LSN, LSN, i, i);
					AddQuestionableCluster(i);
					gFnotA++;
				}

				if (s == 0)
				{
					if (bOption == 0)
						printf("Logical sector %d ($%6.6X) of cluster %d ($%6.6X) in allocation map but not in file structure\n", LSN, LSN, i, i);

					gAnotF++;
				}
			}
		}
	}

	return (ec);
}

static void PathlistsForQuestionableClusters(void)
{
	qCluster_t *tmp;
	qBitPath_t *bitpath, *tmpBP;

	while (qCluster != NULL)
	{
		bitpath = gBitPaths;

		while (bitpath != NULL)
		{
			if (qCluster->lsn == bitpath->lsn)
				printf("Cluster $%6.6X in path: %s\n",
				       qCluster->lsn, bitpath->path);

			bitpath = bitpath->next;
		}

		tmp = qCluster->next;
		free(qCluster);
		qCluster = tmp;
	}

	bitpath = gBitPaths;

	while (bitpath != NULL)
	{
		free(bitpath->path);
		tmpBP = bitpath->next;
		free(bitpath);
		bitpath = tmpBP;
	}
}

static void FreeQuestionableMemory(void)
{
	qCluster_t *tmp;
	qBitPath_t *tmpBP;

	while (qCluster != NULL)
	{
		tmp = qCluster->next;
		free(qCluster);
		qCluster = tmp;
	}

	while (gBitPaths != NULL)
	{
		free(gBitPaths->path);
		tmpBP = gBitPaths->next;
		free(gBitPaths);
		gBitPaths = tmpBP;
	}
}

static char *strcatdup(char *orig, char *cat1, char *cat2)
{
	char *result;

	if (cat2 == NULL)
		result = (char *) malloc(strlen(orig) + strlen(cat1) + 1);
	else
		result = (char *) malloc(strlen(orig) + strlen(cat1) +
					 strlen(cat2) + 1);

	if (result != NULL)
	{
		strcpy(result, orig);
		strcat(result, cat1);
		if (cat2 != NULL)
			strcat(result, cat2);
	}

	return result;
}

static void AddQuestionableCluster(int cluster)
{
	qCluster_t *curCluster;

	curCluster = (qCluster_t *) malloc(sizeof(qCluster_t));
	if (curCluster == NULL)
		return;

	curCluster->lsn = cluster;
	curCluster->next = qCluster;
	qCluster = curCluster;
}

static void AddPathToBit(int lsn, char *path)
{
	qBitPath_t *bitPath;

	bitPath = (qBitPath_t *) malloc(sizeof(qBitPath_t));
	if (bitPath == NULL)
		return;

	bitPath->lsn = lsn;
	bitPath->path = (char *) strdup(path);
	bitPath->next = gBitPaths;
	gBitPaths = bitPath;
}

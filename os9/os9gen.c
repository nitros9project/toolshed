/********************************************************************
 * os9gen.c - Link a bootfile and copy boot track to track 34
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cocopath.h>


struct personality
{
	int startlsn;
};

static int do_os9gen(char **argv, char *device, char *bootfile,
		     char *trackfile, struct personality *hwtype,
		     int extended);

static struct personality coco = { 18 * 34 };
static struct personality dragon = { 2 };

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: gen {[<opts>]} {<disk_image>}\n",
	"Usage:  Prepare the disk image for booting.\n",
	"Options:\n",
	"     -b=bootfile     bootfile to copy and link to the image\n",
	"     -c              CoCo disk (default)\n",
	"     -d              Dragon disk\n",
	"     -e              Extended boot (fragmented)\n",
	"     -t=trackfile    kernel trackfile to copy to the image\n",
	"     -lX             Special boottrack/kerneltrack Start LSN\n",
	NULL
};

int specialStartLSN = 0;

int os9gen(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	char *device = NULL, *bootfile = NULL, *trackfile = NULL;
	int i;
	struct personality *hwtype = &coco;
	int extended = 0;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);
				case 'b':
					if (*(++p) == '=')
					{
						p++;
					}
					bootfile = p;
					p += strlen(p) - 1;
					break;
				case 'c':
					hwtype = &coco;
					break;
				case 'd':
					hwtype = &dragon;
					break;
				case 'e':
					extended = 1;
					break;
				case 't':
					if (*(++p) == '=')
					{
						p++;
					}
					trackfile = p;
					p += strlen(p) - 1;
					break;
				case 'l':	/* Special startLSN for boottrack/kerneltrack */
					specialStartLSN = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;
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

			if (device == NULL)
			{
				device = p;
			}
		}
	}

	if (device == NULL || (bootfile == NULL && trackfile == NULL))
	{
		show_help(helpMessage);
		return (0);
	}

	ec = do_os9gen(argv, device, bootfile, trackfile, hwtype, extended);

	if (ec != 0)
	{
		fprintf(stderr, "Error %d\n", ec);
	}

	return (ec);
}



static int do_os9gen(char **argv, char *device, char *bootfile,
		     char *trackfile, struct personality *hwtype,
		     int extended)
{
	error_code ec = 0;
	os9_path_id opath;
	coco_path_id cpath;
	int bootfile_LSN = 0, bootfile_Size = 0;
	char buffer[256];
	lsn0_sect LSN0;
	u_int size;
	u_int sectors;
	u_int sectorSize;
	u_int clusterSize;


	/* 1. If we have a boot track file, put it on the disk first */

	if (trackfile != NULL)
	{
		int startlsn;
		error_code ec;
		char boottrack[256 * 18];


		/* 1. Open the device raw and read LSB0 */

		sprintf(buffer, "%s,@", device);

		ec = _os9_open(&opath, buffer, FAM_READ | FAM_WRITE);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, buffer);
			return 1;
		}

		sectorSize = opath->bps;

		size = sizeof(lsn0_sect);

		_os9_read(opath, &LSN0, &size);

		clusterSize = int2(LSN0.dd_bit);

		/*  Diagnostic output */

		/* printf("Sectors per track: %d, Heads: %d, Disk Type: %d, Total Sectors: %d \n", int2(LSN0.pd_sct), int1(LSN0.pd_sid), int1(LSN0.pd_typ), int3(LSN0.dd_tot)); */

		/* 2. Determine startlsn based on single or double-sided device */

		startlsn = hwtype->startlsn;

		/* printf("hardware type: %d\n", startlsn); */

		if (startlsn == 2)
		{
			printf("Dragon boottrack selected: ");
			/* Check to make sure the disk image has minimum of 18 sectors per track */
			if (int2(LSN0.pd_sct) < 18)
			{
				printf("\n");
				fprintf(stderr,
					"Error: minimum sectors per track of 18 required for DragonDOS, found %d\n",
					int2(LSN0.pd_sct));
				_os9_close(opath);
				return (1);
			}
		}
		else
		{
			printf("CoCo boottrack selected: ");
			/* If special startLSN for boottrack is set then set startlsn to  */
			/* the value stored in specialStartLSN  */
			if (specialStartLSN > 0)
			{
				startlsn = specialStartLSN;
			}
			else
			{
				/* Check to see if disk image is a HDD image if so set for default  */
				/* startLSN of 612 for the boottrack for use with CoCoSDC and DriveWire HDD images */
				if (int1(LSN0.pd_typ) == 0x80)
				{
					startlsn = 612;
				}
				else
				{
					/* Check to make sure the disk image has minimum of 18 sectors per track */
					if (int2(LSN0.pd_sct) < 18)
					{
						printf("\n");
						fprintf(stderr,
							"Error: minimum sectors per track of 18 required for Disk Basic, found %d\n",
							int2(LSN0.pd_sct));
						_os9_close(opath);
						return (1);
					}
					/* Check to make sure the disk image has minimum of 35 tracks */
					if (int2(LSN0.pd_cyl) < 35)
					{
						printf("\n");
						fprintf(stderr,
							"Error: minimum number of tracks required for Disk Basic is 35, found %d\n",
							int2(LSN0.pd_cyl));
						_os9_close(opath);
						return (1);
					}
					/* Use real floppy disk geometry to figure out real startLSN for boottrack */
					startlsn =
						34 * int2(LSN0.pd_sct) *
						int1(LSN0.pd_sid);
				}
			}
		}
		if ((startlsn + 18) > int3(LSN0.dd_tot))
		{
			printf("\n");
			fprintf(stderr,
				"Error: start LSN puts boottrack outside of OS-9 volume boundery\n");
			_os9_close(opath);
			return (1);
		}

		/* 3. Open the track file */

		ec = _coco_open(&cpath, trackfile, FAM_READ);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, trackfile);
			_os9_close(opath);
			return (1);
		}


		/* 4. Read the track file. */

		size = 256 * 18;
		_coco_read(cpath, boottrack, &size);

		_coco_close(cpath);

		/* 5. Seek to appropriate track and write out track data. */

		_os9_seek(opath, startlsn * 256, SEEK_SET);

		_os9_write(opath, boottrack, &size);


		/* TODO: Deallocate appropriate bits in bitmap sector */
		/* Is not necesary on Dragon, but wouldn't harm */

		sectors = (size + sectorSize - 1) / sectorSize;
		_os9_allbit(opath->bitmap,
			    (startlsn + clusterSize - 1) / clusterSize,
			    (sectors + clusterSize - 1) / clusterSize);

		printf("Boot track written!  LSN: %d, size: %d\n", startlsn,
		       size);

		_os9_close(opath);
	}


	/* 2. Now put OS9Boot on disk */

	if (bootfile != NULL)
	{
		fd_stats fdbuf;
		char *bootfileMem;
		size = sizeof(fdbuf);

		/* 1. Open a path to the bootfile */

		ec = _coco_open(&cpath, bootfile, FAM_READ);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, bootfile);

			return (1);
		}

		/* 2.2. Allocate buffer memory and read bootfile */
		bootfileMem = (char *) malloc(65536);	/* Allocate maximum */
		if (bootfileMem == NULL)
		{
			_coco_close(cpath);
			fprintf(stderr, "Failed to allocate memory\n");
			return (-1);
		}

		size = 65536;
		_coco_read(cpath, bootfileMem, &size);

		_coco_close(cpath);	/* We're done with the path now */

		/* 2.3. Create a file called 'OS9Boot' in the root dir of device */
		sprintf(buffer, "%s,OS9Boot", device);

		ec = _os9_create(&opath, buffer, FAM_WRITE,
				 FAM_READ | FAM_WRITE);
		if (ec != 0)
		{
			return (ec);
		}

		/* 2.4. Write out bootfile to path */
		_os9_write(opath, bootfileMem, &size);
		free(bootfileMem);

		_os9_gs_fd(opath, sizeof(fdbuf), &fdbuf);

		bootfile_LSN = int3(fdbuf.fd_seg[0].lsn);
		bootfile_Size = int4(fdbuf.fd_siz);

		if (int3(fdbuf.fd_seg[1].lsn) != 0 && extended == 0)
		{
			printf("Error: %s is fragmented\n", bootfile);
			_os9_close(opath);
			return (1);
		}

		_os9_close(opath);

		/* Link the passed bootfile to LSN0 */

		/* 3. Open a path to the device raw */
		sprintf(buffer, "%s,@", device);

		ec = _os9_open(&opath, buffer, FAM_READ | FAM_WRITE);
		if (ec != 0)
		{
			return (ec);
		}

		size = sizeof(lsn0_sect);
		_os9_read(opath, &LSN0, &size);

		if (size != sizeof(lsn0_sect))
		{
			_os9_close(opath);
			printf("Error reading LSN0\n");
			return (1);
		}

		if (extended == 0)
		{
			_int3(bootfile_LSN, LSN0.dd_bt);
			_int2(bootfile_Size, LSN0.dd_bsz);
		}
		else
		{
			bootfile_LSN--;
			_int3(bootfile_LSN, LSN0.dd_bt);
			_int2(0, LSN0.dd_bsz);
		}
		_os9_seek(opath, 0, SEEK_SET);
		_os9_write(opath, &LSN0, &size);
		_os9_close(opath);

		printf("Bootfile Linked!  LSN: %d, size: %d\n", bootfile_LSN,
		       bootfile_Size);
	}

	return (0);
}

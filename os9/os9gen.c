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
#include <toolshed.h>

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

/*
 * OS-9/6809 bootstraps (and some OS-9/68000 bootstraps) require OS9Boot to be
 * contiguous so it can be loaded as a single linear stream from the start LSN.
 * Later OS-9/68000 systems can instead boot via the OS9Boot file descriptor and
 * follow its segment list.
 *
 * In full generality, contiguity would mean that each segment starts exactly
 * where the previous segment ends. ToolShed’s intended usage mirrors the OS-9
 * docs: generate the bootfile on a freshly formatted disk/image. Given that
 * OS9Boot is small (well under the per-segment maximum), a practical and
 * sufficient check here is that OS9Boot uses only one segment.
 */

#define is_single_segment(fd) (int3((fd).fd_seg[1].lsn) == 0)

static int do_os9gen(char **argv, char *device, char *bootfile,
                     char *trackfile, struct personality *hwtype,
                     int extended)
{
    error_code ec = 0;
    coco_path_id opath = NULL, cpath = NULL;
    char buffer[256];
    lsn0_sect LSN0;
    u_int size;
    u_int sectors;

    /* 1. If a boot track file was provided, process and write it first */

    if (trackfile != NULL)
    {
        /* Safe buffer size for (256 * 18) + 1 test byte */
        char boottrack[(256 * 18) + 1];
        u_int boottrack_len = 0;
        _path_type track_type = NATIVE;
		int is_osk;
		u_char *pd_sct, *pd_cyl, *pd_sid, *pd_typ;
		int startlsn;

        _coco_identify_image(trackfile, &track_type);

        if (track_type == OS9 && TSIsDirectory(trackfile))
        {
            /* Trackfile is an OS-9 disk image container — extract Track 34 */
            snprintf(buffer, sizeof(buffer), "%s,@", trackfile);
            ec = _coco_open(&cpath, buffer, FAM_READ);
            if (ec != 0)
            {
                fprintf(stderr, "%s: error %d opening track image '%s'\n", argv[0], ec, buffer);
                return (1);
            }

            /* Read LSN0 to determine image geometry */
            lsn0_sect track_LSN0;
            size = sizeof(lsn0_sect);
            ec = _coco_read(cpath, &track_LSN0, &size);
            if (ec != 0) return ec;

            u_int total_sectors = int3(track_LSN0.dd_tot);
            int t34_sct = int2(track_LSN0.dd_opt.m6809.pd_sct);
            int t34_sid = int1(track_LSN0.dd_opt.m6809.pd_sid);

            /* Fallback to default CoCo 18 sectors/track if uninitialized */
            if (t34_sct == 0) t34_sct = 18;

            /* If pd_sid is uninitialized, deduce side count from total sectors */
            if (t34_sid == 0)
            {
                /* 35 trk * 18 sct = 630 (1-sided), 1260 (2-sided) */
                /* 40 trk * 18 sct = 720 (1-sided), 1440 (2-sided) */
                /* 80 trk * 18 sct = 1440 (1-sided), 2880 (2-sided) */
                t34_sid = (total_sectors > 720 && (total_sectors % (t34_sct * 2) == 0)) ? 2 : 1;
            }

            /* Calculate exact LSN for Track 34 Side 0 */
            int sectors_per_cylinder = t34_sct * t34_sid;
            int embed_lsn = (hwtype->startlsn == 2) ? 2 : (34 * sectors_per_cylinder);

            /* Seek to Track 34 and verify header */
            ec = _coco_seek(cpath, embed_lsn * 256, SEEK_SET);
            if (ec != 0) return ec;
            size = 2;
            char track_hdr[2];
            ec = _coco_read(cpath, track_hdr, &size);

            if (ec == 0 && track_hdr[0] == 'O' && track_hdr[1] == 'S')
            {
                printf("Using embedded kernel trackfile code found at LSN %d of image '%s'\n", embed_lsn, trackfile);
                _coco_seek(cpath, embed_lsn * 256, SEEK_SET);
                boottrack_len = 256 * 18;
                _coco_read(cpath, boottrack, &boottrack_len);
            }
            else
            {
                fprintf(stderr, "%s: image '%s' does not contain valid OS kernel trackfile code (0x%02x%02x) at LSN %d\n",
                	argv[0], trackfile, track_hdr[0], track_hdr[1], embed_lsn);
                _coco_close(cpath);
                return (1);
            }
            _coco_close(cpath);
        }
        else
        {
            /* Standalone trackfile — open and read up to 4609 bytes to check size */
            ec = _coco_open(&cpath, trackfile, FAM_READ);
            if (ec != 0)
            {
                fprintf(stderr, "%s: error %d opening kernel trackfile '%s'\n", argv[0], ec, trackfile);
                return (1);
            }

            boottrack_len = (256 * 18) + 1;
            ec = _coco_read(cpath, boottrack, &boottrack_len);

            if (boottrack_len > (256 * 18))
            {
                fprintf(stderr, "%s: kernel trackfile '%s' exceeds 18 sectors (4608 bytes)\n", argv[0], trackfile);
                _coco_close(cpath);
                return (1);
            }

            if (ec != 0 && ec != EOS_EOF)
            {
                fprintf(stderr, "%s: error %d reading kernel trackfile '%s'\n", argv[0], ec, trackfile);
                _coco_close(cpath);
                return (1);
            }

            _coco_close(cpath);
        }

        /* Open target device raw to write the kernel trackfile */
        snprintf(buffer, sizeof(buffer), "%s,@", device);
        ec = _coco_open(&opath, buffer, FAM_READ | FAM_WRITE);
        if (ec != 0)
        {
            fprintf(stderr, "%s: error %d opening target device '%s'\n", argv[0], ec, buffer);
            return (1);
        }

        /* Read target LSN0 to get cluster size and sector size */
        size = sizeof(lsn0_sect);
        _coco_read(opath, &LSN0, &size);

		is_osk = (memcmp(LSN0.dd_sync, "Cruz", 4) == 0);

		if (is_osk != 0)
		{
			pd_sct = LSN0.dd_opt.m68k.pd_sct;
			pd_cyl = LSN0.dd_opt.m68k.pd_cyl;
			pd_sid = LSN0.dd_opt.m68k.pd_sid;
			pd_typ = LSN0.dd_opt.m68k.pd_typ;
		}
		else
		{
			pd_sct = LSN0.dd_opt.m6809.pd_sct;
			pd_cyl = LSN0.dd_opt.m6809.pd_cyl;
			pd_sid = LSN0.dd_opt.m6809.pd_sid;
			pd_typ = LSN0.dd_opt.m6809.pd_typ;
		}

		startlsn = hwtype->startlsn;

		if (startlsn == 2)
		{
			printf("Dragon boottrack selected: ");
			/* Check to make sure the disk image has minimum of 18 sectors per track */
			if (int2(pd_sct) < 18)
			{
				printf("\n");
				fprintf(stderr,
					"Error: minimum sectors per track of 18 required for DragonDOS, found %d\n",
					int2(pd_sct));
				_coco_close(opath);
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
				if (int1(pd_typ) == 0x80)
				{
					startlsn = 612;
				}
				else
				{
					/* Check to make sure the disk image has minimum of 18 sectors per track */
					if (int2(pd_sct) < 18)
					{
						printf("\n");
						fprintf(stderr,
							"Error: minimum sectors per track of 18 required for Disk Basic, found %d\n",
							int2(pd_sct));
						_coco_close(opath);
						return (1);
					}
					/* Check to make sure the disk image has minimum of 35 tracks */
					if (int2(pd_cyl) < 35)
					{
						printf("\n");
						fprintf(stderr,
							"Error: minimum number of tracks required for Disk Basic is 35, found %d\n",
							int2(pd_cyl));
						_coco_close(opath);
						return (1);
					}
					/* Use real floppy disk geometry to figure out real startLSN for boottrack */
					startlsn =
						34 * int2(pd_sct) *
						int1(pd_sid);
				}
			}
		}
		if ((startlsn + 18) > int3(LSN0.dd_tot))
		{
			printf("\n");
			fprintf(stderr,
				"Error: start LSN puts boottrack outside of OS-9 volume boundery\n");
			_coco_close(opath);
			return (1);
		}

		u_int sectorSize = opath->path.os9->bps;
		u_int clusterSize = int2(LSN0.dd_bit);

        /* Write boot track data out to target using actual read length */
        fprintf(stderr, "calculated seek: %d\n", startlsn * 256 );
        _coco_seek(opath, startlsn * 256, SEEK_SET);
        size = boottrack_len;
        fprintf(stderr, "ftell: %ld\n", ftell(opath->path.os9->fd) );
        _coco_write(opath, boottrack, &size);

        sectors = (size + sectorSize - 1) / sectorSize;
        _os9_allbit(opath->path.os9->bitmap,
                    (startlsn + clusterSize - 1) / clusterSize,
                    (sectors + clusterSize - 1) / clusterSize);

        printf("Kernel trackfile written! LSN: %d, size: %d\n", startlsn, size);
        _coco_close(opath);
    }

    /* 2. Process and write OS9Boot to target disk */

    if (bootfile != NULL)
    {
        _path_type boot_type = NATIVE;
        char bootfile_path[512];

        _coco_identify_image(bootfile, &boot_type);

        /* Case A: Source is an OS-9 disk image container — default to reading ',OS9Boot' */
        if (boot_type == OS9 && TSIsDirectory(bootfile))
        {
            snprintf(bootfile_path, sizeof(bootfile_path), "%s,OS9Boot", bootfile);
            printf("Extracting 'OS9Boot' from disk image '%s'\n", bootfile);
        }
        else
        {
            /* Case B: Standalone host file or already fully-qualified path */
            strncpy(bootfile_path, bootfile, sizeof(bootfile_path) - 1);
            bootfile_path[sizeof(bootfile_path) - 1] = '\0';
        }

        /* Let _coco_open handle opening the file (works for contiguous or discontiguous) */
        ec = _coco_open(&cpath, bootfile_path, FAM_READ);
        if (ec != 0)
        {
            fprintf(stderr, "%s: error %d opening bootfile '%s'\n", argv[0], ec, bootfile_path);
            return (1);
        }

        /* Create target 'OS9Boot' on destination device */
        ec = snprintf(buffer, sizeof(buffer), "%s,OS9Boot", device);
        if (ec >= sizeof(buffer))
        {
            _coco_close(cpath);
            fprintf(stderr, "could not generate absolute boot file name\n");
            return (-1);
        }

		coco_file_stat fstat = { 0 };
		fstat.perms = FAP_READ | FAP_WRITE;
        ec = _coco_create(&opath, buffer, FAM_WRITE, &fstat);
        if (ec != 0)
        {
            _coco_close(cpath);
            fprintf(stderr, "%s: error %d creating '%s'\n", argv[0], ec, buffer);
            return (ec);
        }

        /* Stream copy from source to target */
        char ioBuf[8192];
        for (;;)
        {
            u_int n = sizeof(ioBuf);
            ec = _coco_read(cpath, ioBuf, &n);
            if (ec == EOS_EOF || n == 0) break;
            if (ec != 0)
            {
                fprintf(stderr, "%s: error %d reading '%s'\n", argv[0], ec, bootfile_path);
                _coco_close(cpath);
                _coco_close(opath);
                return (ec);
            }
            _coco_write(opath, ioBuf, &n);
        }

        _coco_close(cpath);

        /* Read FD stats of created OS9Boot file to link into target device's LSN0 */
        fd_stats fdbuf;
        _os9_gs_fd(opath->path.os9, sizeof(fdbuf), &fdbuf);

        if (!is_single_segment(fdbuf) && extended == 0)
        {
            printf("Error: %s is fragmented\n", bootfile_path);
            _coco_close(opath);
            return (1);
        }

        int bootfile_Size = int4(fdbuf.fd_siz);
        int bootfile_LSN = opath->path.os9->pl_fd_lsn;
        int bootfile_Data = int3(fdbuf.fd_seg[0].lsn);

        _coco_close(opath);

        /* Link OS9Boot into target device's LSN0 */
        snprintf(buffer, sizeof(buffer), "%s,@", device);
        ec = _coco_open(&opath, buffer, FAM_READ | FAM_WRITE);
        if (ec != 0)
        {
            return (ec);
        }

        size = sizeof(lsn0_sect);
        _coco_read(opath, &LSN0, &size);

        if (size != sizeof(lsn0_sect))
        {
            _coco_close(opath);
            printf("Error reading LSN0\n");
            return (1);
        }

        if (extended == 0 || (bootfile_Size < 65536 && is_single_segment(fdbuf)))
        {
            _int3(bootfile_Data, LSN0.dd_bt);
            _int2(bootfile_Size, LSN0.dd_bsz);
        }
        else
        {
            _int3(bootfile_LSN, LSN0.dd_bt);
            _int2(0, LSN0.dd_bsz);
        }

        _coco_seek(opath, 0, SEEK_SET);
        _coco_write(opath, &LSN0, &size);
        _coco_close(opath);

        printf("Bootfile Linked! LSN: %d, size: %d\n", bootfile_LSN, bootfile_Size);
    }

    return (0);
}

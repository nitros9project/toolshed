/********************************************************************
 * os9id.c - LSN0 display utility for os9 filesystems
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


static int do_id(char **argv, char *p);

/* globals */
static char os9pathlist[256];

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: id {[<opts>]} {<disk> [<...>]} {[<opts>]}\n",
	"Usage:  Display sector 0 of an image.\n",
	"Options:\n",
	NULL
};


int os9id(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
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

		ec = do_id(argv, p);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (0);
}


static int do_id(char **argv, char *p)
{
	error_code ec = 0;
	os9_path_id path;

	strcpy(os9pathlist, p);
	strcat(os9pathlist, ",@");

	/* open a path to the device */
	ec = _os9_open(&path, os9pathlist, FAM_READ);
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d opening '%s'\n", argv[0], ec,
			os9pathlist);
		return (ec);
	}

	{
		char *p;
		lsn0_sect buffer;
		u_int size = sizeof(buffer);
		int is_osk;

		/* read LSN0 */
		ec = _os9_read(path, &buffer, &size);
		printf("\nLSN0 Information:\n");
		printf("  Total sectors   :   %d\n", int3(buffer.dd_tot));
		printf("  Track size      :   %d\n", buffer.dd_tks[0]);
		printf("  Bytes in bitmap :   %d\n", int2(buffer.dd_map));
		printf("  Sectors/Cluster :   %d\n", int2(buffer.dd_bit));
		printf("  Root dir sector :   %d\n", int3(buffer.dd_dir));
		printf("  Disk owner      :   %d.%-3d\n", buffer.dd_own[0],
		       buffer.dd_own[1]);
		printf("  Disk attributes :   ");
		{
			char attrs[9];

			OS9AttrToString(buffer.dd_att[0], attrs);
			printf("%s", attrs);
		}
		printf("\n");
		printf("  Disk ID         :   $%X\n", int2(buffer.dd_dsk));
		/* Build disk format string */
		{
			char formatString[255];
			int fmtbyte = buffer.dd_fmt[0];

			strcpy(formatString, "");

			if (fmtbyte & 4)
			{
				strcat(formatString, "96 TPI");
			}
			else
			{
				strcat(formatString, "48 TPI");
			}

			if (fmtbyte & 2)
			{
				strcat(formatString, ", Double Density");
			}
			else
			{
				strcat(formatString, ", Single Density");
			}

			if (fmtbyte & 1)
			{
				strcat(formatString, ", Double Sided");
			}
			else
			{
				strcat(formatString, ", Single Sided");
			}

			printf("  Disk format     :   $%X (%s)\n",
			       buffer.dd_fmt[0], formatString);
		}
		printf("  Sectors/track   :   %d\n", int2(buffer.dd_spt));
		printf("  Boot sector     :   %d\n", int3(buffer.dd_bt));
		printf("  Bootfile size   :   %d\n", int2(buffer.dd_bsz));
		printf("  Creation date   :   %02d/%02d/%02d %02d:%02d\n",
		       buffer.dd_dat[1],
		       buffer.dd_dat[2],
		       buffer.dd_dat[0] + 1900,
		       buffer.dd_dat[3], buffer.dd_dat[4]);
		p = strdup((char *) buffer.dd_nam);
		printf("  Disk name       :   %s\n",
		       OS9StringToCString((u_char *) p));

		/* Attempt to identify an OS-9/68K os9 formatted disk image */
		is_osk = (int4(buffer.dd_sync) == 0x4372757A);
		if (is_osk)
		{
			printf("  Sync Bytes      :   $4372757A (CRUZ)\n");
			printf("  Bitmap Sector   :   %d\n",
			       int4(buffer.dd_maplsn));
			printf("  LSN0 Version ID :   %d\n",
			       int2(buffer.dd_versid));
		}
		{
			int bytesPerSector = 256;
			u_char *pd_t0s = is_osk ? buffer.dd_opt.m68k.pd_t0s
						: buffer.dd_opt.m6809.pd_t0s;

			if (int1(buffer.dd_lsnsize) > 0)
			{
				bytesPerSector =
					int1(buffer.dd_lsnsize) * 256;
			}
			printf("  Bytes/Sector    :   %d\n", bytesPerSector);
			printf("  Sectors Track 0 :   %d\n", int2(pd_t0s));
		}
	}

	_os9_close(path);

	return (0);
}

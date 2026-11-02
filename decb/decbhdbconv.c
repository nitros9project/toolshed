/********************************************************************
 * decbhdbconv.c - Converts a disk image to a 512-byte sector HDB disk image
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <cococonv.h>
#include <decbpath.h>
#include <sys/stat.h>


#define YES 1
#define NO 0

/* globals */
//static u_int buffer_size = 32768;
//static char *buffer;

static error_code HDBConv(char *srcfile, char *dstfile, int sourceSize,
			  int targetSize);
static error_code check_source(const char *srcfile);


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: hdbconv {[<opts>]} <srcfile> {[<...>]} <target> {[<opts>]}\n",
	"Usage:  Converts an HDB-DOS disk image into a 512-byte sector compatible one.\n",
	"Options:\n",
	"     -2         go from 512-byte sector to 256-byte sector\n",
	"     -5         go from 256-byte sector to 512-byte sector (default)\n",
	NULL
};



int decbhdbconv(int argc, char *argv[])
{
	error_code ec = 0;
	char *source = NULL, *destination = NULL, *p;
	int i;
	int count = 0;
	int sourceSize = 256;
	int targetSize = 512;


	/* 1. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '2':
					sourceSize = 512;
					targetSize = 256;
					break;
				case '5':
					sourceSize = 256;
					targetSize = 512;
					break;
				case 'h':
				case '?':
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


	/* 2. Count non option arguments. */

	for (i = 1, count = 0; i < argc; i++)
	{
		if (argv[i] == NULL)
		{
			continue;
		}

		if (argv[i][0] == '-')
		{
			continue;
		}

		count++;
	}

	if (count != 2)
	{
		show_help(helpMessage);

		return 0;
	}


	/* 3. Walk backwards and get the destination first. */

	for (i = argc - 1; i > 0; i--)
	{
		if (argv[i][0] != '-')
		{
			if (destination == NULL)
			{
				destination = argv[i];
			}
			else
			{
				source = argv[i];
			}
		}
	}


	ec = HDBConv(source, destination, sourceSize, targetSize);

	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d\n", argv[0], ec);
	}


	return ec;
}


/* Is the source a raw image?  If not, say so. */
static error_code check_source(const char *srcfile)
{
	diskimage_format format;
	const char *comma = strchr(srcfile, ',');
	char image[1024];
	size_t length = comma == NULL ? strlen(srcfile) : (size_t) (comma - srcfile);
	error_code ec;

	if (length >= sizeof(image))
	{
		return EOS_BPNAM;
	}
	memcpy(image, srcfile, length);
	image[length] = '\0';

	ec = _diskimage_identify(image, &format);
	if (ec == 0 && format != DISKIMAGE_RAW)
	{
		fprintf(stderr, "hdbconv: '%s' is not a raw disk image, and only raw images can be converted\n", image);
		ec = EOS_WT;
	}

	return ec;
}


static error_code HDBConv(char *srcfile, char *dstfile, int sourceSize,
			  int targetSize)
{
	error_code ec = 0;
	coco_path_id path;
	coco_path_id destpath;
	int mode = FAM_WRITE;
	u_int buffer_size;
	u_char buffer[512];
	coco_file_stat fstat;


	/* 1. Sectors of two sizes only make sense in a raw image. */

	ec = check_source(srcfile);

	if (ec != 0)
	{
		return ec;
	}

	if (_coco_refuse_disk_image_target("hdbconv", dstfile))
	{
		return EOS_FAE;
	}


	/* 2. Open a path to the srcfile. */

	ec = _coco_open(&path, srcfile, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}


	/* 3. Attempt to create the destfile. */

	fstat.perms = FAP_READ | FAP_WRITE | FAP_PREAD;
	ec = _coco_create(&destpath, dstfile, mode, &fstat);

	if (ec != 0)
	{
		_coco_close(path);

		return ec;
	}

	while (ec == 0)
	{
		memset(buffer, 0, sizeof(buffer));
		buffer_size = sourceSize;
		ec = _coco_read(path, buffer, &buffer_size);

		if (ec == EOS_EOF)
		{
			ec = 0;
			break;
		}

		if (ec == 0)
		{
			buffer_size = targetSize;
			ec = _coco_write(destpath, buffer, &buffer_size);
		}
	}

	_coco_close(path);

	{
		error_code close_ec = _coco_close(destpath);

		if (ec == 0)
		{
			ec = close_ec;
		}
	}

	return (ec);
}

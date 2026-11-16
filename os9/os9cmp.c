/********************************************************************
 * os9cmp.c - OS-9 compare utility
 *
 * $Id$
 ********************************************************************/
#include <util.h>

#include "cocopath.h"
#include "cocotypes.h"


#define BUFFSIZ	256

static int do_cmp(char **argv, char *file1, char *file2);
static int compare(u_char * buffer1, u_char * buffer2, size_t num_bytes,
		   size_t total_bytes);

static void show_header(void);

static int different;

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: cmp {[<opts>]} <file1> <file2> {[<...>]} {[<opts>]}\n",
	"Usage:  Compare the contents of two files.\n",
	"Options:\n",
	NULL
};


int os9cmp(int argc, char **argv)
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	char *file1 = NULL, *file2 = NULL;


	/* if no arguments, show help and exit */
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
			if (file1 == NULL)
			{
				file1 = argv[i];
			}
			else
			{
				error_code result;

				file2 = argv[i];
				result = do_cmp(argv, file1, file2);
				if (ec == 0)
				{
					ec = result;
				}
				file1 = NULL;
			}
		}
	}

	return (ec);
}


static int do_cmp(char **argv, char *file1, char *file2)
{
	error_code ec = 0;
	u_char buffer1[BUFFSIZ];
	u_char buffer2[BUFFSIZ];
	coco_path_id path1, path2;
	u_int num_bytes1, num_bytes2;
	error_code ec1, ec2;
	int accum1 = 0, accum2 = 0;
	int diffCount = 0;

	/* open a path to the first file */
	ec = _coco_open(&path1, file1, FAM_READ);

	if (ec != 0)
	{
		fprintf(stderr, "%s: cannot open file\n", argv[0]);
		return (ec);
	}

	/* open a path to the second file */
	ec = _coco_open(&path2, file2, FAM_READ);
	if (ec != 0)
	{
		fprintf(stderr, "%s: cannot open file\n", argv[0]);
		_coco_close(path1);
		return (ec);
	}

	printf("Differences\n");
	different = 0;

	do
	{
		num_bytes1 = BUFFSIZ;
		num_bytes2 = BUFFSIZ;

		ec1 = _coco_read(path1, buffer1, &num_bytes1);
		ec2 = _coco_read(path2, buffer2, &num_bytes2);
		if (ec1 != 0 && ec1 != EOS_EOF)
		{
			ec = ec1;
			break;
		}
		if (ec2 != 0 && ec2 != EOS_EOF)
		{
			ec = ec2;
			break;
		}

		diffCount +=
			compare(buffer1, buffer2,
				(num_bytes1 >
				 num_bytes2) ? num_bytes2 : num_bytes1,
				accum1);

		accum1 += num_bytes1;
		accum2 += num_bytes2;
	}
	while (ec1 == 0 && ec2 == 0);

	if (different == 0)
	{
		printf("None\n");
	}

	printf("Bytes compared:   %08X\n",
	       (accum1 > accum2) ? accum2 : accum1);
	printf("Bytes different:  %08X\n", diffCount);

	if (accum1 > accum2)
	{
		printf("%s is longer\n", file1);
	}
	else if (accum2 > accum1)
	{
		printf("%s is longer\n", file2);
	}

	ec2 = _coco_close(path2);
	ec1 = _coco_close(path1);
	if (ec == 0)
	{
		ec = ec2 != 0 ? ec2 : ec1;
	}

	return (ec);
}


static int compare(u_char * buffer1, u_char * buffer2, size_t num_bytes,
		   size_t total_bytes)
{
	u_int i;
	int dc = 0;

	for (i = 0; i < num_bytes; i++)
	{
		if (buffer1[i] != buffer2[i])
		{
			if (different == 0)
			{
				different = 1;
				show_header();
			}
			printf("%08lx  %02x %02x\n",
			       (unsigned long) total_bytes + i, buffer1[i],
			       buffer2[i]);
			dc++;
		}
	}

	return (dc);
}


static void show_header(void)
{
	printf("byte      #1 #2\n");
	printf("========= == ==\n");

	return;
}

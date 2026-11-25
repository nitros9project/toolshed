/********************************************************************
 * decblist.c - List utility for Disk BASIC
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cococonv.h>
#include <cocotypes.h>
#include <cocopath.h>

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: list {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display contents of a text file.\n",
	"Options:\n",
	"     -t         perform BASIC token translation\n",
	"     -s         perform S-Record encoding of binary\n",
	NULL
};

#define BLOCKSIZE 256

int decblist(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	coco_path_id path;
	int i;
	unsigned char *buffer, *buffer2;
	u_int size, size2;
	int token_translation = 0;
	int srec_translation = 0;
	_path_type disk_type;

	/* 1. Walk command line for options */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{

				switch (*p)
				{
				case 't':
					token_translation = 1;
					break;

				case 's':
					srec_translation = 1;
					break;


				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (EOS_IA);
				}
			}
		}
	}


	/* 2. Walk command line for pathnames. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			p = argv[i];
			break;
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	/* 3. Open a path to the file. */

	ec = _coco_open_read_whole_file(&path, p, FAM_READ, &buffer, &size);

	if (ec != 0)
	{
		printf("Error %d opening %s\n", ec, p);

		return (ec);
	}

	_coco_gs_pathtype(path, &disk_type);

	if (token_translation == 1)
	{
		char *program;
		u_int program_size;

		if (cecb_suggest_mc10 || (disk_type == CECB && path->path.cecb->tape_type == C10))
		{
			ec = _cecb_detoken(buffer, size, &program, &program_size);

			if (ec != 0)
			{
				return ec;
			}
		}
		else
		{
			ec = _decb_detoken(buffer, size, &program, &program_size);
			if (ec != 0)
			{
				return ec;
			}
		}

		free(buffer);
		buffer = (u_char *) program;
		size = program_size;
	}

	if (srec_translation == 1)
	{
		char *program;
		u_int program_size;
		coco_file_stat statbuf;

		_coco_gs_fd(path, &statbuf);

		if ((disk_type == CECB) && (statbuf.gap_flag == 0))
			ec = _decb_srec_encode_sr(buffer, size,
						  statbuf.ml_load_address,
						  statbuf.ml_exec_address,
						  &program, &program_size);
		else
			ec = _decb_srec_encode(buffer, size, &program,
					       &program_size);

		if (ec != 0)
		{
			return ec;
		}

		free(buffer);
		buffer = (u_char *) program;
		size = program_size;
	}

	DECBToNative((char *) buffer, size, (char **) &buffer2, &size2);

	WriteNativeText(stdout, (char *) buffer2, size2);

	free(buffer);
	free(buffer2);

	_coco_close(path);

	return (0);
}

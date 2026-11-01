/********************************************************************
 * dgndoslist.c - List utility for Dragon DOS
 ********************************************************************/
#include <util.h>
#include <cococonv.h>
#include <cocopath.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: list {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display contents of a text file.\n",
	"Options:\n",
	NULL
};


static error_code do_list(char *file)
{
	coco_path_id path;
	u_char *buffer = NULL;
	char *translated;
	u_int size = 0, translated_size;
	error_code ec = _coco_open_read_whole_file(&path, file, FAM_READ, &buffer, &size);

	if (path == NULL)
	{
		return ec;
	}

	if (ec == 0 && size != 0)
	{
		DECBToNative((char *) buffer, size, &translated, &translated_size);
		fwrite(translated, translated_size, 1, stdout);
		free(translated);
	}

	free(buffer);
	_coco_close(path);
	return ec;
}


int dgndoslist(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;

	/* 1. Walk command line for options. */

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

				default:
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (DGNDOS_USAGE_ERROR);
				}
			}
		}
	}

	/* 2. Walk command line for pathnames. */

	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = do_list(p);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d with '%s'\n", argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

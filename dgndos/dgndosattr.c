/********************************************************************
 * dgndosattr.c - File attribute utility for Dragon DOS
 ********************************************************************/
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: attr {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display or change whether files are protected.\n",
	"Options:\n",
	"     -p        protect the file\n",
	"     -u        unprotect the file\n",
	NULL
};


static error_code do_attr(char *file, int change, int protect)
{
	dragondos_path_id path;
	dragondos_file_stat stat;
	error_code ec = _dragondos_open(&path, file, change ? FAM_READ | FAM_WRITE : FAM_READ);

	if (ec != 0)
	{
		return ec;
	}

	ec = _dragondos_gs_fd(path, &stat);
	if (ec == 0 && change)
	{
		stat.protected_file = protect;
		ec = _dragondos_ss_fd(path, &stat);
	}
	else if (ec == 0)
	{
		printf("%s: %s\n", file, stat.protected_file ? "protected" : "not protected");
	}

	{
		error_code close_ec = _dragondos_close(path);

		return ec != 0 ? ec : close_ec;
	}
}


int dgndosattr(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int change = 0, protect = 0;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'p':
					change = 1;
					protect = 1;
					break;

				case 'u':
					change = 1;
					protect = 0;
					break;

				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (DGNDOS_USAGE_ERROR);
				}
			}
		}
	}

	/* walk command line for pathnames */
	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = do_attr(p, change, protect);
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

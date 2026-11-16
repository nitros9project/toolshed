/********************************************************************
 * os9list.c - List utility for OS-9
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

static error_code list_file(char *filename);

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: list {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display contents of a text file.\n",
	"Options:\n",
	NULL
};


int os9list(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int files = 0;

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
					return 0;

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return 1;
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
			files++;
			ec = list_file(argv[i]);
			if (ec != 0)
			{
				return ec;
			}
		}
	}

	if (files == 0)
	{
		show_help(helpMessage);
		return 1;
	}

	return 0;
}

static error_code list_file(char *filename)
{
	os9_path_id path;
	error_code ec, close_ec;

	/* open a path to the file */
	ec = _os9_open(&path, filename, FAM_READ);
	if (ec != 0)
	{
		printf("Error %d opening %s\n", ec, filename);
		return ec;
	}

	while (_os9_gs_eof(path) == 0)
	{
		u_int size = 1022;
		char buffer[1024];
		char *p;

		ec = _os9_readln(path, buffer, &size);
		if (ec != 0)
		{
			break;
		}

		buffer[size] = '\0';

		p = strchr(buffer, 0x0D);

		if (p != NULL)
		{
			*p = '\0';
		}

		printf("%s\n", buffer);
	}

	close_ec = _os9_close(path);

	return ec != 0 ? ec : close_ec;
}

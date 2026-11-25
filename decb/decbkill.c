/********************************************************************
 * decbkill.c - Delete Disk BASIC file
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cocotypes.h>
#include <decbpath.h>


static int do_kill(char **argv, char *p);

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: kill {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Delete one or more files.\n",
	"Options:\n",
	NULL
};


int decbkill(int argc, char *argv[])
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
				case '?':
				case 'h':
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
		}

		ec = do_kill(argv, p);

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

		return 0;
	}


	return 0;
}



static int do_kill(char **argv, char *p)
{
	error_code ec = 0;


	ec = _decb_kill(p);


	return ec;
}

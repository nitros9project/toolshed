/********************************************************************
 * dgndoskill.c - Delete Dragon DOS file
 ********************************************************************/
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: kill {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Delete one or more files.\n",
	"Options:\n",
	NULL
};


int dgndoskill(int argc, char *argv[])
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
		ec = _dragondos_kill(p);
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

/********************************************************************
 * os9del.c - Delete file entry for OS-9 filesystems
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
#include <cocopath.h>


error_code RBFDelete(char *file)
{
	return _os9_delete(file);
}


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: del {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Delete one or more files.\n",
	"Options:\n",
	NULL
};


int os9del(int argc, char *argv[])
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
					return (EOS_IA);
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

		ec = RBFDelete(p);

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

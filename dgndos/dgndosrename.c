/********************************************************************
 * dgndosrename.c - Renames Dragon DOS filename
 ********************************************************************/
#include <util.h>
#include <dragondospath.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: rename {<file> <newfilename>}\n",
	"Usage:  Give a file a new filename.\n",
	"Options:\n",
	NULL
};


int dgndosrename(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL, *file = NULL, *new_name = NULL;
	int i;

	/* walk command line for options and pathnames */
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
		else if (file == NULL)
		{
			file = argv[i];
		}
		else if (new_name == NULL)
		{
			new_name = argv[i];
		}
	}

	if (file == NULL || new_name == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	ec = _dragondos_rename(file, new_name);
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d with '%s'\n", argv[0], ec, file);
	}

	return (ec);
}

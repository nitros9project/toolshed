/********************************************************************
 * dgndosdsave.c - Copy the contents of a Dragon DOS disk or a directory
 ********************************************************************/
#include <string.h>
#include <toolshed.h>
#include <util.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dsave {[<opts>]} {[<source>]} <target> {[<opts>]}\n",
	"Usage:  Copy the contents of a disk or directory.\n",
	"Options:\n",
	"     -e         actually execute commands\n",
	"     -l         perform end of line translation on copy\n",
	"     -r         force rewrite on copy\n",
	"     -t         perform BASIC token translation\n",
	"     -x=path    dgndos executable to run the commands with\n",
	NULL
};


int dgndosdsave(int argc, char *argv[])
{
	ts_dsave_options options;
	error_code ec;
	char *p;
	char *source = NULL, *target = NULL;
	int i, count = 0;

	memset(&options, 0, sizeof(options));
	options.program = "dgndos";

	/* Dragon DOS has no directories, and so no makdir. */
	options.no_directories = 1;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'e':
					options.execute = 1;
					break;

				case 'l':
					options.eol_translate = 1;
					break;

				case 'r':
					options.rewrite = 1;
					break;

				case 't':
					options.token_translate = 1;
					break;

				case 'x':
					if (*(++p) == '=')
					{
						p++;
					}
					if (*p == '\0')
					{
						fprintf(stderr, "%s: -x needs the path of an executable\n", argv[0]);
						return (DGNDOS_USAGE_ERROR);
					}
					options.executable = p;
					p += strlen(p) - 1;
					break;

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

	/* Count non option arguments */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		if (source == NULL)
		{
			source = argv[i];
		}
		else if (target == NULL)
		{
			target = argv[i];
		}
		count++;
	}

	if (count < 1 || count > 2)
	{
		show_help(helpMessage);
		return (DGNDOS_USAGE_ERROR);
	}

	/* if target is NULL, then source is really . and target is source */
	if (target == NULL)
	{
		target = source;
		source = ".";
	}

	ec = TSDSave(&options, source, target);
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d encountered during dsave\n", argv[0], ec);
	}

	return (ec);
}

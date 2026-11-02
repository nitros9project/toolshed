/********************************************************************
 * decbdsave.c - Copy utility for RSDOS filesystem
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <toolshed.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dsave {[<opts>]} {[<source>]} <target> {[<opts>]}\n",
	"Usage: Copy the contents of a directory or device.\n",
	"Options:\n",
	"     -b=size    size of copy buffer in bytes or K-bytes\n",
	"     -e         actually execute commands\n",
	"     -l         perform end of line translation on copy\n",
	"     -t         perform BASIC token translation\n",
	"     -r         force rewrite on copy\n",
	"     -x=path    decb executable to run the commands with\n",
	NULL
};


int decbdsave(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL, *q;
	int i;
	int count = 0;
	u_int buffer_size = 32768;
	ts_dsave_options options;
	char *target = NULL;
	char *source = NULL;

	memset(&options, 0, sizeof(options));
	options.program = "decb";

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'b':
					if (*(++p) == '=')
					{
						p++;
					}
					q = p + strlen(p) - 1;
					if (toupper((unsigned char)*q) == 'K')
					{
						*q = '\0';
						buffer_size = atoi(p) * 1024;
					}
					else
					{
						buffer_size = atoi(p);
					}
					p = q;
					break;

				case 'r':
					options.rewrite = 1;
					break;

				case 'l':
					options.eol_translate = 1;
					break;

				case 't':
					options.token_translate = 1;
					break;

				case 'e':
					options.execute = 1;
					break;

				case 'x':
					if (*(++p) == '=')
					{
						p++;
					}
					if (*p == '\0')
					{
						fprintf(stderr, "%s: -x needs the path of an executable\n", argv[0]);
						return (1);
					}
					options.executable = p;
					p += strlen(p) - 1;
					break;

				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (1);
				}
			}
		}
	}

	/* Count non option arguments */
	for (i = 1, count = 0; i < argc; i++)
	{
		if (argv[i] == NULL)
		{
			continue;
		}

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
		return (1);
	}

	/* if target is NULL, then source is really . and target is source */
	if (target == NULL)
	{
		target = source;
		source = ".";
	}

	/* do dsave.  decb copy has no use for the buffer size. */
	(void) buffer_size;
	ec = TSDSave(&options, source, target);
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d encountered during dsave\n",
			argv[0], ec);
	}

	return (ec);
}

/********************************************************************
 * os9padrom.c - OS-9 ROM padding utility
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <cocopath.h>
#include <cocotypes.h>
#include <toolshed.h>

#define BUFFSIZ	256


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: padrom {[<opts>]} <padsize> {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Pad a file to a specific length.\n",
	"Options:\n",
	"     -b          place padding at the beginning of the file\n",
	"     -c[=]<n>    character to pad (n=dec, %bin, 0oct or $hex)\n",
	NULL
};


int os9padrom(int argc, char **argv)
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int padSize = 0;
	int padAtStart = 0;
	char padChar = '\xff';
	char *file = NULL;


	/* 1. If no arguments, show help and return. */

	if (argv[1] == NULL)
	{
		show_help(helpMessage);

		return 0;
	}


	/* 2. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				char *q;


				switch (*p)
				{
				case 'b':
					padAtStart = 1;
					break;

				case 'c':
					if (*(++p) == '=')
					{
						p++;
					}
					q = p + strlen(p) - 1;
					padChar = StrToInt(p);
					p = q;
					break;

				case '?':
				case 'h':
					show_help(helpMessage);

					return 0;

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (EOS_IA);
				}
			}
		}
	}


	/* 3. Walk command line for pathnames. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			if (padSize == 0)
			{
				padSize = StrToInt(argv[i]);
				if (padSize <= 0)
				{
					fprintf(stderr,
						"%s: illegal pad size\n",
						argv[0]);

					return 1;
				}
			}
			else
			{
				file = argv[i];
				ec = TSPadROM(file, padSize, padChar,
					       padAtStart);
				if (ec != 0)
				{
					break;
				}
			}
		}
	}


	return ec;
}

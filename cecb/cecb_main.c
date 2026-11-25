/********************************************************************
 * cecb.c - Cassette BASIC Tools Executive
 *
 * $Id$
 ********************************************************************/
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <toolshed.h>

#include <util.h>
#include <cecbpath.h>

static void show_cecb_help(char const *const *helpMessage);
static int do_command(int argc, char **argv);

/* Help message */
static char const *const helpMessage[] = {
	"cecb from Toolshed " TOOLSHED_VERSION "\n",
	"Syntax: cecb {[<opts>]} <sub command> {[<opts>]}\n",
	"Usage:  Cassette BASIC File Tools Executive\n",
	"        Works with WAV, CAS and C10 files\n",
	"Options:\n",
	"     -r<real> = Set WAV FSK aspect ratio for autodetection. Default: 1.85\n",
	"     -t<%>    = Set WAV FSK aspect ratio threshold. Default: 0.3 (30%)\n",
	"     -n<%>    = Set near-zero threshold for WAV samples. Default: 0.01 (1%)\n",
	"     -s<n>    = Start at sample/bit <n> in the input file.\n",
	"     -z       = Suggest MC-10 mode.\n",
	"                 Use MC-10 WAV files and MicroColor BASIC tokens.\n",
	"\n",
	"     % is a decimal number between 0 and 1.\n",
	"     A value can also be the next argument, as in -r 1.85.\n",
	NULL
};


struct cmdtbl
{
	int (*func)(int, char **);
	char *keyword;
	char *shortcode;
	char *synopsis;
};


static struct cmdtbl table[] = {
	{cecbdir, "dir", "dr", "List the contents of a cassette image."},
	{cecbfstat, "fstat", "fs", "Display detailed file information."},
	{cocodump, "dump", "du", "Display a hexadecimal dump of a file."},
	{cecbdumpblock, "dumpblock", "db", "Display a hexadecimal dump of cassette blocks."},
	{decblist, "list", "li", "Display text or a tokenized BASIC program."},
	{cecbbulkerase, "bulkerase", "be", "Create blank cassette images."},
	{cecbcopy, "copy", "cp", "Copy files between images and native files."},
	{cecbleader, "leader", "le", "Analyze leaders in a WAV cassette image."},
	{NULL, NULL, NULL, NULL}
};


int main(int argc, char *argv[])
{
	error_code ec = 0;
	int i;
	char *p, *command = NULL;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				char option = *p;
				char *value;

				switch (option)
				{
				case 'h':
				case '?':
					show_cecb_help(helpMessage);
					return (0);

				case 'z':
					cecb_suggest_mc10 = 1;
					continue;

				case 'r':
				case 't':
				case 'n':
				case 's':
					break;

				default:
					fprintf(stderr, "cecb: unknown option '%c'\n", option);
					return (EOS_IA);
				}

				/* The value follows the letter, or is the next argument. */
				value = p + 1;
				if (*value == '\0')
				{
					if (i + 1 >= argc)
					{
						fprintf(stderr, "cecb: -%c needs a value\n", option);
						return (EOS_IA);
					}
					value = argv[++i];
				}

				switch (option)
				{
				case 'r':
					cecb_ratio = strtod(value, NULL);
					break;
				case 't':
					cecb_threshold = strtod(value, NULL);
					break;
				case 'n':
					cecb_zero_adjust = strtod(value, NULL);
					break;
				case 's':
					cecb_start_sample = strtol(value, NULL, 0);
					break;
				}
				break;
			}
		}
		else
		{
			command = argv[i];
			break;
		}
	}

	if (command == NULL)
	{
		show_cecb_help(helpMessage);
	}
	else
	{
		ec = do_command(argc - i, &argv[i]);
	}

	if (ec != 0 )
	{
		fprintf(stderr, "Error: %d - %s\n", ec, TSReportError(ec));
 	}

	return (ec);
}


static int do_command(int argc, char **argv)
{
	struct cmdtbl *x = table;

	while (x->func != NULL)
	{
		if (strcmp(argv[0], x->keyword) == 0 || strcmp(argv[0], x->shortcode) == 0)
		{
			return (x->func(argc, argv));
		}
		x++;
	}

	if (x->func == NULL)
	{
		fprintf(stderr, "cecb: unknown command '%s'\n", argv[0]);
	}

	return (0);
}


static void show_cecb_help(char const *const *helpMessage)
{
	char const *const *p = helpMessage;
	struct cmdtbl *ptr = table;

	while (*p)
	{
		fputs(*(p++), stderr);
	}

	printf("\nSub commands:\n");

	while (ptr->keyword != NULL)
	{
		printf("     %-9s (%s)  %s\n", ptr->keyword, ptr->shortcode, ptr->synopsis);
		ptr++;
	}

	return;
}

/********************************************************************
 * cflex_main.c - FLEX Tools Executive
 ********************************************************************/
#include <string.h>
#include <toolshed.h>

#include "util.h"
#include "cflex.h"


static void show_cflex_help(char const *const *helpMessage);
static int do_command(int argc, char **argv);

/* Help message */
static char const *const helpMessage[] = {
	"cflex from Toolshed " TOOLSHED_VERSION "\n",
	"Syntax: cflex {[<opts>]} <command> {[<opts>]}\n",
	"Usage:  FLEX File Tools Executive\n",
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
	{cflexattr, "attr", "at", "Display or modify file attributes."},
	{cflexcopy, "copy", "cp", "Copy files between images and native files."},
	{cflexdir, "dir", "dr", "List the contents of a directory."},
	{cflexdsave, "dsave", "ds", "Copy the contents of a directory or device."},
	{cflexdskinit, "dskinit", "di", "Create and initialize a disk image."},
	{cocodump, "dump", "du", "Display a hexadecimal dump of a file."},
	{cflexfree, "free", "fr", "Display free space on a disk image."},
	{cflexfstat, "fstat", "fs", "Display detailed file information."},
	{cflexgen, "gen", "ge", "Make a disk boot FLEX on the Color Computer."},
	{cocoinfo, "info", "in", "Report disk image format and track layout."},
	{cflexkill, "kill", "ki", "Delete files."},
	{cflexlist, "list", "li", "Display the contents of a text file."},
	{cflexrename, "rename", "mv", "Rename a file."},
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
				switch (*p)
				{
				case 'h':
				case '?':
					show_cflex_help(helpMessage);
					return (0);
				}
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
		show_cflex_help(helpMessage);
	}
	else
	{
		ec = do_command(argc - i, &argv[i]);
	}

	/* info reports its own errors; -s uses nonzero statuses for filesystems. */
	if (ec != 0 && ec != CFLEX_USAGE_ERROR &&
	    strcmp(command, "info") != 0 && strcmp(command, "in") != 0)
	{
		fprintf(stderr, "Error: %d - %s\n", ec, TSReportError(ec));
	}

	/* An exit status only holds 8 bits, which would make 256 mean success. */
	return (ec > 255 ? CFLEX_USAGE_ERROR : ec);
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

	fprintf(stderr, "cflex: unknown command '%s'\n", argv[0]);

	return (CFLEX_USAGE_ERROR);
}


static void show_cflex_help(char const *const *helpMessage)
{
	char const *const *p = helpMessage;
	struct cmdtbl *ptr = table;

	while (*p)
	{
		fputs(*(p++), stderr);
	}

	printf("\nCommands:\n");

	while (ptr->keyword != NULL)
	{
		printf("     %-9s (%s)  %s\n", ptr->keyword, ptr->shortcode, ptr->synopsis);
		ptr++;
	}
}

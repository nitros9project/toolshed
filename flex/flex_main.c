/********************************************************************
 * flex_main.c - FLEX Tools Executive
 ********************************************************************/
#include <ctype.h>
#include <string.h>
#include <toolshed.h>

#include "util.h"
#include "flex.h"


static void show_flex_help(char const *const *helpMessage);
static int do_command(int argc, char **argv);

/* Help message */
static char const *const helpMessage[] = {
	"flex from Toolshed " TOOLSHED_VERSION "\n",
	"Syntax: flex {[<opts>]} <command> {[<opts>]}\n",
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
	{flexattr, "attr", "at", "Display or modify file attributes."},
	{flexcopy, "copy", "cp", "Copy files between images and native files."},
	{flexdir, "dir", "dr", "List the contents of a directory."},
	{flexdsave, "dsave", "ds", "Copy the contents of a directory or device."},
	{flexdskinit, "dskinit", "di", "Create and initialize a disk image."},
	{cocodump, "dump", "du", "Display a hexadecimal dump of a file."},
	{flexfree, "free", "fr", "Display free space on a disk image."},
	{flexfstat, "fstat", "fs", "Display detailed file information."},
	{cocoinfo, "info", "in", "Report disk image format and track layout."},
	{flexkill, "kill", "ki", "Delete files."},
	{flexlist, "list", "li", "Display the contents of a text file."},
	{flexrename, "rename", "mv", "Rename a file."},
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
					show_flex_help(helpMessage);
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
		show_flex_help(helpMessage);
	}
	else
	{
		ec = do_command(argc - i, &argv[i]);
	}

	/* info reports its own errors; -s uses nonzero statuses for filesystems. */
	if (ec != 0 && ec != FLEX_USAGE_ERROR &&
	    strcmp(command, "info") != 0 && strcmp(command, "in") != 0)
	{
		fprintf(stderr, "Error: %d - %s\n", ec, TSReportError(ec));
	}

	/* An exit status only holds 8 bits, which would make 256 mean success. */
	return (ec > 255 ? FLEX_USAGE_ERROR : ec);
}


int flex_disk_pathlist(const char *disk, char *pathlist, size_t size)
{
	int length = snprintf(pathlist, size, "%s%s", disk, strchr(disk, ',') == NULL ? "," : "");

	return (length < 0 || (size_t) length >= size) ? EOS_BPNAM : 0;
}


void flex_print_field(const unsigned char *field, int length, int pad)
{
	int i;

	for (i = 0; i < length; i++)
	{
		if (field[i] == 0)
		{
			if (pad)
			{
				putchar(' ');
			}
		}
		else if (isprint(field[i]))
		{
			putchar(field[i]);
		}
		else
		{
			printf("\\%o", field[i]);
		}
	}
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

	fprintf(stderr, "flex: unknown command '%s'\n", argv[0]);

	return (FLEX_USAGE_ERROR);
}


static void show_flex_help(char const *const *helpMessage)
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

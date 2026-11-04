/********************************************************************
 * os9.c - OS-9 File Tools Executive
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <toolshed.h>


static void show_os9_help(char const *const *helpMessage);
static int do_command(int argc, char **argv);

/* Help message */
static char const *const helpMessage[] = {
	"os9 from Toolshed " TOOLSHED_VERSION "\n",
	"Syntax: os9 {[<opts>]} <command> {[<opts>]}\n",
	"Usage:  OS-9 File Tools Executive\n",
	"Options:\n",
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
	{os9attr, "attr", "at", "Display or modify file attributes."},
	{os9cmp, "cmp", "cm", "Compare the contents of two files."},
	{os9copy, "copy", "cp", "Copy files between images and native files."},
	{os9dcheck, "dcheck", "dc", "Verify the file structure of a disk image."},
	{os9del, "del", "de", "Delete files."},
	{os9deldir, "deldir", "dd", "Delete a directory and its contents."},
	{os9dir, "dir", "dr", "List the contents of a directory."},
	{os9dsave, "dsave", "ds", "Copy the contents of a directory or device."},
	{cocodump, "dump", "du", "Display a hexadecimal dump of a file."},
	{os9format, "format", "fo", "Create and format a disk image."},
	{os9free, "free", "fr", "Display free space on a disk image."},
	{os9fstat, "fstat", "fs", "Display the file descriptor sector for a file."},
	{os9gen, "gen", "ge", "Prepare a disk image for booting."},
	{os9id, "id", "id", "Display sector zero of a disk image."},
	{os9ident, "ident", "it", "Display OS-9 module information."},
	{cocoinfo, "info", "in", "Report disk image format and track layout."},
	{os9list, "list", "li", "Display the contents of a text file."},
	{os9makdir, "makdir", "md", "Create directories."},
	{os9modbust, "modbust", "mb", "Split merged OS-9 modules into separate files."},
	{os9padrom, "padrom", "pr", "Pad a file to a specified length."},
	{os9rename, "rename", "mv", "Rename a file."},
	{os9reveal, "reveal", "rv", "Explain what occupies a disk sector or byte."},
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
					show_os9_help(helpMessage);
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
		show_os9_help(helpMessage);
	}
	else
	{
		ec = do_command(argc - i, &argv[i]);
	}

	/* info reports its own errors; -s uses nonzero statuses for filesystems. */
	if (ec != 0 && strcmp(command, "info") != 0 && strcmp(command, "in") != 0)
	{
		fprintf(stderr, "Error: %d - %s\n", ec, TSReportError(ec));
	}

	return (ec);
}


static int do_command(int argc, char **argv)
{
	struct cmdtbl *x;

	for (x = table; x->func != NULL; x++)
	{
		if (strcmp(argv[0], x->keyword) == 0 || strcmp(argv[0], x->shortcode) == 0)
		{
			return (x->func(argc, argv));
		}
	}

	fprintf(stderr, "os9: unknown command '%s'\n", argv[0]);

	return (1);
}


static void show_os9_help(char const *const *helpMessage)
{
	char const *const *p;
	struct cmdtbl *ptr;

	for (p = helpMessage; *p; p++)
	{
		fputs(*p, stderr);
	}

	printf("\nCommands:\n");

	for (ptr = table; ptr->keyword != NULL; ptr++)
	{
		printf("     %-9s (%s)  %s\n", ptr->keyword, ptr->shortcode, ptr->synopsis);
	}

	return;
}

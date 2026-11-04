/********************************************************************
 * dgndos_main.c - Dragon DOS Tools Executive
 ********************************************************************/
#include <string.h>
#include <toolshed.h>

#include "util.h"
#include "dgndos.h"


static void show_dgndos_help(char const *const *helpMessage);
static int do_command(int argc, char **argv);

/* Help message */
static char const *const helpMessage[] = {
	"dgndos from Toolshed " TOOLSHED_VERSION "\n",
	"Syntax: dgndos {[<opts>]} <command> {[<opts>]}\n",
	"Usage:  Dragon DOS File Tools Executive\n",
	NULL
};


struct cmdtbl
{
	int (*func)(int, char **);
	char *keyword;
};


static struct cmdtbl table[] = {
	{dgndosattr, "attr"},
	{dgndoscopy, "copy"},
	{dgndosdir, "dir"},
	{dgndosdsave, "dsave"},
	{dgndosdskinit, "dskinit"},
	{cocodump, "dump"},
	{dgndosfree, "free"},
	{dgndosfstat, "fstat"},
	{cocoinfo, "info"},
	{dgndoskill, "kill"},
	{dgndoslist, "list"},
	{dgndosrename, "rename"},
	{dgndosreveal, "reveal"},
	{NULL, NULL}
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
					show_dgndos_help(helpMessage);
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
		show_dgndos_help(helpMessage);
	}
	else
	{
		ec = do_command(argc - i, &argv[i]);
	}

	/* info reports its own errors; -s uses nonzero statuses for filesystems. */
	if (ec != 0 && ec != DGNDOS_USAGE_ERROR && strcmp(command, "info") != 0)
	{
		fprintf(stderr, "Error: %d - %s\n", ec, TSReportError(ec));
	}

	/* An exit status only holds 8 bits, which would make 256 mean success. */
	return (ec > 255 ? DGNDOS_USAGE_ERROR : ec);
}


int dgndos_disk_pathlist(const char *disk, char *pathlist, size_t size)
{
	int length = snprintf(pathlist, size, "%s%s", disk, strchr(disk, ',') == NULL ? "," : "");

	return (length < 0 || (size_t) length >= size) ? EOS_BPNAM : 0;
}


static int do_command(int argc, char **argv)
{
	struct cmdtbl *x = table;

	while (x->func != NULL)
	{
		if (strcmp(argv[0], x->keyword) == 0)
		{
			return (x->func(argc, argv));
		}
		x++;
	}

	fprintf(stderr, "dgndos: unknown command '%s'\n", argv[0]);

	return (DGNDOS_USAGE_ERROR);
}


static void show_dgndos_help(char const *const *helpMessage)
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
		printf("     %s\n", ptr->keyword);
		ptr++;
	}
}

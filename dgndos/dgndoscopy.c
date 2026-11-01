/********************************************************************
 * dgndoscopy.c - Copy utility for Dragon DOS
 ********************************************************************/
#include <string.h>
#include <toolshed.h>
#include <util.h>

#include "dgndos.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: copy {[<opts>]} <srcfile> {[<...>]} <target> {[<opts>]}\n",
	"Usage:  Copy one or more files to a target file or directory.\n",
	"Options:\n",
	"     -l         perform end of line translation\n",
	"     -r         rewrite if file exists\n",
	NULL
};


/* Is the target a place to put files with their own names? */
static int is_directory(char *target)
{
	char *comma = strchr(target, ',');
	coco_path_id path;

	if (comma != NULL)
	{
		return comma[1] == '\0';
	}

	if (_coco_open(&path, target, FAM_DIR | FAM_READ) != 0)
	{
		return 0;
	}
	_coco_close(path);
	return 1;
}


/* This works for both native file paths and image file paths. */
static char *get_filename(char *path)
{
	char *a = strchr(path, ',');
	char *b = strrchr(a == NULL ? path : a, '/');

	if (b != NULL)
	{
		return b + 1;
	}
	return a == NULL ? path : a + 1;
}


static error_code copy_file(char *source, char *target, int target_is_directory, int eol_translate, int rewrite)
{
	static char buffer[32768];
	char destination[1024];
	int length;
	error_code ec;

	if (target_is_directory)
	{
		length = snprintf(destination, sizeof(destination), "%s%s%s", target,
				  (strchr(target, ',') != NULL || target[strlen(target) - 1] == '/') ? "" : "/",
				  get_filename(source));
	}
	else
	{
		length = snprintf(destination, sizeof(destination), "%s", target);
	}
	if (length < 0 || (size_t) length >= sizeof(destination))
	{
		return EOS_BPNAM;
	}

	ec = TSCopyFile(source, destination, eol_translate, rewrite, 0, 0, buffer, sizeof(buffer));

	/* Don't leave what fit of a file too large for the disk. */
	if (ec == EOS_DF)
	{
		_coco_delete(destination);
	}
	return ec;
}


int dgndoscopy(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL, *target = NULL;
	int i, count = 0;
	int eol_translate = 0, rewrite = 0;
	int target_is_directory;

	/* 1. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'l':
					eol_translate = 1;
					break;

				case 'r':
					rewrite = 1;
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
		else
		{
			target = argv[i];
			count++;
		}
	}

	if (count < 2)
	{
		show_help(helpMessage);
		return (0);
	}

	/* 2. Copy each source to the target, which is the last name given. */

	target_is_directory = is_directory(target);

	if (!target_is_directory && count > 2)
	{
		fprintf(stderr, "%s: two or more sources requires target to be a directory\n", argv[0]);
		return (DGNDOS_USAGE_ERROR);
	}

	for (i = 1; i < argc; i++)
	{
		error_code copy_ec;

		if (argv[i][0] == '-' || argv[i] == target)
		{
			continue;
		}

		copy_ec = copy_file(argv[i], target, target_is_directory, eol_translate, rewrite);
		if (copy_ec != 0)
		{
			fprintf(stderr, "%s: error %d copying '%s'\n", argv[0], copy_ec, argv[i]);
			if (ec == 0)
			{
				ec = copy_ec;
			}
		}
	}

	return (ec);
}

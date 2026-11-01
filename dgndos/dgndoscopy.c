/********************************************************************
 * dgndoscopy.c - Copy utility for Dragon DOS
 ********************************************************************/
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
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
	"     -t         perform BASIC token translation\n",
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


/* Tokenize text, or list a tokenized program, and write the result.  Text on a
   Dragon DOS disk has lines ending in CR. */
static error_code copy_translating_tokens(char *source, char *destination, int rewrite)
{
	coco_path_id path, destpath;
	coco_file_stat fstat;
	u_char *buffer = NULL, *result = NULL;
	u_int size = 0, result_size = 0;
	error_code ec = _coco_open_read_whole_file(&path, source, FAM_READ, &buffer, &size);

	if (path == NULL)
	{
		return ec;
	}
	_coco_close(path);

	if (ec == 0)
	{
		if (_dragondos_detect_tokenized(buffer, size) == 0)
		{
			char *text;
			u_int text_size;

			ec = _dragondos_detoken(buffer, size, &text, &text_size);
			if (ec == 0)
			{
				_path_type type;

				_coco_identify_image(destination, &type);
				if (type == NATIVE)
				{
					result = (u_char *) text;
					result_size = text_size;
				}
				else
				{
					char *translated;

					NativeToCoCo(text, text_size, &translated, &result_size);
					result = (u_char *) translated;
					free(text);
				}
			}
		}
		else
		{
			ec = _dragondos_entoken(buffer, size, &result, &result_size, DRAGONDOS_BASIC_LOAD_ADDRESS);
		}
	}
	free(buffer);

	if (ec == 0)
	{
		memset(&fstat, 0, sizeof(fstat));
		fstat.perms = FAP_READ | FAP_WRITE | FAP_PREAD;
		ec = _coco_create(&destpath, destination, rewrite ? FAM_WRITE : FAM_WRITE | FAM_NOCREATE, &fstat);
	}
	if (ec == 0)
	{
		if (result_size != 0)
		{
			ec = _coco_write(destpath, result, &result_size);
		}
		_coco_close(destpath);
	}

	free(result);
	return ec;
}


static error_code copy_file(char *source, char *target, int target_is_directory, int eol_translate, int rewrite,
			    int token_translate)
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

	if (_coco_refuse_disk_image_target("copy", destination))
	{
		return DGNDOS_USAGE_ERROR;
	}

	if (token_translate)
	{
		ec = copy_translating_tokens(source, destination, rewrite);
	}
	else
	{
		ec = TSCopyFile(source, destination, eol_translate, rewrite, 0, 0, buffer, sizeof(buffer));
	}

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
	int eol_translate = 0, rewrite = 0, token_translate = 0;
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

				case 't':
					token_translate = 1;
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

		copy_ec = copy_file(argv[i], target, target_is_directory, eol_translate, rewrite, token_translate);
		if (copy_ec != 0)
		{
			if (copy_ec != DGNDOS_USAGE_ERROR)
			{
				fprintf(stderr, "%s: error %d copying '%s'\n", argv[0], copy_ec, argv[i]);
			}
			if (ec == 0)
			{
				ec = copy_ec;
			}
		}
	}

	return (ec);
}

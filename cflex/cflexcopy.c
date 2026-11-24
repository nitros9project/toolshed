/********************************************************************
 * cflexcopy.c - Copy utility for FLEX
 ********************************************************************/
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <flexpath.h>
#include <toolshed.h>
#include <util.h>

#include "cflex.h"




/* Help message */
static char const *const helpMessage[] = {
	"Syntax: copy {[<opts>]} <srcfile> {[<...>]} <target> {[<opts>]}\n",
	"Usage:  Copy one or more files to a target file or directory.\n",
	"Options:\n",
	"     -l         translate text: line ends, and FLEX's space compression\n",
	"     -r         rewrite if file exists\n",
	"Between FLEX disks a random file stays random, with its protection\n",
	"and date.  Copies are dated from SOURCE_DATE_EPOCH if it is set.\n",
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


/* Copy a text file, changing the form of its text where only one end is
   FLEX.  Text on a FLEX disk ends lines with CR, compresses spaces and is
   padded with nuls. */
static error_code copy_translating_text(char *source, char *destination, int rewrite, int *handled)
{
	coco_path_id path, destpath;
	coco_file_stat fstat, source_stat;
	_path_type source_type, destination_type;
	u_char *buffer = NULL, *result = NULL;
	u_int size = 0, result_size = 0;
	error_code ec;

	*handled = 0;
	ec = _coco_identify_image(source, &source_type);
	if (ec == 0)
	{
		ec = _coco_identify_image(destination, &destination_type);
	}
	if (ec != 0 || (source_type == FLEX) == (destination_type == FLEX))
	{
		return 0;
	}
	*handled = 1;

	ec = _coco_open_read_whole_file(&path, source, FAM_READ, &buffer, &size);
	if (path == NULL)
	{
		return ec;
	}
	memset(&source_stat, 0, sizeof(source_stat));
	_coco_gs_fd(path, &source_stat);
	_coco_close(path);

	if (ec == 0 && source_type == FLEX)
	{
		ec = _flex_text_to_native(buffer, size, &result, &result_size);
		if (ec == 0 && destination_type != NATIVE)
		{
			char *translated;

			NativeToCoCo((char *) result, result_size, &translated, &result_size);
			free(result);
			result = (u_char *) translated;
		}
	}
	else if (ec == 0)
	{
		u_char *text = buffer;
		u_int text_size = size;
		char *translated = NULL;

		if (source_type != NATIVE)
		{
			CoCoToNative((char *) buffer, size, &translated, &text_size);
			text = (u_char *) translated;
		}
		ec = _flex_text_from_native(text, text_size, &result, &result_size);
		free(translated);
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
		error_code close_ec;

		if (result_size != 0)
		{
			ec = _coco_write(destpath, result, &result_size);
		}
		close_ec = _coco_close(destpath);
		if (ec == 0)
		{
			ec = close_ec;
		}
	}
	if (ec == 0)
	{
		TSSetCopiedMetadata(destination, &source_stat);
	}

	free(result);
	return ec;
}


/* Read the whole of a file's data. */
static error_code read_flex_file(flex_path_id path, u_int size, u_char **data)
{
	u_int got = size;
	error_code ec;

	*data = malloc(size ? size : 1);
	if (*data == NULL)
	{
		return EOS_OM;
	}
	ec = _flex_read(path, *data, &got);
	if (ec == EOS_EOF && size == 0)
	{
		ec = 0;
	}
	if (ec == 0 && got != size)
	{
		ec = EOS_READ;
	}
	return ec;
}


/* Copy a file from one FLEX disk to another as it is: a random file stays
   random, and it keeps its protection and date.  As other copies do, the
   date is SOURCE_DATE_EPOCH if that is set.  *handled says whether both ends
   were FLEX files, which only this can copy. */
static error_code copy_flex_file(char *source, char *destination, int rewrite, int *handled)
{
	flex_path_id in = NULL, out = NULL;
	flex_file_stat stat;
	_path_type source_type, destination_type;
	u_char *data = NULL;
	u_int size;
	error_code ec, close_ec;

	*handled = 0;
	ec = _coco_identify_image(source, &source_type);
	if (ec == 0)
	{
		ec = _coco_identify_image(destination, &destination_type);
	}
	if (ec != 0 || source_type != FLEX || destination_type != FLEX)
	{
		return 0;
	}

	ec = _flex_open(&in, source, FAM_READ);
	if (ec == 0 && in->israw)
	{
		/* The whole disk, which isn't a file to copy here. */
		_flex_close(in);
		return 0;
	}
	*handled = 1;
	if (ec != 0)
	{
		return ec;
	}
	ec = _flex_gs_fd(in, &stat);
	size = stat.file_size;
	if (ec == 0)
	{
		ec = read_flex_file(in, size, &data);
	}
	close_ec = _flex_close(in);
	if (ec == 0)
	{
		ec = close_ec;
	}

	if (ec == 0)
	{
		ec = _flex_create(&out, destination, rewrite ? FAM_WRITE : FAM_WRITE | FAM_NOCREATE);
	}
	if (ec == 0 && stat.random)
	{
		ec = _flex_ss_random(out);
	}
	if (ec == 0 && size != 0)
	{
		u_int written = size;

		ec = _flex_write(out, data, &written);
	}
	if (ec == 0)
	{
		_flex_reproducible_date(&stat);
		ec = _flex_ss_fd(out, &stat);
	}
	if (out != NULL)
	{
		close_ec = _flex_close(out);
		if (ec == 0)
		{
			ec = close_ec;
		}
		/* A copy that failed leaves nothing behind. */
		if (ec != 0)
		{
			_coco_delete(destination);
		}
	}

	free(data);
	return ec;
}


static error_code copy_file(char *source, char *target, int target_is_directory, int eol_translate, int rewrite)
{
	static char buffer[32768];
	char destination[1024];
	int length, handled;
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
		return CFLEX_USAGE_ERROR;
	}

	handled = 0;
	ec = eol_translate ? copy_translating_text(source, destination, rewrite, &handled) : 0;
	if (!handled)
	{
		ec = copy_flex_file(source, destination, rewrite, &handled);
	}
	if (!handled)
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


int cflexcopy(int argc, char *argv[])
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
					return (CFLEX_USAGE_ERROR);
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
		return (CFLEX_USAGE_ERROR);
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
			if (copy_ec != CFLEX_USAGE_ERROR)
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

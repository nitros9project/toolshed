/********************************************************************
 * tsdsave.c - Copy the contents of a directory or device
 *
 * dsave doesn't copy anything itself.  It writes the commands that would, and
 * runs them if asked.
 ********************************************************************/
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <sys/wait.h>
#endif

#include <cocotypes.h>
#include <cocopath.h>
#include <toolshed.h>

static int singlequotes;


/* This function escapes the single quote character for later passing to a shell */
static char *EscapePart(char *dest, const char *src)
{
	while (*src != 0)
	{
		if (*src == '\'' && singlequotes)
		{
			*dest++ = '\'';
			*dest++ = '\\';
			*dest++ = '\'';
		}

		*dest++ = *src++;
	}

	*dest = '\0';

	return dest;
}


static char *ShellEscapePath(const char *source, const char *src_path_separator,
			     const u_char * direntry_name_buffer)
{
	char *buffer = malloc((strlen(source) + strlen(src_path_separator) +
			       strlen((const char *) direntry_name_buffer)) * 4 + 1);

	if (buffer != NULL)
	{
		char *p;

		p = EscapePart(buffer, source);
		p = EscapePart(p, src_path_separator);
		EscapePart(p, (const char *) direntry_name_buffer);
	}

	return buffer;
}


/* Run a command, and return its exit status rather than how it is packed in
   the wait status, which would not survive being an exit status itself. */
static error_code run_command(const char *command)
{
	int status = system(command);

#ifndef WIN32
	if (status != -1 && WIFEXITED(status))
	{
		return WEXITSTATUS(status);
	}
#endif

	return status;
}


/* Show a command, and run it if asked. */
static error_code issue_command(const ts_dsave_options *options, const char *command)
{
	puts(command);

	/* What it runs may print too, and must come after this. */
	fflush(stdout);

	return options->execute ? run_command(command) : 0;
}


static error_code do_dsave(const ts_dsave_options *options, const char *runner, char *source, char *target)
{
	error_code ec = 0;
	coco_dir_entry dirent;
	char command[2048];
	char sourcePathList[1024];
	coco_path_id sourcePath;
	char *src_path_separator, *dst_path_separator;
	const char *quote = singlequotes ? "'" : "\"";
	_path_type type;

	ec = _coco_open(&sourcePath, source, FAM_DIR | FAM_READ);
	if (ec != 0)
	{
		return (ec);
	}

	src_path_separator = "/";

	/* avoid leading slash in path on source image */
	if (source[strlen(source) - 1] == ',')
		src_path_separator = "";

	/* avoid double slash if source is a folder with trailing slash */
	if (strlen(source) > 1 && source[strlen(source) - 1] == '/')
		source[strlen(source) - 1] = '\0';

	_coco_identify_image(target, &type);

	if ((type == OS9) || (type == NATIVE))
		dst_path_separator = "/";
	else
		dst_path_separator = "";

	/* avoid leading slash in path on target image */
	if (target[strlen(target) - 1] == ',')
		dst_path_separator = "";

	/* avoid double slash if source is a folder with trailing slash */
	if (strlen(target) > 1 && target[strlen(target) - 1] == '/')
		target[strlen(target) - 1] = '\0';

	ec = 0;

	while (ec == 0 && _coco_readdir(sourcePath, &dirent) == 0)
	{
		u_char direntry_name_buffer[255];
		_coco_ncpy_name(&dirent, direntry_name_buffer, 255);

		if ((direntry_name_buffer[0] != '\0')
		    && (direntry_name_buffer[0] != 255)
		    && (strncmp((const char *) direntry_name_buffer, ".", 2)
			!= 0)
		    && (strncmp((const char *) direntry_name_buffer, "..", 3)
			!= 0))
		{
			coco_path_id filePath;
			int isdir = 1;
			int length;

			length = snprintf(sourcePathList, sizeof(sourcePathList), "%s%s%s", source,
					  src_path_separator, direntry_name_buffer);
			if (length < 0 || (size_t) length >= sizeof(sourcePathList))
			{
				ec = EOS_BPNAM;
				break;
			}

			ec = _coco_open(&filePath, sourcePathList,
					FAM_DIR | FAM_READ);
			if (ec != 0)
			{
				isdir = 0;

				ec = _coco_open(&filePath, sourcePathList,
						FAM_READ);
				if (ec != 0)
				{
					break;
				}
			}

			_coco_close(filePath);

			if (isdir == 1)
			{
				char newTarget[1024];

				/* We've encountered a directory */

				if (options->no_directories)
				{
					fprintf(stderr, "dsave: can't copy directory '%s': this filesystem has no directories\n",
						sourcePathList);
					ec = EOS_BPNAM;
					break;
				}

				/* 1. make directory on target IF target path is relative */
				if (strcmp(target, "/") == 0)
				{
					length = snprintf(newTarget, sizeof(newTarget), "%s%s",
							  dst_path_separator, direntry_name_buffer);
				}
				else
				{
					length = snprintf(newTarget, sizeof(newTarget), "%s%s%s", target,
							  dst_path_separator, direntry_name_buffer);
				}
				if (length < 0 || (size_t) length >= sizeof(newTarget))
				{
					ec = EOS_BPNAM;
					break;
				}

				/* 2. make directory on target */
				length = snprintf(command, sizeof(command), "%s makdir %s%s%s",
						  runner, quote, newTarget, quote);
				if (length < 0 || (size_t) length >= sizeof(command))
				{
					ec = EOS_BPNAM;
					break;
				}

				ec = issue_command(options, command);
				if (ec != 0)
				{
					break;
				}

				/* 3. call this function again */
				ec = do_dsave(options, runner, sourcePathList, newTarget);
			}
			else
			{
				/* We've encountered a file -- just copy */
				char opts[64], *escaped_source, *escaped_dest;

				opts[0] = 0;

				if (options->rewrite)
				{
					strcat(opts, " -r");
				}

				if (options->eol_translate)
				{
					strcat(opts, " -l");
				}

				if (options->token_translate)
				{
					strcat(opts, " -t");
				}

				/* Only some copy commands have this. */
				if (options->copy_buffer_size > 0)
				{
					sprintf(opts + strlen(opts), " -b=%u", options->copy_buffer_size);
				}

				escaped_source = ShellEscapePath(source, src_path_separator, direntry_name_buffer);
				escaped_dest = ShellEscapePath(target, dst_path_separator, direntry_name_buffer);

				if (escaped_source == NULL || escaped_dest == NULL)
				{
					ec = EOS_OM;
				}
				else
				{
					length = snprintf(command, sizeof(command), "%s copy %s%s%s %s%s%s%s",
							  runner, quote, escaped_source, quote,
							  quote, escaped_dest, quote, opts);
					if (length < 0 || (size_t) length >= sizeof(command))
					{
						ec = EOS_BPNAM;
					}
					else
					{
						ec = issue_command(options, command);
					}
				}

				free(escaped_source);
				free(escaped_dest);
			}
		}
	}

	_coco_close(sourcePath);

	return (ec);
}


/*
 * TSDSave()
 *
 * Copy the contents of a directory or device, using the copy commands of
 * options->executable, or of options->program as the shell finds it.
 */
error_code TSDSave(const ts_dsave_options *options, char *source, char *target)
{
	char runner[1024];

#ifdef WIN32
	/* Use single quotes only if a Posix-like shell seems to be in use */
	singlequotes = getenv("SHELL") != NULL;
#else
	singlequotes = 1;
#endif

	if (options->executable == NULL)
	{
		if (strlen(options->program) >= sizeof(runner))
		{
			return EOS_BPNAM;
		}
		strcpy(runner, options->program);
	}
	else
	{
		char *p;

		/* Each character could become four, and it needs quotes. */
		if (strlen(options->executable) * 4 + 3 > sizeof(runner))
		{
			return EOS_BPNAM;
		}
		p = runner;
		*p++ = singlequotes ? '\'' : '"';
		p = EscapePart(p, options->executable);
		*p++ = singlequotes ? '\'' : '"';
		*p = '\0';
	}

	return do_dsave(options, runner, source, target);
}

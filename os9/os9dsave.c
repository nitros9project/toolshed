/********************************************************************
 * os9dsave.c - Copy utility for OS-9 filesystem
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#ifndef WIN32
#include <sys/wait.h>
#endif

/* globals */
u_int buffer_size = 32768;
int singlequotes;

error_code do_dsave(char *pgmname, char *source, char *target, int execute,
		    int buffsize, int rewrite, int eoltranslate, int basicdetoken);
static char *ShellEscapePath(char *source, char *src_path_separator,
			     u_char * direntry_name_buffer);
static char *EscapePart(char *dest, char *src);

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dsave {[<opts>]} {[<source>]} <target> {[<opts>]}\n",
	"Usage: Copy the contents of a directory or device.\n",
	"Options:\n",
	"     -b=size    size of copy buffer in bytes or K-bytes\n",
	"     -e         actually execute commands\n",
	"     -l         perform end of line translation on copy\n",
	"     -r         force rewrite on copy\n",
	NULL
};


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


int os9dsave(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL, *q;
	int i;
	int count = 0;
	int rewrite = 0;
	int execute = 0;
	int eoltranslate = 0;
	int basicdetoken = 0;
	char *target = NULL;
	char *source = NULL;

#ifdef WIN32
	/* Use single quotes only if a Posix-like shell seems to be in use */
	singlequotes = getenv("SHELL") != NULL;
#else
	singlequotes = 1;
#endif
	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'b':
					if (*(++p) == '=')
					{
						p++;
					}
					q = p + strlen(p) - 1;
					if (toupper((unsigned char)*q) == 'K')
					{
						*q = '\0';
						buffer_size = atoi(p) * 1024;
					}
					else
					{
						buffer_size = atoi(p);
					}
					p = q;
					break;

				case 'r':
					rewrite = 1;
					break;

				case 'l':
					eoltranslate = 1;
					break;

				case 'e':
					execute = 1;
					break;

				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (1);
				}
			}
		}
	}

	/* Count non option arguments */
	for (i = 1, count = 0; i < argc; i++)
	{
		if (argv[i] == NULL)
		{
			continue;
		}

		if (argv[i][0] == '-')
		{
			continue;
		}

		if (source == NULL)
		{
			source = argv[i];
		}
		else if (target == NULL)
		{
			target = argv[i];
		}
		count++;
	}

	if (count < 1 || count > 2)
	{
		show_help(helpMessage);
		return (1);
	}

	/* if target is NULL, then source is really . and target is source */
	if (target == NULL)
	{
		target = source;
		source = ".";
	}

	/* do dsave */
	/* TODO: Possibly use original argv[0] here? */
	ec = do_dsave("os9", source, target, execute, buffer_size, rewrite,
		      eoltranslate, basicdetoken);
	if (ec != 0)
	{
		fprintf(stderr, "%s: error %d encountered during dsave\n",
			argv[0], ec);
	}

	return (ec);
}

/* this function is also used by the decb utility */
error_code do_dsave(char *pgmname, char *source, char *target, int execute,
		    int buffer_size, int rewrite, int eoltranslate, int basicdetoken)
{
	error_code ec = 0;
	static int level = 0;
	coco_dir_entry dirent;
	char command[1024];
	char sourcePathList[1024];
	coco_path_id sourcePath;
	char *src_path_separator, *dst_path_separator;
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

	while (_coco_readdir(sourcePath, &dirent) == 0)
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

			sprintf(sourcePathList, "%s%s%s", source,
				src_path_separator, direntry_name_buffer);

			ec = _coco_open(&filePath, sourcePathList,
					FAM_DIR | FAM_READ);
			if (ec != 0)
			{
				isdir = 0;

				ec = _coco_open(&filePath, sourcePathList,
						FAM_READ);
				if (ec != 0)
				{
					_coco_close(sourcePath);

					return (ec);
				}
			}

			_coco_close(filePath);

			if (isdir == 1)
			{
				char newTarget[512];

				/* We've encountered a directory */
				newTarget[0] = '\0';

				/* 1. increment level indicator */
				level++;

				/* 2. make directory on target IF target path is relative */
				if (strcmp(target, "/") == 0)
				{
					sprintf(newTarget, "%s%s",
						dst_path_separator,
						direntry_name_buffer);
				}
				else
				{
					sprintf(newTarget, "%s%s%s", target,
						dst_path_separator,
						direntry_name_buffer);
				}

				/* 3. make directory on target */
				if (singlequotes)
				{
					snprintf(command, sizeof(command),
						 "%s makdir '%s'", pgmname,
						 newTarget);
				}
				else
				{
					snprintf(command, sizeof(command),
						 "%s makdir \"%s\"", pgmname,
						 newTarget);
				}

				puts(command);
				if (execute)
				{
					ec = run_command(command);
					if (ec != 0)
					{
						_coco_close(sourcePath);

						return (ec);
					}
				}

				/* 4. call this function again */
				ec = do_dsave(pgmname, sourcePathList, newTarget,
					      execute, buffer_size, rewrite,
					      eoltranslate, basicdetoken);
				if (ec != 0)
				{
					_coco_close(sourcePath);

					return (ec);
				}

				/* 5. decrement level indicator */
				level--;
			}
			else
			{
				/* We've encountered a file -- just copy */
				char ropt[6], bopt[32], *escaped_source,
					*escaped_dest;

				ropt[0] = 0;
				bopt[0] = 0;

				/* not applicable for decb copy. TODO: search in pgmname if taken from argv */
				if (strcmp(pgmname, "os9") == 0
				    && buffer_size > 0)
				{
					sprintf(bopt, "-b=%d", buffer_size);
				}

				if (rewrite > 0)
				{
					strcat(ropt, "-r");
				}

				if (eoltranslate > 0)
				{
					if (*ropt)
						strcat(ropt, " ");
					strcat(ropt, "-l");
				}

				if (basicdetoken > 0)
				{
					if (*ropt)
						strcat(ropt, " ");
					strcat(ropt, "-t");
				}

				escaped_source =
					ShellEscapePath(source,
							src_path_separator,
							direntry_name_buffer);
				escaped_dest =
					ShellEscapePath(target,
							dst_path_separator,
							direntry_name_buffer);

				if (singlequotes)
				{
					snprintf(command, sizeof(command),
						 "%s copy '%s' '%s' %s %s",
						 pgmname, escaped_source,
						 escaped_dest, ropt, bopt);
				}
				else
				{
					snprintf(command, sizeof(command),
						 "%s copy \"%s\" \"%s\" %s %s",
						 pgmname, escaped_source,
						 escaped_dest, ropt, bopt);
				}
				puts(command);
				if (execute)
				{
					ec = run_command(command);
					if (ec != 0)
					{
						_coco_close(sourcePath);

						return (ec);
					}
				}

				if (escaped_source != NULL)
				{
					free(escaped_source);
				}

				if (escaped_dest != NULL)
				{
					free(escaped_dest);
				}
			}
		}
	}

	/* presumably used for debugging */
	(void) level;

	_coco_close(sourcePath);

	return (ec);
}

static char *ShellEscapePath(char *source, char *src_path_separator,
			     u_char * direntry_name_buffer)
{
	char *buffer =
		malloc((strlen(source) + strlen(src_path_separator) +
			strlen((const char *) direntry_name_buffer) * 4) + 1);

	if (buffer != NULL)
	{
		char *p;

		p = EscapePart(buffer, source);
		p = EscapePart(p, src_path_separator);
		p = EscapePart(p, (char *) direntry_name_buffer);
	}

	return buffer;
}

/* This function escapes the single quote character for later passing to a shell */
static char *EscapePart(char *dest, char *src)
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

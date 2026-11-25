/********************************************************************
 * toolshed_ql.c - Run ToolShed's listings for the Quick Look extension
 *
 * The commands print to stdout and stderr, so while one runs both are
 * pointed at temporary files.  Only one command runs at a time, as the
 * file descriptors belong to the whole process.
 ********************************************************************/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cocopath.h>
#include <diskimage.h>

#include "toolshed_ql.h"

int cocoinfo(int argc, char *argv[]);
int os9dir(int argc, char *argv[]);
int decbdir(int argc, char *argv[]);
int dgndosdir(int argc, char *argv[]);
int cflexdir(int argc, char *argv[]);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


int tsql_detect(const char *path, unsigned int *filesystems)
{
	diskimage_format format = DISKIMAGE_UNKNOWN;
	u_int found = 0;
	error_code ec;

	pthread_mutex_lock(&lock);
	ec = _diskimage_identify(path, &format);
	if (ec == 0 && format == DISKIMAGE_UNKNOWN)
	{
		ec = EOS_BPNAM;
	}
	if (ec == 0)
	{
		ec = _coco_detect_filesystems(path, &found);
	}
	pthread_mutex_unlock(&lock);

	*filesystems = found;

	return ec;
}


/* Read all of a file from the start, as a string. */
static char *read_all(FILE *file)
{
	long length;
	char *text;
	size_t got;

	if (fflush(file) != 0 || fseek(file, 0, SEEK_END) != 0)
	{
		return NULL;
	}
	length = ftell(file);
	if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
	{
		return NULL;
	}

	text = malloc((size_t)length + 1);
	if (text == NULL)
	{
		return NULL;
	}
	got = fread(text, 1, (size_t)length, file);
	text[got] = '\0';

	return text;
}


/* Point a file descriptor at a file, and say where it pointed before. */
static int redirect(int fd, FILE *file)
{
	int saved = dup(fd);

	if (saved < 0)
	{
		return -1;
	}
	if (dup2(fileno(file), fd) < 0)
	{
		close(saved);
		return -1;
	}

	return saved;
}


static void restore(int fd, int saved)
{
	dup2(saved, fd);
	close(saved);
}


static int run_command(tsql_command command, char *pathlist)
{
	char name_info[] = "info", name_dir[] = "dir", option[] = "-er";
	char *argv[4] = { NULL, NULL, NULL, NULL };
	int argc = 0;

	switch (command)
	{
	case TSQL_INFO:
		argv[argc++] = name_info;
		argv[argc++] = pathlist;
		return cocoinfo(argc, argv);

	case TSQL_DIR_OS9:
		argv[argc++] = name_dir;
		argv[argc++] = option;
		argv[argc++] = pathlist;
		return os9dir(argc, argv);

	case TSQL_DIR_DECB:
		argv[argc++] = name_dir;
		argv[argc++] = pathlist;
		return decbdir(argc, argv);

	case TSQL_DIR_DRAGONDOS:
		argv[argc++] = name_dir;
		argv[argc++] = pathlist;
		return dgndosdir(argc, argv);

	case TSQL_DIR_FLEX:
		argv[argc++] = name_dir;
		argv[argc++] = pathlist;
		return cflexdir(argc, argv);
	}

	return -1;
}


char *tsql_run(tsql_command command, const char *path, char **errors, int *status)
{
	FILE *out = NULL, *err = NULL;
	int saved_out = -1, saved_err = -1;
	char *pathlist, *text = NULL;
	size_t length = strlen(path);

	*errors = NULL;
	*status = -1;

	/* The listings take an <image,> pathlist; info takes the image. */
	pathlist = malloc(length + 2);
	if (pathlist == NULL)
	{
		return NULL;
	}
	memcpy(pathlist, path, length);
	pathlist[length] = command == TSQL_INFO ? '\0' : ',';
	pathlist[length + 1] = '\0';

	pthread_mutex_lock(&lock);

	out = tmpfile();
	err = tmpfile();
	if (out == NULL || err == NULL)
	{
		goto done;
	}

	fflush(stdout);
	fflush(stderr);
	saved_out = redirect(STDOUT_FILENO, out);
	saved_err = redirect(STDERR_FILENO, err);
	if (saved_out >= 0 && saved_err >= 0)
	{
		*status = run_command(command, pathlist);
	}
	fflush(stdout);
	fflush(stderr);
	if (saved_out >= 0)
	{
		restore(STDOUT_FILENO, saved_out);
	}
	if (saved_err >= 0)
	{
		restore(STDERR_FILENO, saved_err);
	}
	if (saved_out < 0 || saved_err < 0)
	{
		goto done;
	}

	text = read_all(out);
	*errors = read_all(err);
	if (*errors != NULL && (*errors)[0] == '\0')
	{
		free(*errors);
		*errors = NULL;
	}

done:
	if (out != NULL)
	{
		fclose(out);
	}
	if (err != NULL)
	{
		fclose(err);
	}
	pthread_mutex_unlock(&lock);
	free(pathlist);

	return text;
}

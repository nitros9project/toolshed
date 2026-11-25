/*
 * The POSIX calls the tests use, for Windows.
 *
 * The tests run commands written for a POSIX shell ("> /dev/null 2>&1",
 * "cp", "cmp", "PATH=... command").  On Windows, system() and popen() would
 * hand them to cmd.exe, so here they are handed to sh instead, which an
 * MSYS2 build has on its PATH.  Exit statuses come back in the same form
 * as waitpid() gives them, so WIFEXITED() and WEXITSTATUS() work on both.
 *
 * On other systems this only includes the usual headers.
 */
#ifndef TESTPOSIX_H
#define TESTPOSIX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Windows programs end the lines they print with CR LF.  Make text that a
   command printed end its lines with LF alone, so it compares the same. */
static inline void test_text_output(char *text)
{
#ifdef _WIN32
	char *in = text, *out = text;

	for (; *in != '\0'; in++)
	{
		if (in[0] != '\r' || in[1] != '\n')
		{
			*out++ = *in;
		}
	}
	*out = '\0';
#else
	(void) text;
#endif
}

#ifndef _WIN32

#include <unistd.h>
#include <sys/wait.h>

#else

#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

#ifndef WIFEXITED
#define WIFEXITED(status)	(((status) & 0x7f) == 0)
#define WEXITSTATUS(status)	(((status) >> 8) & 0xff)
#define WIFSIGNALED(status)	(((status) & 0x7f) != 0)
#endif

#define TEST_SPAWN_TIMED_OUT	-2

/* Add text to a command line as one argument, quoted the way the C runtime
   splits a command line back into arguments. */
static inline int test_quote_argument(char *line, size_t capacity, size_t *length, const char *text)
{
	size_t at = *length;
	const char *p;

#define TEST_PUT(c) do { if (at + 1 >= capacity) return -1; line[at++] = (c); } while (0)
	TEST_PUT('"');
	for (p = text; ; p++)
	{
		size_t backslashes = 0;

		while (*p == '\\')
		{
			backslashes++;
			p++;
		}
		if (*p == '\0')
		{
			/* Doubled, so the closing quote stays a quote. */
			while (backslashes-- > 0)
			{
				TEST_PUT('\\');
				TEST_PUT('\\');
			}
			break;
		}
		if (*p == '"')
		{
			while (backslashes-- > 0)
			{
				TEST_PUT('\\');
				TEST_PUT('\\');
			}
			TEST_PUT('\\');
		}
		else
		{
			while (backslashes-- > 0)
			{
				TEST_PUT('\\');
			}
		}
		TEST_PUT(*p);
	}
	TEST_PUT('"');
#undef TEST_PUT
	line[at] = '\0';
	*length = at;
	return 0;
}

/* Run "sh -c command", with its output going to output (or to ours when it
   is INVALID_HANDLE_VALUE), for at most milliseconds.  Everything it starts
   is in one job, so a time-out ends all of it.  The result is a status in
   waitpid() form, or TEST_SPAWN_TIMED_OUT. */
static inline int test_spawn(const char *command, HANDLE output, DWORD milliseconds)
{
	static char line[16384];
	size_t length;
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	HANDLE job;
	DWORD code = 0;
	int status;

	strcpy(line, "sh -c ");
	length = strlen(line);
	if (test_quote_argument(line, sizeof(line), &length, command) != 0)
	{
		return 127 << 8;
	}

	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = output != INVALID_HANDLE_VALUE ? output : GetStdHandle(STD_OUTPUT_HANDLE);
	startup.hStdError = output != INVALID_HANDLE_VALUE ? output : GetStdHandle(STD_ERROR_HANDLE);

	fflush(stdout);
	fflush(stderr);
	job = CreateJobObjectA(NULL, NULL);
	if (!CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &startup, &process))
	{
		if (job != NULL)
		{
			CloseHandle(job);
		}
		return 127 << 8;
	}
	if (job != NULL)
	{
		AssignProcessToJobObject(job, process.hProcess);
	}
	ResumeThread(process.hThread);

	if (WaitForSingleObject(process.hProcess, milliseconds) == WAIT_TIMEOUT)
	{
		if (job != NULL)
		{
			TerminateJobObject(job, 1);
		}
		TerminateProcess(process.hProcess, 1);
		WaitForSingleObject(process.hProcess, INFINITE);
		status = TEST_SPAWN_TIMED_OUT;
	}
	else
	{
		GetExitCodeProcess(process.hProcess, &code);
		/* An exception such as an access violation ends a program with
		   an NTSTATUS code, which is what a signal is elsewhere. */
		status = code >= 0xC0000000 ? 11 : (int) ((code & 0xff) << 8);
	}

	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	if (job != NULL)
	{
		CloseHandle(job);
	}
	return status;
}

static inline int test_system(const char *command)
{
	if (command == NULL)
	{
		return 1;
	}
	return test_spawn(command, INVALID_HANDLE_VALUE, INFINITE);
}

/* popen() for reading only: the command runs to the end, and its output is
   read back from a file. */
#define TEST_POPEN_OUTPUT	"testposix_popen.txt"
static int test_popen_status;

static inline FILE *test_popen(const char *command, const char *mode)
{
	static char line[16384];
	int length;

	if (strcmp(mode, "r") != 0)
	{
		return NULL;
	}
	length = snprintf(line, sizeof(line), "%s > " TEST_POPEN_OUTPUT, command);
	if (length < 0 || (size_t) length >= sizeof(line))
	{
		return NULL;
	}
	test_popen_status = test_system(line);
	return fopen(TEST_POPEN_OUTPUT, "rb");
}

static inline int test_pclose(FILE *file)
{
	fclose(file);
	remove(TEST_POPEN_OUTPUT);
	return test_popen_status;
}

static inline int test_setenv(const char *name, const char *value, int overwrite)
{
	if (!overwrite && getenv(name) != NULL)
	{
		return 0;
	}
	return _putenv_s(name, value);
}

static inline int test_unsetenv(const char *name)
{
	return _putenv_s(name, "");
}

static inline int test_truncate(const char *path, long long length)
{
	int fd = _open(path, _O_RDWR | _O_BINARY);
	int result;

	if (fd < 0)
	{
		return -1;
	}
	result = _chsize_s(fd, length) == 0 ? 0 : -1;
	_close(fd);
	return result;
}

/* Elsewhere a read-only file can be removed, as the tests clean up files
   that copies made read-only.  Windows refuses until it is writable. */
static inline int test_remove(const char *path)
{
	int result = remove(path);

	if (result != 0 && errno == EACCES)
	{
		_chmod(path, _S_IREAD | _S_IWRITE);
		result = remove(path);
	}
	return result;
}

/* MinGW's stdio.h names cmd.exe's versions popen and pclose. */
#undef popen
#undef pclose

#define system(command)		test_system(command)
#define popen(command, mode)	test_popen(command, mode)
#define pclose(file)		test_pclose(file)
#define truncate(path, length)	test_truncate(path, length)
#define remove(path)		test_remove(path)
#define setenv(name, value, overwrite)	test_setenv(name, value, overwrite)
#define unsetenv(name)		test_unsetenv(name)
#define mkdir(path, mode)	_mkdir(path)

#endif	/* _WIN32 */

#endif	/* TESTPOSIX_H */

/*
 * Run a command with a time limit, as tests of what must not hang need.
 *
 * A command that loops forever would stop the tests rather than fail one, so
 * it runs in its own process group, which is killed once the time is up.
 *
 * Include this after tinytest.h.
 */
#ifndef COMMANDRUN_H
#define COMMANDRUN_H

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define COMMAND_TIMED_OUT	-2	/* it ran out of time and was killed */
#define COMMAND_CRASHED		-1	/* a signal ended it, so it crashed */
#define COMMAND_OUTPUT		"commandrun_output.txt"


/* Run a shell command, with its output and errors going to output, for at
   most seconds.  The result is its exit status, or one of the above. */
static inline int run_command_timed(const char *command, int seconds, char *output, size_t capacity)
{
	pid_t pid;
	int status = 0;
	int ticks;
	FILE *fp;
	size_t got = 0;

	fflush(stdout);
	fflush(stderr);
	pid = fork();
	if (pid == 0)
	{
		static char line[8192];
		FILE *out;

		setpgid(0, 0);
		out = freopen(COMMAND_OUTPUT, "wb", stdout);
		if (out == NULL || dup2(fileno(stdout), fileno(stderr)) < 0)
		{
			_exit(127);
		}
		/* The shell is replaced by the command, so a signal that ends
		   the command isn't turned into an exit status by it. */
		snprintf(line, sizeof(line), "exec %s", command);
		execl("/bin/sh", "sh", "-c", line, (char *) NULL);
		_exit(127);
	}
	if (pid < 0)
	{
		return COMMAND_CRASHED;
	}

	for (ticks = 0; ticks < seconds * 100; ticks++)
	{
		pid_t done = waitpid(pid, &status, WNOHANG);

		if (done == pid)
		{
			break;
		}
		usleep(10000);
	}

	if (ticks == seconds * 100)
	{
		kill(-pid, SIGKILL);
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);
		status = COMMAND_TIMED_OUT;
	}
	else if (WIFSIGNALED(status))
	{
		status = COMMAND_CRASHED;
	}
	else
	{
		status = WEXITSTATUS(status);
	}

	if (output != NULL && capacity > 0)
	{
		fp = fopen(COMMAND_OUTPUT, "rb");
		if (fp != NULL)
		{
			got = fread(output, 1, capacity - 1, fp);
			fclose(fp);
		}
		output[got] = '\0';
	}
	remove(COMMAND_OUTPUT);

	return status;
}

#endif	/* COMMANDRUN_H */

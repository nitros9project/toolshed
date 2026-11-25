/* Check that the executives and their commands do what their help says with
   options, and refuse the options they don't have. */
#include <string.h>
#include <toolshed.h>
#include "tinytest.h"
#include "commandrun.h"

#define OPTION_FILE	"test_option.txt"
#define OPTION_OS9	"test_option.dsk"
#define OPTION_DECB	"test_option_decb.dsk"


/* Run a tool with arguments, for at most 20 seconds.  The result is its exit
   status, or COMMAND_CRASHED or COMMAND_TIMED_OUT. */
static int run_tool(const char *tool, const char *arguments, char *output, size_t capacity)
{
	char command[256];
	int length;

	length = snprintf(command, sizeof(command), "../%s/%s %s", tool, tool, arguments);
	if (length < 0 || (size_t) length >= sizeof(command))
	{
		return COMMAND_CRASHED;
	}

	return run_command_timed(command, 20, output, capacity);
}


static int image_exists(const char *pathname)
{
	FILE *fp = fopen(pathname, "rb");

	if (fp == NULL)
	{
		return 0;
	}
	fclose(fp);

	return 1;
}


/* Every command refuses an option it doesn't have, says so, and fails. */
void test_unknown_option_fails()
{
	static const struct
	{
		const char *tool;
		const char *commands;
	} tools[] = {
		{"cecb", "dir fstat dump dumpblock list bulkerase copy leader"},
		{"decb", "attr binbust copy dir dsave dskini dump free fstat hdbconv info kill list rename reveal"},
		{"os9", "attr cmp copy dcheck del deldir dir dsave dump free fstat gen id ident info list makdir modbust padrom rename reveal"},
		{"dgndos", "attr copy dir dsave dskinit dump free fstat info kill list rename reveal"},
		{"cflex", "attr copy dir dsave dskinit dump free fstat gen info kill list rename"},
	};
	char output[1024];
	char commands[256];
	char arguments[64];
	char *command;
	size_t i;
	int status;
	int length;

	for (i = 0; i < sizeof(tools) / sizeof(tools[0]); i++)
	{
		strncpy(commands, tools[i].commands, sizeof(commands) - 1);
		commands[sizeof(commands) - 1] = '\0';
		for (command = strtok(commands, " "); command != NULL; command = strtok(NULL, " "))
		{
			length = snprintf(arguments, sizeof(arguments), "%s -Q " OPTION_FILE, command);
			ASSERT_EQUALS(1, length > 0 && (size_t) length < sizeof(arguments));
			status = run_tool(tools[i].tool, arguments, output, sizeof(output));
			if (status <= 0 || strstr(output, "unknown option") == NULL)
			{
				printf("%s %s: status %d, output: %s\n", tools[i].tool, arguments, status, output);
			}
			ASSERT_EQUALS(1, status > 0);
			ASSERT_EQUALS(1, strstr(output, "unknown option") != NULL);
		}
	}
	remove(OPTION_FILE);
}


/* The executives refuse options they don't have, and cecb's options that
   take a value say so when it is missing, rather than crashing. */
void test_executive_options()
{
	static const struct
	{
		const char *tool;
		const char *arguments;
		const char *message;
	} refused[] = {
		{"cecb", "-Q dir", "unknown option 'Q'"},
		{"cecb", "-r", "-r needs a value"},
		{"cecb", "-t", "-t needs a value"},
		{"cecb", "-n", "-n needs a value"},
		{"cecb", "-s", "-s needs a value"},
		{"cecb", "-zs", "-s needs a value"},
		{"decb", "-Q dir", "unknown option 'Q'"},
		{"decb", "-g0 dir", "-g wants a granule count from 1 to 192"},
		{"decb", "-g193 dir", "-g wants a granule count from 1 to 192"},
		{"os9", "-Q dir", "unknown option 'Q'"},
		{"dgndos", "-Q dir", "unknown option 'Q'"},
		{"cflex", "-Q dir", "unknown option 'Q'"},
	};
	char output[2048];
	size_t i;
	int status;

	for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		status = run_tool(refused[i].tool, refused[i].arguments, output, sizeof(output));
		if (status <= 0 || strstr(output, refused[i].message) == NULL)
		{
			printf("%s %s: status %d, output: %s\n", refused[i].tool, refused[i].arguments, status, output);
		}
		ASSERT_EQUALS(1, status > 0);
		ASSERT_EQUALS(1, strstr(output, refused[i].message) != NULL);
	}

	/* A value can follow the letter or be the next argument. */
	status = run_tool("cecb", "-r 1.85 -s0 -z dir", output, sizeof(output));
	ASSERT_EQUALS(0, status);
	status = run_tool("cecb", "-r1.85 dir", output, sizeof(output));
	ASSERT_EQUALS(0, status);
}


/* What os9 attr shows for the file after the options are applied to a new
   copy of it, or "" if a command failed. */
static const char *os9_attr_after(const char *options, int *status)
{
	static char output[256];
	char arguments[128];
	int length;

	run_tool("os9", "del " OPTION_OS9 ",F", output, sizeof(output));
	*status = run_tool("os9", "copy " OPTION_FILE " " OPTION_OS9 ",F", output, sizeof(output));
	if (*status != 0)
	{
		return "";
	}

	/* A copy's permissions come from the host file, so start the same way
	   everywhere: ----r-wr. */
	*status = run_tool("os9", "attr -q -w -r -pr -ns -ne -npe -npw " OPTION_OS9 ",F", output, sizeof(output));
	if (*status != 0)
	{
		return "";
	}

	length = snprintf(arguments, sizeof(arguments), "attr -q %s " OPTION_OS9 ",F", options);
	if (length < 0 || (size_t) length >= sizeof(arguments))
	{
		*status = COMMAND_CRASHED;
		return "";
	}
	*status = run_tool("os9", arguments, output, sizeof(output));

	if (run_tool("os9", "attr " OPTION_OS9 ",F", output, sizeof(output)) != 0)
	{
		return "";
	}
	output[strcspn(output, "\n")] = '\0';

	return output;
}


/* Each os9 attr option changes only its own permission.  -p and -n used to
   leave their letter to be read again, so -pe also set owner execute. */
void test_os9_attr_options()
{
	static const struct
	{
		const char *options;
		const char *attributes;
	} cases[] = {
		{"", "----r-wr"},
		{"-pe", "--e-r-wr"},
		{"-pw", "---wr-wr"},
		{"-npr", "------wr"},
		{"-pe -npe", "----r-wr"},
		{"-e -npe", "----rewr"},
		{"-npe", "----r-wr"},
		{"-ne", "----r-wr"},
		{"-nw", "----r--r"},
		{"-s", "-s--r-wr"},
		{"-s -ns", "----r-wr"},
		{"-ewpr", "----rewr"},
		{"-pwe", "---wrewr"},
		{"-nwnpr", "-------r"},
	};
	static const char *const refused[] = { "-px", "-p", "-n", "-np", "-nx", "-npx" };
	FILE *fp;
	size_t i;
	int status;
	const char *attributes;
	char output[512];

	fp = fopen(OPTION_FILE, "wb");
	ASSERT_EQUALS(1, fp != NULL);
	fputs("hello\n", fp);
	fclose(fp);
	remove(OPTION_OS9);
	ASSERT_EQUALS(0, run_tool("os9", "format -q " OPTION_OS9, output, sizeof(output)));

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		attributes = os9_attr_after(cases[i].options, &status);
		if (strcmp(attributes, cases[i].attributes) != 0)
		{
			printf("attr %s: got '%s', want '%s'\n", cases[i].options, attributes, cases[i].attributes);
		}
		ASSERT_EQUALS(0, status);
		ASSERT_STRING_EQUALS(cases[i].attributes, attributes);
	}

	/* A permission letter that is missing or wrong is refused, and nothing
	   changes. */
	for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		attributes = os9_attr_after(refused[i], &status);
		ASSERT_EQUALS(EOS_IA, status);
		ASSERT_STRING_EQUALS("----r-wr", attributes);
	}

	/* The error is reported as an illegal argument. */
	status = run_tool("os9", "attr -px " OPTION_OS9 ",F", output, sizeof(output));
	ASSERT_EQUALS(EOS_IA, status);
	ASSERT_EQUALS(1, strstr(output, "unknown option 'px'") != NULL);
	ASSERT_EQUALS(1, strstr(output, "illegal argument") != NULL);

	remove(OPTION_OS9);
	remove(OPTION_FILE);
}


/* Whether os9 attr's output matches a pattern, where ? matches anything. */
static int attributes_match(const char *pattern, const char *attributes)
{
	if (strlen(pattern) != strlen(attributes))
	{
		return 0;
	}
	for (; *pattern != '\0'; pattern++, attributes++)
	{
		if (*pattern != '?' && *pattern != *attributes)
		{
			return 0;
		}
	}

	return 1;
}


/* os9 copy -a= takes the same permissions, and refuses ones it doesn't know. */
void test_os9_copy_attribute_option()
{
	static const struct
	{
		const char *option;
		const char *attributes;
	} cases[] = {
		/* Public read comes from the host file unless it is given. */
		{"-a=pe", "--e-?-wr"},
		{"-a=ewpr", "----rewr"},
		{"-a=npr", "------wr"},
	};
	static const char *const refused[] = { "-a=px", "-a=p", "-a=x", "-a=nx", "-a=npx" };
	char output[512];
	char arguments[128];
	FILE *fp;
	size_t i;
	int status;
	int length;

	fp = fopen(OPTION_FILE, "wb");
	ASSERT_EQUALS(1, fp != NULL);
	fputs("hello\n", fp);
	fclose(fp);
	remove(OPTION_OS9);
	ASSERT_EQUALS(0, run_tool("os9", "format -q " OPTION_OS9, output, sizeof(output)));

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		length = snprintf(arguments, sizeof(arguments), "copy -r %s " OPTION_FILE " " OPTION_OS9 ",F",
				  cases[i].option);
		ASSERT_EQUALS(1, length > 0 && (size_t) length < sizeof(arguments));
		status = run_tool("os9", arguments, output, sizeof(output));
		ASSERT_EQUALS(0, status);
		status = run_tool("os9", "attr " OPTION_OS9 ",F", output, sizeof(output));
		ASSERT_EQUALS(0, status);
		output[strcspn(output, "\n")] = '\0';
		if (!attributes_match(cases[i].attributes, output))
		{
			printf("copy %s: got '%s', want '%s'\n", cases[i].option, output, cases[i].attributes);
		}
		ASSERT_EQUALS(1, attributes_match(cases[i].attributes, output));
	}

	for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		length = snprintf(arguments, sizeof(arguments), "copy %s " OPTION_FILE " " OPTION_OS9 ",G",
				  refused[i]);
		ASSERT_EQUALS(1, length > 0 && (size_t) length < sizeof(arguments));
		status = run_tool("os9", arguments, output, sizeof(output));
		ASSERT_EQUALS(EOS_IA, status);
		ASSERT_EQUALS(1, strstr(output, "unknown attribute") != NULL);
	}

	/* None of the refused copies made the file. */
	status = run_tool("os9", "attr " OPTION_OS9 ",G", output, sizeof(output));
	ASSERT_NEQUALS(0, status);

	remove(OPTION_OS9);
	remove(OPTION_FILE);
}


/* decb dskini -h needs at least one drive; -h0 used to make an empty image. */
void test_decb_dskini_hdb_drives()
{
	static const char *const refused[] = { "-h", "-h0", "-h-1" };
	char output[512];
	char arguments[64];
	size_t i;
	int status;
	int length;

	for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
	{
		remove(OPTION_DECB);
		length = snprintf(arguments, sizeof(arguments), "dskini %s " OPTION_DECB, refused[i]);
		ASSERT_EQUALS(1, length > 0 && (size_t) length < sizeof(arguments));
		status = run_tool("decb", arguments, output, sizeof(output));
		ASSERT_EQUALS(EOS_IA, status);
		ASSERT_EQUALS(1, strstr(output, "-h wants a number of HDB-DOS drives") != NULL);
		ASSERT_EQUALS(0, image_exists(OPTION_DECB));
	}

	remove(OPTION_DECB);
	status = run_tool("decb", "dskini -h1 " OPTION_DECB, output, sizeof(output));
	ASSERT_EQUALS(0, status);
	status = run_tool("decb", "dir " OPTION_DECB, output, sizeof(output));
	ASSERT_EQUALS(0, status);
	remove(OPTION_DECB);
}


int main()
{
	RUN(test_unknown_option_fails);
	RUN(test_executive_options);
	RUN(test_os9_attr_options);
	RUN(test_os9_copy_attribute_option);
	RUN(test_decb_dskini_hdb_drives);
	return TEST_REPORT();
}

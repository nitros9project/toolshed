/* Check the advertised short commands and executive help formatting. */
#include <string.h>
#include "tinytest.h"

#define OUTPUT_FILE "commandalias_output.txt"

static int run_command(const char *tool, const char *arguments,
		       char *output, size_t size)
{
	char command[512];
	FILE *file;
	size_t count;
	int result;

	snprintf(command, sizeof(command), "../%s/%s %s > %s 2>&1",
		 tool, tool, arguments, OUTPUT_FILE);
	result = system(command);
	file = fopen(OUTPUT_FILE, "rb");
	if (file == NULL)
	{
		output[0] = '\0';
		return -1;
	}
	count = fread(output, 1, size - 1, file);
	output[count] = '\0';
	fclose(file);
	remove(OUTPUT_FILE);
	return result;
}

static void check_commands(const char *tool, int expected_count)
{
	char help[8192], full_output[8192], short_output[8192];
	char names[32][32], aliases[32][3], arguments[64];
	char *line, *next;
	int result, full_result, short_result, count = 0, used, i;

	result = run_command(tool, "", help, sizeof(help));
	ASSERT_EQUALS(0, result);
	result = run_command(tool, "-h", full_output, sizeof(full_output));
	ASSERT_EQUALS(0, result);
	ASSERT_STRING_EQUALS(help, full_output);
	line = strstr(help, "Commands:\n");
	if (line == NULL)
	{
		line = strstr(help, "Sub commands:\n");
	}
	ASSERT("command listing exists", line != NULL);
	line = strchr(line, '\n') + 1;
	while (*line != '\0')
	{
		next = strchr(line, '\n');
		ASSERT("command line ends with newline", next != NULL);
		ASSERT("command line fits in 80 columns", next - line <= 80);
		ASSERT("room for command", count < 32);
		used = 0;
		result = sscanf(line, " %31s (%2[a-z]) %n",
				names[count], aliases[count], &used);
		ASSERT_EQUALS(2, result);
		ASSERT_EQUALS(2, strlen(aliases[count]));
		if (strcmp(names[count], "copy") == 0)
		{
			ASSERT_STRING_EQUALS("cp", aliases[count]);
		}
		if (strcmp(names[count], "dir") == 0)
		{
			ASSERT_STRING_EQUALS("dr", aliases[count]);
		}
		if (strcmp(names[count], "dskini") == 0 || strcmp(names[count], "dskinit") == 0)
		{
			ASSERT_STRING_EQUALS("di", aliases[count]);
		}
		if (strcmp(names[count], "rename") == 0)
		{
			ASSERT_STRING_EQUALS("mv", aliases[count]);
		}
		ASSERT("each command has a description", used > 0 && line + used < next);
		for (i = 0; i < count; i++)
		{
			ASSERT("unique alias", strcmp(aliases[i], aliases[count]) != 0);
			ASSERT("alias does not shadow a command", strcmp(names[i], aliases[count]) != 0);
			ASSERT("command does not shadow an alias", strcmp(aliases[i], names[count]) != 0);
		}
		snprintf(arguments, sizeof(arguments), "%s -?", names[count]);
		full_result = run_command(tool, arguments, full_output, sizeof(full_output));
		snprintf(arguments, sizeof(arguments), "%s -?", aliases[count]);
		short_result = run_command(tool, arguments, short_output, sizeof(short_output));
		ASSERT_EQUALS(full_result, short_result);
		ASSERT("subcommand help is not empty", full_output[0] != '\0');
		ASSERT("full command is recognized", strstr(full_output, "unknown command") == NULL);
		ASSERT_STRING_EQUALS(full_output, short_output);
		count++;
		line = next + 1;
	}
	ASSERT_EQUALS(expected_count, count);
}

static void test_decb_aliases(void) { check_commands("decb", 15); }
static void test_os9_aliases(void) { check_commands("os9", 22); }
static void test_dgndos_aliases(void) { check_commands("dgndos", 13); }
static void test_cecb_aliases(void) { check_commands("cecb", 8); }

static void test_info_status_alias(void)
{
	const char *tools[] = {"decb", "os9", "dgndos"};
	char output[8192], alias_output[8192];
	int result, alias_result, i;

	result = run_command("decb", "di commandalias.dsk", output, sizeof(output));
	ASSERT_EQUALS(0, result);
	for (i = 0; i < 3; i++)
	{
		result = run_command(tools[i], "info -s commandalias.dsk", output, sizeof(output));
		alias_result = run_command(tools[i], "in -s commandalias.dsk", alias_output, sizeof(alias_output));
		ASSERT("filesystem status is nonzero", result > 0);
		ASSERT_EQUALS(result, alias_result);
		ASSERT_STRING_EQUALS("", output);
		ASSERT_STRING_EQUALS(output, alias_output);
	}
	remove("commandalias.dsk");
}

int main(void)
{
	RUN(test_decb_aliases);
	RUN(test_os9_aliases);
	RUN(test_dgndos_aliases);
	RUN(test_cecb_aliases);
	RUN(test_info_status_alias);
	return TEST_REPORT();
}

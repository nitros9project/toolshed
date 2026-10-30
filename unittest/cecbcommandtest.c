/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>

void test_cecb_command_copy_basic_data()
{
	error_code ec, read_ec, close_ec;
	FILE *file;
	cecb_path_id path;
	cecb_file_stat statbuf;
	const char contents[] = "123\r456\r";
	char result[sizeof(contents)];
	u_int size;
	size_t written;

	file = fopen("cecb_data_source", "wb");
	ASSERT("create BASIC data source", file != NULL);
	written = fwrite(contents, 1, sizeof(contents) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents) - 1, written);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_bulkerase("cecb_data.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);

	ec = system("../cecb/cecb copy -1ag cecb_data_source cecb_data.cas,DATA > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = _cecb_open(&path, "cecb_data.cas,DATA", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_gs_fd(path, &statbuf);
	size = sizeof(result);
	read_ec = _cecb_read(path, result, &size);
	close_ec = _cecb_close(path);
	remove("cecb_data_source");
	remove("cecb_data.cas");

	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, read_ec);
	ASSERT_EQUALS(0, close_ec);
	ASSERT_EQUALS(1, statbuf.file_type);
	ASSERT_EQUALS(255, statbuf.data_type);
	ASSERT_EQUALS(255, statbuf.gap_flag);
	ASSERT_EQUALS(sizeof(contents) - 1, size);
	ASSERT_MEM_EQUALS(contents, result, size);
}

void test_cecb_command_copy_to_native_directory()
{
	error_code ec;
	FILE *file;
	const unsigned char contents[] = {0x00, 0x01, 0x0a, 0x0d, 0x80, 0xff};
	unsigned char result[sizeof(contents) + 1];
	const char *targets[] = {"cecb_copy_dir", "cecb_copy_dir/"};
	char command[256];
	size_t size;
	int i;

	ec = _cecb_bulkerase("cecb_copy_dir.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	file = fopen("cecb_copy_source", "wb");
	ASSERT("create source file", file != NULL);
	size = fwrite(contents, 1, sizeof(contents), file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents), size);
	ASSERT_EQUALS(0, ec);
	ec = system("../cecb/cecb copy -3bg cecb_copy_source cecb_copy_dir.cas,SOURCE > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = system("mkdir -p cecb_copy_dir");
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		remove("cecb_copy_dir/SOURCE");
		snprintf(command, sizeof(command),
			 "../cecb/cecb copy cecb_copy_dir.cas,SOURCE %s > /dev/null 2>&1",
			 targets[i]);
		ec = system(command);
		ASSERT_EQUALS(0, ec);
		file = fopen("cecb_copy_dir/SOURCE", "rb");
		ASSERT("open extracted file", file != NULL);
		size = fread(result, 1, sizeof(result), file);
		ec = fclose(file);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(sizeof(contents), size);
		ASSERT_MEM_EQUALS(contents, result, sizeof(contents));
	}

	remove("cecb_copy_dir/SOURCE");
	remove("cecb_copy_dir");
	remove("cecb_copy_source");
	remove("cecb_copy_dir.cas");
}

void test_cecb_command_copy_eol_same_type()
{
	error_code ec, close_ec;
	FILE *file;
	coco_path_id path;
	const char contents[] = "FIRST\rSECOND\nTHIRD\r\n";
	char result[sizeof(contents)];
	const char *commands[] = {
		"../cecb/cecb copy -l cecb_eol.cas,SOURCE cecb_eol.cas,DEST > /dev/null 2>&1",
		"../cecb/cecb copy -l cecb_eol_source cecb_eol_dest > /dev/null 2>&1"
	};
	char *destinations[] = {"cecb_eol.cas,DEST", "cecb_eol_dest"};
	u_int size;
	size_t written;
	int i;

	ec = _cecb_bulkerase("cecb_eol.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	remove("cecb_eol_dest");
	file = fopen("cecb_eol_source", "wb");
	ASSERT("create EOL source file", file != NULL);
	written = fwrite(contents, 1, sizeof(contents) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents) - 1, written);
	ASSERT_EQUALS(0, ec);
	ec = system("../cecb/cecb copy -3ag cecb_eol_source cecb_eol.cas,SOURCE > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(0, ec);
		ec = _coco_open(&path, destinations[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		size = sizeof(result);
		ec = _coco_read(path, result, &size);
		close_ec = _coco_close(path);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(0, close_ec);
		ASSERT_EQUALS(sizeof(contents) - 1, size);
		ASSERT_MEM_EQUALS(contents, result, size);
	}

	remove("cecb_eol_source");
	remove("cecb_eol_dest");
	remove("cecb_eol.cas");
}

void test_cecb_command_copy_detokenize_metadata()
{
	error_code ec, read_ec, close_ec;
	FILE *file;
	cecb_path_id path;
	cecb_file_stat statbuf;
	const char program[] = "10 PRINT \"TEST\"\n";
	char result[sizeof(program)];
	u_int size;
	size_t written;

	file = fopen("cecb_token_source", "wb");
	ASSERT("create BASIC source", file != NULL);
	written = fwrite(program, 1, sizeof(program) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(program) - 1, written);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_bulkerase("cecb_token.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = system("../cecb/cecb copy -tbg cecb_token_source cecb_token.cas,TOKENS > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = system("../cecb/cecb copy -k cecb_token.cas,TOKENS cecb_token.cas,TEXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = _cecb_open(&path, "cecb_token.cas,TEXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_gs_fd(path, &statbuf);
	size = sizeof(result);
	read_ec = _cecb_read(path, result, &size);
	close_ec = _cecb_close(path);
	remove("cecb_token_source");
	remove("cecb_token.cas");

	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, read_ec);
	ASSERT_EQUALS(0, close_ec);
	ASSERT_EQUALS(sizeof(program) - 1, size);
	ASSERT_MEM_EQUALS(program, result, size);
	ASSERT_EQUALS(0, statbuf.file_type);
	ASSERT_EQUALS(255, statbuf.data_type);
}

int main()
{
	RUN(test_cecb_command_copy_basic_data);
	RUN(test_cecb_command_copy_to_native_directory);
	RUN(test_cecb_command_copy_eol_same_type);
	RUN(test_cecb_command_copy_detokenize_metadata);

	return TEST_REPORT();
}

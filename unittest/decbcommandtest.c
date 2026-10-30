/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>

void test_decb_command_dskini()
{
	error_code ec;
	native_path_id nativepath;
	u_int size;

	ec = system("../decb/decb dskini test.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	
	ec = _native_open(&nativepath, "test.dsk", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &size);	
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(35*18*256, size);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);
}

void test_decb_command_copy()
{
	error_code ec;
	native_path_id nativepath;
	coco_path_id cocopath;
	u_int source_size, dest_size;

 	ec = system("../decb/decb copy does_not_exist test.dsk,DNE.TXT > /dev/null 2>&1");
 	ASSERT_EQUALS(1, WIFEXITED(ec));
 	ec = WEXITSTATUS(ec);
	ASSERT_EQUALS(EOS_PNNF, ec);

	/* Test a simple copy */
	
	ec = system("../decb/decb copy Makefile test.dsk,MAKEF.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	ec = _native_open(&nativepath, "Makefile", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &source_size);	
	ASSERT_EQUALS(0, ec);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);

	ec = _coco_open(&cocopath, "test.dsk,MAKEF.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _coco_gs_size(cocopath, &dest_size);	
	ASSERT_EQUALS(0, ec);

	ec = _coco_close(cocopath);
	ASSERT_EQUALS(0, ec);

	ASSERT_EQUALS(dest_size, source_size);

	/* Test a copy of an empty file */
	
	remove("empty_file");
	ec = system("touch empty_file > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	ec = system("../decb/decb copy empty_file test.dsk,EMPTY.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	ec = _native_open(&nativepath, "empty_file", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &source_size);	
	ASSERT_EQUALS(0, ec);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);

	ec = _coco_open(&cocopath, "test.dsk,EMPTY.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _coco_gs_size(cocopath, &dest_size);	
	ASSERT_EQUALS(0, ec);

	ec = _coco_close(cocopath);
	ASSERT_EQUALS(0, ec);

	ASSERT_EQUALS(dest_size, source_size);
}

void test_decb_command_copy_to_native_directory()
{
	error_code ec;
	FILE *file;
	const unsigned char contents[] = {0x00, 0x01, 0x0a, 0x0d, 0x80, 0xff};
	unsigned char result[sizeof(contents) + 1];
	const char *targets[] = {"decb_copy_dir", "decb_copy_dir/"};
	char command[256];
	size_t size;
	int i;

	ec = system("../decb/decb dskini copy_dir.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	file = fopen("copy_source", "wb");
	ASSERT("create source file", file != NULL);
	size = fwrite(contents, 1, sizeof(contents), file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents), size);
	ASSERT_EQUALS(0, ec);
	ec = system("../decb/decb copy copy_source copy_dir.dsk,SOURCE.BIN > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = system("mkdir -p decb_copy_dir");
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		remove("decb_copy_dir/SOURCE.BIN");
		snprintf(command, sizeof(command),
			 "../decb/decb copy copy_dir.dsk,SOURCE.BIN %s > /dev/null 2>&1",
			 targets[i]);
		ec = system(command);
		ASSERT_EQUALS(0, ec);
		file = fopen("decb_copy_dir/SOURCE.BIN", "rb");
		ASSERT("open extracted file", file != NULL);
		size = fread(result, 1, sizeof(result), file);
		ec = fclose(file);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(sizeof(contents), size);
		ASSERT_MEM_EQUALS(contents, result, sizeof(contents));
	}

	remove("decb_copy_dir/SOURCE.BIN");
	remove("decb_copy_dir");
	remove("copy_source");
	remove("copy_dir.dsk");
}

void test_decb_command_copy_eol_same_type()
{
	error_code ec;
	FILE *file;
	coco_path_id path;
	const char contents[] = "FIRST\rSECOND\nTHIRD\r\n";
	char result[sizeof(contents)];
	const char *commands[] = {
		"../decb/decb copy -l copy_eol.dsk,SOURCE.TXT copy_eol.dsk,DEST.TXT > /dev/null 2>&1",
		"../decb/decb copy -l copy_eol_source copy_eol_dest > /dev/null 2>&1"
	};
	char *destinations[] = {"copy_eol.dsk,DEST.TXT", "copy_eol_dest"};
	u_int size;
	size_t written;
	int i;

	ec = system("../decb/decb dskini copy_eol.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	remove("copy_eol_dest");
	file = fopen("copy_eol_source", "wb");
	ASSERT("create EOL source file", file != NULL);
	written = fwrite(contents, 1, sizeof(contents) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents) - 1, written);
	ASSERT_EQUALS(0, ec);
	ec = system("../decb/decb copy copy_eol_source copy_eol.dsk,SOURCE.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(0, ec);
		ec = _coco_open(&path, destinations[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_size(path, &size);
		ASSERT_EQUALS(0, ec);
		if (size != sizeof(contents) - 1)
		{
			_coco_close(path);
			ASSERT_EQUALS(sizeof(contents) - 1, size);
		}
		size = sizeof(result);
		ec = _coco_read(path, result, &size);
		_coco_close(path);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(sizeof(contents) - 1, size);
		ASSERT_MEM_EQUALS(contents, result, size);
	}

	remove("copy_eol_source");
	remove("copy_eol_dest");
	remove("copy_eol.dsk");
}

void test_decb_command_copy_empty_metadata()
{
	error_code ec;
	FILE *file;
	coco_path_id path;
	coco_file_stat statbuf;
	u_int size;
	const char *commands[] = {
		"../decb/decb copy -3a copy_empty_source copy_empty.dsk,EMPTY.TXT > /dev/null 2>&1",
		"../decb/decb copy copy_empty.dsk,EMPTY.TXT copy_empty.dsk,COPY.TXT > /dev/null 2>&1"
	};
	char *destinations[] = {"copy_empty.dsk,EMPTY.TXT", "copy_empty.dsk,COPY.TXT"};
	int i;

	ec = system("../decb/decb dskini copy_empty.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	file = fopen("copy_empty_source", "wb");
	ASSERT("create empty source file", file != NULL);
	ec = fclose(file);
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(0, ec);
		ec = _coco_open(&path, destinations[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_fd(path, &statbuf);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_size(path, &size);
		_coco_close(path);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(0, size);
		ASSERT_EQUALS(3, statbuf.file_type);
		ASSERT_EQUALS(255, statbuf.data_type);
	}

	remove("copy_empty_source");
	remove("copy_empty.dsk");
}

int main()
{
	remove("test.dsk");
	RUN(test_decb_command_dskini);
	RUN(test_decb_command_copy);
	RUN(test_decb_command_copy_to_native_directory);
	RUN(test_decb_command_copy_eol_same_type);
	RUN(test_decb_command_copy_empty_metadata);
	
	return TEST_REPORT();
}

/********************************************************************
 * qltest.c - Tests of what the Quick Look extension runs in ToolShed
 *
 * Run from the build directory, with the unit tests' testdata directory
 * as the argument.
 ********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinytest.h"
#include "toolshed_ql.h"

#include <diskimage.h>
#include <flexpath.h>

static const char *testdata;

#define FLEX_IMAGE	"qltest_flex.dsk"
#define NOT_IMAGE	"qltest_text.txt"


static void image_path(char *path, size_t size, const char *name)
{
	int length = snprintf(path, size, "%s/%s", testdata, name);

	if (length < 0 || (size_t)length >= size)
	{
		fprintf(stderr, "testdata path too long\n");
		exit(1);
	}
}


/* Run a command and check that it worked and printed each of the texts. */
static int listing_has(tsql_command command, const char *path, const char *const *texts)
{
	char *errors = NULL;
	int status = -1;
	char *output = tsql_run(command, path, &errors, &status);
	int ok = output != NULL && status == 0 && errors == NULL;
	int i;

	for (i = 0; ok && texts[i] != NULL; i++)
	{
		if (strstr(output, texts[i]) == NULL)
		{
			fprintf(stderr, "'%s' not in:\n%s\n", texts[i], output);
			ok = 0;
		}
	}
	if (errors != NULL)
	{
		fprintf(stderr, "errors: %s\n", errors);
	}
	free(output);
	free(errors);

	return ok;
}


static void test_os9(void)
{
	static const char *const texts[] = { "ATTRALL", "DEEP/L2/L3/L4/L5", "Bytecount", NULL };
	char path[1024];
	unsigned int filesystems = 0;
	int ec;

	image_path(path, sizeof(path), "os9 files mame.dmk");
	ec = tsql_detect(path, &filesystems);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(TSQL_FS_OS9, filesystems);
	ASSERT("the OS-9 listing is recursive and extended", listing_has(TSQL_DIR_OS9, path, texts));
}


static void test_decb(void)
{
	static const char *const texts[] = { "EDTASM   TXT", "TOKEN    BAS", NULL };
	char path[1024];
	unsigned int filesystems = 0;
	int ec;

	image_path(path, sizeof(path), "rsdos mame.dmk");
	ec = tsql_detect(path, &filesystems);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(TSQL_FS_DECB, filesystems);
	ASSERT("the Disk BASIC listing", listing_has(TSQL_DIR_DECB, path, texts));
}


static void test_dragondos(void)
{
	static const char *const texts[] = { "HW1", "5 files", NULL };
	char path[1024];
	unsigned int filesystems = 0;
	int ec;

	image_path(path, sizeof(path), "dragondos mame.vdk");
	ec = tsql_detect(path, &filesystems);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(TSQL_FS_DRAGONDOS, filesystems);
	ASSERT("the Dragon DOS listing", listing_has(TSQL_DIR_DRAGONDOS, path, texts));
}


static void test_flex(void)
{
	static const char *const texts[] = { "QLTEST", NULL };
	unsigned int filesystems = 0;
	error_code ec;
	int detected;

	remove(FLEX_IMAGE);
	ec = _flex_dskinit(FLEX_IMAGE, DISKIMAGE_RAW, 35, 1, 0, 0, "QLTEST", 1);
	ASSERT_EQUALS(0, ec);
	detected = tsql_detect(FLEX_IMAGE, &filesystems);
	ASSERT_EQUALS(0, detected);
	ASSERT("FLEX is found", (filesystems & TSQL_FS_FLEX) != 0);
	ASSERT("the FLEX listing names the volume", listing_has(TSQL_DIR_FLEX, FLEX_IMAGE, texts));
	remove(FLEX_IMAGE);
}


static void test_info(void)
{
	static const char *const texts[] = { "is a DMK disk image", "Tracks 0 to 34", NULL };
	char path[1024];

	image_path(path, sizeof(path), "os9 files mame.dmk");
	ASSERT("info reports the format and tracks", listing_has(TSQL_INFO, path, texts));
}


/* A file that isn't a disk image is refused, so Quick Look can show it some
   other way. */
static void test_not_image(void)
{
	FILE *file = fopen(NOT_IMAGE, "w");
	unsigned int filesystems = 0;
	int ec;

	ASSERT("the text file was made", file != NULL);
	fputs("not a disk\n", file);
	fclose(file);

	ec = tsql_detect(NOT_IMAGE, &filesystems);
	ASSERT("a text file is not a disk image", ec != 0);
	ASSERT_EQUALS(0, filesystems);
	remove(NOT_IMAGE);
}


/* Errors are kept apart from the listing, and stdout and stderr work as
   before afterwards. */
static void test_errors(void)
{
	char *errors = NULL;
	int status = 0;
	char *output = tsql_run(TSQL_DIR_OS9, "qltest_missing.dsk", &errors, &status);

	ASSERT("the output was captured", output != NULL);
	ASSERT("a missing image fails", status != 0);
	ASSERT("the error was captured", errors != NULL && strstr(errors, "qltest_missing.dsk") != NULL);
	free(output);
	free(errors);

	output = tsql_run(TSQL_DIR_OS9, "qltest_missing.dsk", &errors, &status);
	ASSERT("output can be captured again", output != NULL && errors != NULL);
	free(output);
	free(errors);
}


int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Syntax: qltest <testdata directory>\n");
		return 1;
	}
	testdata = argv[1];

	RUN(test_os9);
	RUN(test_decb);
	RUN(test_dragondos);
	RUN(test_flex);
	RUN(test_info);
	RUN(test_not_image);
	RUN(test_errors);

	return TEST_REPORT();
}

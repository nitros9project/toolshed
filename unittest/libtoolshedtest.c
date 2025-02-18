/*
 * Test EOL conversions used by os9 copy -l
 */

#include "tinytest.h"
#include <toolshed.h>

/* Workaround for nested test functions, needed because the tinytest
 * ASSERT will simply set error variables and return */
#define REPORT(name) do { if (tt_current_test_failed) { \
	    printf("failure: While checking \"%s\":\n", name); \
	    return; }} while (0);


/* Adjust size to break CR-LF across two chunks */
/* Note 5 will still break */
#define TEST_CHUNKSIZE 11
#define TEST_CRLF_DATA   "1234\r\n7890\r\n3456\r\n"
#define TEST_LF_DATA     "1234\n7890\n3456\n"
#define TEST_COCO_DATA   "1234\r7890\r3456\r"


#define TRANSLATE_TO_DISK 1
#define TRANSLATE_FROM_DISK 2
void copy_to_disk_and_back(char *original, int translation, char *expected);
void create_disk(char *name);

/*
 * Native2Coco test method:
 * - create native LF file
 * - copy to disk image with translation
 * - copy verbatim back to native file
 * - compare with CR format
 *
 * repeat with native CRLF file
 *
 * Coco2Native test method:
 * - create CR file
 * - copy verbatim to disk image
 * - copy back with translation
 * - compare with native format (CRLF or LF)
 *
 */

void test_via_disk_image()
{
	create_disk("test.dsk");
	REPORT("create_disk");

	copy_to_disk_and_back(TEST_LF_DATA, TRANSLATE_TO_DISK, TEST_COCO_DATA);
	REPORT("LF -> Coco");

	copy_to_disk_and_back(TEST_CRLF_DATA, TRANSLATE_TO_DISK, TEST_COCO_DATA);
	REPORT("CRLF -> Coco");

#ifdef WIN32
	copy_to_disk_and_back(TEST_COCO_DATA, TRANSLATE_FROM_DISK, TEST_CRLF_DATA);
#else
	copy_to_disk_and_back(TEST_COCO_DATA, TRANSLATE_FROM_DISK, TEST_LF_DATA);
#endif
	REPORT("Coco -> native");

	remove("test.dsk");
	remove("test-1.dat");
	remove("test-2.dat");
}


void copy_to_disk_and_back(char *original, int translation, char *expected)
{
	FILE *f;
	int eolTranslate;
	int written;
	int was_read;
	int ec;
	char returned[256];
	char buffer[TEST_CHUNKSIZE];

	/* write original test content to local file */
	f = fopen("test-1.dat", "wb");
	ASSERT("fopen", f != NULL);
	written = fwrite(original, 1, strlen(original), f);
	fclose(f);
	ASSERT_EQUALS(strlen(original), written);

	/* copy to disk image */
	eolTranslate = (translation == TRANSLATE_TO_DISK);
	ec = TSCopyFile("test-1.dat", "test.dsk,test.dat", eolTranslate,
	                1, 0, 0, buffer, TEST_CHUNKSIZE);
	ASSERT_EQUALS(0, ec);

	/* copy back */
	eolTranslate = (translation == TRANSLATE_FROM_DISK);
	ec = TSCopyFile("test.dsk,test.dat", "test-2.dat", eolTranslate,
	                1, 0, 0, buffer, TEST_CHUNKSIZE);
	ASSERT_EQUALS(0, ec);

	/* read content from new local file */
	f = fopen("test-2.dat", "rb");
	was_read = fread(returned, 1, sizeof(returned), f);
	fclose(f);
	ASSERT("Failed fread", was_read > 0);

	if (was_read > 0)
		returned[was_read] = '\0';
	else
		returned[0] = '\0';

	ASSERT_STRING_EQUALS(expected, returned);
}

/* only a helper, disk creation is properly tested in librbf tests */
void create_disk(char *name)
{
	error_code ec;

	/* Create disk */
	unsigned int totalSectors, totalBytes;
	int clusterSize = 0;
	ec = _os9_format(name, 0, 80, 18, 18, 2, 256, &clusterSize, "Test LTS", 8, 8,
			 1, 1, 0, 0, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
}

/* simple test of EOL auto-detection on a single chunk */
void test_native2coco()
{
	char *newBuffer;
	u_int newSize;

	NativeToCoCo(TEST_LF_DATA, strlen(TEST_LF_DATA), &newBuffer, &newSize);
	ASSERT_STRING_EQUALS(TEST_COCO_DATA, newBuffer);
	ASSERT_EQUALS(strlen(TEST_COCO_DATA), newSize);

	free(newBuffer);

	NativeToCoCo(TEST_CRLF_DATA, strlen(TEST_CRLF_DATA), &newBuffer, &newSize);
	ASSERT_STRING_EQUALS(TEST_COCO_DATA, newBuffer);
	ASSERT_EQUALS(strlen(TEST_COCO_DATA), newSize);

	free(newBuffer);
}

/* unfortunately only tests for current native platform */
void test_coco2native()
{
	char *newBuffer;
	u_int newSize;

	CoCoToNative(TEST_COCO_DATA, strlen(TEST_COCO_DATA), &newBuffer, &newSize);
#ifdef WIN32
	ASSERT_STRING_EQUALS(TEST_CRLF_DATA, newBuffer);
	ASSERT_EQUALS(strlen(TEST_CRLF_DATA), newSize);
#else
	ASSERT_STRING_EQUALS(TEST_LF_DATA, newBuffer);
	ASSERT_EQUALS(strlen(TEST_LF_DATA), newSize);
#endif
	free(newBuffer);
}

int main()
{
	RUN(test_native2coco);
	RUN(test_coco2native);
	RUN(test_via_disk_image);

	return TEST_REPORT();
}

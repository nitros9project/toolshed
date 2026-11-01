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
			 1, 1, 0, 0, 3, &totalSectors, &totalBytes);
	ASSERT_EQUALS(0, ec);
}

/* simple test of EOL auto-detection on a single chunk */
void test_native2coco()
{
	char *newBuffer;
	u_int newSize;

	NativeToCoCo(TEST_LF_DATA, strlen(TEST_LF_DATA), &newBuffer, &newSize);
	ASSERT_EQUALS(strlen(TEST_COCO_DATA), newSize);
	ASSERT_MEM_EQUALS(TEST_COCO_DATA, newBuffer, newSize);

	free(newBuffer);

	NativeToCoCo(TEST_CRLF_DATA, strlen(TEST_CRLF_DATA), &newBuffer, &newSize);
	ASSERT_EQUALS(strlen(TEST_COCO_DATA), newSize);
	ASSERT_MEM_EQUALS(TEST_COCO_DATA, newBuffer, newSize);

	free(newBuffer);
}

/* unfortunately only tests for current native platform */
void test_coco2native()
{
	char *newBuffer;
	u_int newSize;

	CoCoToNative(TEST_COCO_DATA, strlen(TEST_COCO_DATA), &newBuffer, &newSize);
#ifdef WIN32
	ASSERT_EQUALS(strlen(TEST_CRLF_DATA), newSize);
	ASSERT_MEM_EQUALS(TEST_CRLF_DATA, newBuffer, newSize);
#else
	ASSERT_EQUALS(strlen(TEST_LF_DATA), newSize);
	ASSERT_MEM_EQUALS(TEST_LF_DATA, newBuffer, newSize);
#endif
	free(newBuffer);
}

#define FS_OS9_MAME "../../../unittest/testdata/os9 mame.dmk"
#define FS_DRAGON_MAME "../../../unittest/testdata/dragondos mame.vdk"
#define FS_IMAGE "test_filesystems.dsk"

static u_int detected(const char *pathlist)
{
	u_int filesystems = 0xFF;

	return _coco_detect_filesystems(pathlist, &filesystems) == 0 ? filesystems : 0xFF;
}

/* Put an empty Disk BASIC FAT and directory on track 17 of an image. */
static void add_decb_track(const char *pathname)
{
	diskimage_id disk;
	u_char sector[256];
	error_code ec = _diskimage_open(&disk, pathname, FAM_READ | FAM_WRITE);
	int number;

	ASSERT_EQUALS(0, ec);
	memset(sector, 0, sizeof(sector));
	memset(sector, 0xFF, 68);
	ec = _diskimage_write_sector(disk, 17, 0, 2, sector, sizeof(sector));
	ASSERT_EQUALS(0, ec);
	memset(sector, 0xFF, sizeof(sector));
	for (number = 3; number <= 11; number++)
	{
		ec = _diskimage_write_sector(disk, 17, 0, number, sector, sizeof(sector));
		ASSERT_EQUALS(0, ec);
	}
	_diskimage_close(disk);
}

void test_detect_filesystems()
{
	char names[64];
	unsigned int total_sectors, total_bytes;
	int cluster_size = 0;
	decb_path_id path;
	u_int count = 5;
	error_code ec;
	u_int found;

	/* Real disks */
	found = detected(FS_OS9_MAME);
	ASSERT_EQUALS(COCO_FS_OS9, found);
	found = detected(FS_DRAGON_MAME);
	ASSERT_EQUALS(COCO_FS_DRAGONDOS, found);

	/* A pathlist means its image. */
	found = detected(FS_OS9_MAME ",SOME/FILE");
	ASSERT_EQUALS(COCO_FS_OS9, found);

	/* An empty Disk BASIC disk, then with a file */
	remove(FS_IMAGE);
	ec = _decb_dskini(FS_IMAGE, 35, NULL, 1, 256, 0);
	ASSERT_EQUALS(0, ec);
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(COCO_FS_DECB, found);
	ec = _decb_create(&path, FS_IMAGE ",FILE.TXT", FAM_WRITE, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_write(path, "hello", &count);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(path);
	ASSERT_EQUALS(0, ec);
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(COCO_FS_DECB, found);

	/* A bad chain in its FAT means it is not one. */
	{
		FILE *fp = fopen(FS_IMAGE, "r+b");
		u_char bad = 0xD0;
		int i;

		for (i = 0; i < 68; i++)
		{
			u_char byte;

			fseek(fp, (17 * 18 + 1) * 256 + i, SEEK_SET);
			fread(&byte, 1, 1, fp);
			if (byte != 0xFF)
			{
				fseek(fp, (17 * 18 + 1) * 256 + i, SEEK_SET);
				fwrite(&bad, 1, 1, fp);
				break;
			}
		}
		fclose(fp);
	}
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(0, found);

	/* A half OS-9 half Disk BASIC disk made by decb dskini -s */
	remove(FS_IMAGE);
	ec = _decb_dskini(FS_IMAGE, 35, NULL, 1, 256, 1);
	ASSERT_EQUALS(0, ec);
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(COCO_FS_DECB, found);

	/* OS-9, which does not touch track 17 in a way Disk BASIC could use */
	remove(FS_IMAGE);
	ec = _os9_format(FS_IMAGE, 0, 35, 18, 18, 1, 256, &cluster_size, "Test", 8, 8, 1, 1, 0, 0, 3,
			 &total_sectors, &total_bytes);
	ASSERT_EQUALS(0, ec);
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(COCO_FS_OS9, found);

	/* Both at once */
	add_decb_track(FS_IMAGE);
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(COCO_FS_OS9 | COCO_FS_DECB, found);

	_coco_filesystem_names(COCO_FS_OS9 | COCO_FS_DECB, names, sizeof(names));
	ASSERT_STRING_EQUALS("OS-9, Disk BASIC", names);
	_coco_filesystem_names(COCO_FS_OS9 | COCO_FS_DRAGONDOS, names, sizeof(names));
	ASSERT_STRING_EQUALS("OS-9, Dragon DOS", names);
	_coco_filesystem_names(COCO_FS_DECB | COCO_FS_DRAGONDOS, names, sizeof(names));
	ASSERT_STRING_EQUALS("Disk BASIC, Dragon DOS", names);
	_coco_filesystem_names(COCO_FS_DRAGONDOS, names, sizeof(names));
	ASSERT_STRING_EQUALS("Dragon DOS", names);
	remove(FS_IMAGE);

	/* An image with nothing on it, and a file that is not an image */
	ec = _diskimage_create(FS_IMAGE, DISKIMAGE_RAW, 1, 35, 256, 18, 0, 0, 0xFF);
	ASSERT_EQUALS(0, ec);
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(0, found);
	remove(FS_IMAGE);
	{
		FILE *fp = fopen(FS_IMAGE, "wb");

		fputs("not a disk image", fp);
		fclose(fp);
	}
	found = detected(FS_IMAGE);
	ASSERT_EQUALS(0, found);
	remove(FS_IMAGE);

	/* A file that does not exist */
	ec = _coco_detect_filesystems("test_no_such_image.dsk", &found);
	ASSERT_EQUALS(EOS_PNNF, ec);
	ASSERT_EQUALS(0, found);
}

/* Each dir names the other filesystems, but not its own. */
void test_dir_lists_other_filesystems()
{
	static char output[8192];
	unsigned int total_sectors, total_bytes;
	int cluster_size = 0;
	size_t got;
	FILE *fp;
	int status;
	error_code ec;

	remove(FS_IMAGE);
	ec = _os9_format(FS_IMAGE, 0, 35, 18, 18, 1, 256, &cluster_size, "Test", 8, 8, 1, 1, 0, 0, 3,
			 &total_sectors, &total_bytes);
	ASSERT_EQUALS(0, ec);

	status = system("../os9/os9 dir " FS_IMAGE ", > test_fs_out.txt 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_fs_out.txt", "rb");
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	output[got] = 0;
	ASSERT("alone on the disk, nothing else is named", strstr(output, "Other possible") == NULL);

	add_decb_track(FS_IMAGE);

	status = system("../os9/os9 dir " FS_IMAGE ", > test_fs_out.txt 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_fs_out.txt", "rb");
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	output[got] = 0;
	ASSERT("os9 dir names Disk BASIC", strstr(output, "Other possible filesystems: Disk BASIC\n") != NULL);

	status = system("../decb/decb dir " FS_IMAGE ", > test_fs_out.txt 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_fs_out.txt", "rb");
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	output[got] = 0;
	ASSERT("decb dir names OS-9", strstr(output, "Other possible filesystems: OS-9\n") != NULL);

	remove("test_fs_out.txt");
	remove(FS_IMAGE);
}

int main()
{
	RUN(test_native2coco);
	RUN(test_coco2native);
	RUN(test_via_disk_image);

	RUN(test_detect_filesystems);
	RUN(test_dir_lists_other_filesystems);
	return TEST_REPORT();
}

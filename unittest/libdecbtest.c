/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>
#include "imagedamage.h"


#define DECB_DMK "test_decb.dmk"

/* An initialized DMK image with one file, whose first granule is known. */
static void decb_dmk_with_file(u_char *data, u_int size)
{
	decb_path_id p;
	error_code ec;
	u_int count = size;

	remove(DECB_DMK);
	ec = _diskimage_create(DECB_DMK, DISKIMAGE_DMK, 1, 35, 256, 18, 0, 0, 0xFF);
	ASSERT_EQUALS(0, ec);
	ec = _decb_dskini(DECB_DMK, 35, NULL, 1, 256, 0);
	ASSERT_EQUALS(0, ec);

	ec = _decb_create(&p, DECB_DMK ",FILE.TXT", FAM_WRITE, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_write(p, data, &count);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(size, count);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
}

/* A granule's first sector, on the disk's track and sector numbers. */
static void granule_address(int granule, int *track, int *sector)
{
	*track = granule / 2 + (granule > 33 ? 1 : 0);
	*sector = (granule % 2) * 9 + 1;
}

void test_decb_dmk_read_write()
{
	u_char data[3000], back[3000];
	decb_path_id p;
	error_code ec;
	u_int size = sizeof(back);
	diskimage_format format;
	_path_type type;
	int i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) (i * 7);
	}
	decb_dmk_with_file(data, sizeof(data));

	/* It is still a DMK image, and is found to be a Disk BASIC one. */
	ec = _diskimage_identify(DECB_DMK, &format);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(DISKIMAGE_DMK, format);
	ec = _coco_identify_image(DECB_DMK ",", &type);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(DECB, type);

	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _decb_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS(0, memcmp(data, back, sizeof(data)));
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _decb_kill(DECB_DMK ",FILE.TXT");
	ASSERT_EQUALS(0, ec);
	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(EOS_PNNF, ec);
	remove(DECB_DMK);
}

void test_decb_colon_needs_raw_image()
{
	u_char data[16] = { 0 };
	decb_path_id p;
	error_code ec;

	decb_dmk_with_file(data, sizeof(data));

	ec = _decb_open(&p, DECB_DMK ",FILE.TXT:0", FAM_READ);
	ASSERT_EQUALS(EOS_BPNAM, ec);
	ec = _decb_open(&p, DECB_DMK ",:1", FAM_READ);
	ASSERT_EQUALS(EOS_BPNAM, ec);
	ec = _decb_create(&p, DECB_DMK ",NEW.TXT:1", FAM_WRITE, 0, 0);
	ASSERT_EQUALS(EOS_BPNAM, ec);
	ec = _decb_dskini(DECB_DMK, 35, NULL, 2, 256, 0);
	ASSERT_EQUALS(EOS_BPNAM, ec);
	remove(DECB_DMK);

	/* A raw image holds as many drives as it likes. */
	remove("test_drives.dsk");
	ec = _decb_dskini("test_drives.dsk", 35, NULL, 2, 256, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_create(&p, "test_drives.dsk,NEW.TXT:1", FAM_WRITE, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _decb_open(&p, "test_drives.dsk,NEW.TXT:1", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _decb_open(&p, "test_drives.dsk,NEW.TXT:0", FAM_READ);
	ASSERT_EQUALS(EOS_PNNF, ec);
	remove("test_drives.dsk");
}

void test_decb_only_needed_sectors_are_errors()
{
	u_char data[3000], back[3000];
	decb_path_id p;
	decb_dir_entry entry;
	error_code ec;
	u_int size = sizeof(back);
	int first_granule, track, sector, i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) (i * 3);
	}
	decb_dmk_with_file(data, sizeof(data));

	/* A sector nothing uses changes nothing, and no later sector moves. */
	image_remove_sector(DECB_DMK, 1, 0, 5);
	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	first_granule = p->dir_entry.first_granule;
	ec = _decb_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, memcmp(data, back, sizeof(data)));
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	/* A sector of the file fails only reading the file. */
	granule_address(first_granule, &track, &sector);
	image_remove_sector(DECB_DMK, track, 0, sector + 2);
	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(back);
	ec = _decb_read(p, back, &size);
	ASSERT_EQUALS(EOS_SECT, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	/* Without a FAT the directory still reads, but not file sizes. */
	image_remove_sector(DECB_DMK, 17, 0, 2);
	ec = _decb_open(&p, DECB_DMK ",", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _decb_readdir(p, &entry);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, memcmp(entry.filename, "FILE    ", 8));
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _decb_gs_size(p, &size);
	ASSERT_EQUALS(EOS_SECT, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _decb_create(&p, DECB_DMK ",NEW.TXT", FAM_WRITE, 0, 0);
	ASSERT_EQUALS(EOS_SECT, ec);

	/* A file found before a missing directory sector does not need it, but
	   looking for one that is not there does. */
	image_remove_sector(DECB_DMK, 17, 0, 4);
	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	ec = _decb_open(&p, DECB_DMK ",NOFILE.TXT", FAM_READ);
	ASSERT_EQUALS(EOS_SECT, ec);
	remove(DECB_DMK);
}

/* decb dir lists the names it can read, then reports the missing FAT. */
void test_decb_dir_lists_names_without_fat()
{
	u_char data[16] = { 0 };
	char output[1024];
	size_t got;
	FILE *fp;
	int status;

	decb_dmk_with_file(data, sizeof(data));
	image_remove_sector(DECB_DMK, 17, 0, 2);

	status = system("../decb/decb dir " DECB_DMK ", > test_decb_dir.txt 2> /dev/null");
	ASSERT_NEQUALS(0, status);

	fp = fopen("test_decb_dir.txt", "rb");
	ASSERT("the listing was written", fp != NULL);
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	output[got] = '\0';
	ASSERT("the file is listed with no size", strstr(output, "FILE     TXT  0  B  ?\n") != NULL);

	remove("test_decb_dir.txt");
	remove(DECB_DMK);
}

void test_decb_bad_crc_is_read()
{
	u_char data[100], back[100];
	decb_path_id p;
	error_code ec;
	u_int size = sizeof(back);
	int track, sector, i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) i;
	}
	decb_dmk_with_file(data, sizeof(data));

	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	granule_address(p->dir_entry.first_granule, &track, &sector);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	image_damage_sector(DECB_DMK, track, 0, sector);

	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _decb_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS((u_char) (data[0] ^ 0xFF), back[0]);
	ASSERT_EQUALS(0, memcmp(data + 1, back + 1, sizeof(data) - 1));
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	remove(DECB_DMK);
}

void test_decb_write_protect()
{
	u_char data[16] = { 0 }, back[16];
	decb_path_id p;
	error_code ec;
	u_int size = sizeof(back);

	decb_dmk_with_file(data, sizeof(data));
	ec = _diskimage_set_write_protect(DECB_DMK, 1);
	ASSERT_EQUALS(0, ec);

	/* Reading is still fine, and a path that could write closes cleanly. */
	ec = _decb_open(&p, DECB_DMK ",FILE.TXT", FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _decb_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _decb_create(&p, DECB_DMK ",NEW.TXT", FAM_WRITE, 0, 0);
	ASSERT_EQUALS(EOS_WP, ec);
	ec = _decb_kill(DECB_DMK ",FILE.TXT");
	ASSERT_EQUALS(EOS_WP, ec);
	ec = _decb_dskini(DECB_DMK, 35, NULL, 1, 256, 0);
	ASSERT_EQUALS(EOS_WP, ec);

	ec = _diskimage_set_write_protect(DECB_DMK, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_kill(DECB_DMK ",FILE.TXT");
	ASSERT_EQUALS(0, ec);
	remove(DECB_DMK);
}

void test_decb_dskini()
{
	error_code ec;

	// test format of a non-existent disk image
	ec = _decb_dskini("test.dsk", 35, "TEST", 1, 256, 0);
	ASSERT_EQUALS(0, ec);

	// test format of an existing disk image with a disk name that is way too long
	ec = _decb_dskini("test.dsk", 35,
			  "TESTDISKNAMEISWAYTOOLONGFORNORMALOPERATION", 1,
			  256, 0);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test format of an existing disk image with a disk name that is barely too long
	ec = _decb_dskini("test.dsk", 35, "TADTTOOLONG", 1, 256, 0);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test format of an existing disk image with a disk name that is the max length
	ec = _decb_dskini("test.dsk", 35, "JUSTRIGHT", 1, 256, 0);
	ASSERT_EQUALS(0, ec);
}

void test_decb_create()
{
	decb_path_id p;
	error_code ec;

	// test create of a non-existing illegal file on a non-existing disk image
	ec = _decb_create(&p, "test_disk_doesnt_exist.dsk,file_doesnt_exist",
			  FAM_READ, 0, 1);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test create of a non-existing legal file on a non-existing disk image
	ec = _decb_create(&p, "test_disk_doesnt_exist.dsk,FILE.TXT", FAM_READ,
			  0, 1);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test create of a non-existing file on an existing disk image
	ec = _decb_create(&p, "test.dsk,test.txt:0", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	// test create of an existing file on an existing disk image
	ec = _decb_create(&p, "test.dsk,test.txt", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	// test create of an existing file on an existing disk image with FAM_NOCREATE
	ec = _decb_create(&p, "test.dsk,test.txt",
			  FAM_READ | FAM_WRITE | FAM_NOCREATE, 0, 1);
	ASSERT_EQUALS(EOS_FAE, ec);

	// test create of an extra long (illegal) filename on an existing disk image
	ec = _decb_create(&p,
			  "test.dsk,file_doesnt_exist_and_is_much_longer_than_decb_limit_of_8_characters",
			  FAM_READ, 0, 1);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test create of an extra long (illegal) filename with a subfolder on an existing disk image
	// the root file is a directory when it isn't
	ec = _decb_create(&p,
			  "test.dsk,file_doesnt_exist_and_is_much_longer_than_decb_limit_of_8_characters/and_this_is_an_even_longer_name_than_the_8_character_limit_in9_rbf_because_it_has_more_characters",
			  FAM_READ, 0, 1);
	ASSERT_EQUALS(EOS_BPNAM, ec);
	
	// Test filling up a disk. File 0 to 67 should succeed.
	// File 68 should fail
	ec = _decb_dskini("filldisk.dsk", 35, "FILL", 1, 256, 0);
	ASSERT_EQUALS(0, ec);

	for (int i=0; i<69; i++)
	{
		char path[256];
		
		snprintf(path, 255, "filldisk.dsk,TEST%d.BIN", i);

		ec = _decb_create(&p, path, FAM_READ | FAM_WRITE, 0,
				  1);
		if (i==68)
		{
			ASSERT_EQUALS(EOS_DF, ec);
		}
		else
		{
			ASSERT_EQUALS(0, ec);
		}
		
		if (i!=68)
		{
			ec = _decb_close(p);
			ASSERT_EQUALS(0, ec);
		}
	}
}

void test_decb_read()
{
	decb_path_id p;
	error_code ec;

	// test create of a non-existing file on an existing disk image
	ec = _decb_create(&p, "test.dsk,test2.txt", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);

	// test read of an empty file
	char buff[32];
	u_int size = 32;
	ec = _decb_read(p, buff, &size);
	ASSERT_EQUALS(EOS_EOF, ec);

	// test close of a validly opened file
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_decb_readln_across_granules()
{
	decb_path_id p;
	error_code ec;
	char data[2304 + 8 + 5];
	char buffer[sizeof(data)];
	u_int size, pos;
	u_int starts[] = { 0, 2300, 2304 };
	u_int line_size = 2304 + 8;

	memset(data, 'A', line_size);
	data[line_size - 1] = '\r';
	memcpy(data + line_size, "NEXT\r", 5);

	ec = _decb_dskini("readln.dsk", 35, "READLN", 1, 256, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_create(&p, "readln.dsk,LINES.TXT", FAM_READ | FAM_WRITE,
			  0, 1);
	ASSERT_EQUALS(0, ec);
	size = sizeof(data);
	ec = _decb_write(p, data, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _decb_open(&p, "readln.dsk,LINES.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);
	for (u_int i = 0; i < sizeof(starts) / sizeof(starts[0]); i++)
	{
		ec = _decb_seek(p, starts[i], SEEK_SET);
		ASSERT_EQUALS(0, ec);
		size = sizeof(buffer);
		ec = _decb_readln(p, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(line_size - starts[i], size);
		ASSERT_MEM_EQUALS(data + starts[i], buffer, size);
		ec = _decb_gs_pos(p, &pos);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(line_size, pos);

		size = sizeof(buffer);
		ec = _decb_readln(p, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(5, size);
		ASSERT_MEM_EQUALS("NEXT\r", buffer, size);
		size = sizeof(buffer);
		ec = _decb_readln(p, buffer, &size);
		ASSERT_EQUALS(EOS_EOF, ec);
		ASSERT_EQUALS(0, size);
	}
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	remove("readln.dsk");
}

void test_decb_write()
{
	decb_path_id p;
	error_code ec;

	// test create of a non-exsting file on an existing disk image
	ec = _decb_create(&p, "test.dsk,test3.txt", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);

	// test write when file only open for read
	char *buff = "this is a string\nand this is another string\n";
	u_int size = strlen(buff);
	ec = _decb_write(p, buff, &size);
	ASSERT_EQUALS(0, ec);

	// test close of file
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	// test create of a non-existing file on an existing disk image
	ec = _decb_create(&p, "test.dsk,test4.txt", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);

	// test write when file open for read and write
	buff = "this is a string";
	size = strlen(buff);
	ec = _decb_write(p, buff, &size);
	ASSERT_EQUALS(0, ec);

	// test close of a validly opened file
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	
	// Test filling up a disk with one file.
	ec = _decb_dskini("filldisk.dsk", 35, "FILL", 1, 256, 0);
	ASSERT_EQUALS(0, ec);

	ec = _decb_create(&p, "filldisk.dsk,test.bin", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);
	
	size_t buffer_alloc = 68*2304;
	unsigned int buffer_size = buffer_alloc;
	unsigned char *buffer = calloc(1, buffer_size);
	ASSERT_EQUALS(1, buffer != NULL);
	
	for( int i=0; i<buffer_alloc; i++)
	{
		buffer[i] = i;
	}
	
	ec = _decb_write(p, buffer, &buffer_size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(buffer_alloc, buffer_size);

	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	// Try adding one more file to a full disk. Should fail.
	ec = _decb_create(&p, "filldisk.dsk,test1.bin", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(EOS_DF, ec);
	
	free(buffer);
	
	// Nearly fill a test disk
	
	ec = _decb_dskini("filldisk.dsk", 35, "FILL", 1, 256, 0);
	ASSERT_EQUALS(0, ec);

	buffer_alloc = 67*2304;
	buffer_size = buffer_alloc;
	buffer = calloc(1, buffer_size);
	ASSERT_EQUALS(1, buffer != NULL);
	
	for( int i=0; i<buffer_alloc; i++)
	{
		buffer[i] = i;
	}
	
	ec = _decb_create(&p, "filldisk.dsk,test.bin", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);

	ec = _decb_write(p, buffer, &buffer_size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(buffer_alloc, buffer_size);

	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	// Try adding one more file to a nearly full disk. Should fail, file too big.
	ec = _decb_create(&p, "filldisk.dsk,test1.bin", FAM_READ | FAM_WRITE, 0,
			  1);
	ASSERT_EQUALS(0, ec);
	
	buffer_alloc = 2304 + 1;
	buffer_size = buffer_alloc;
	ec = _decb_write(p, buffer, &buffer_size);
	ASSERT_EQUALS(EOS_DF, ec);
	ASSERT_EQUALS(0, buffer_size);

	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	
	// Now delete the dangling file
	ec = _decb_kill("filldisk.dsk,test1.bin");
	ASSERT_EQUALS(0, ec);

	free(buffer);
}

void test_decb_overwrite()
{
	decb_path_id p;
	error_code ec;
	char data[2304 * 2 + 32];
	char expected[sizeof(data) + 10], buffer[sizeof(expected)];
	u_int offsets[] = { 0, 2298, sizeof(data) - 10 };
	u_int lengths[] = { sizeof(data), 20, 20 };
	u_int size, pos, filesize, expected_size = sizeof(data);

	ec = _decb_dskini("overwrite.dsk", 35, "OVERWRITE", 1, 256, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_create(&p, "overwrite.dsk,DATA.BIN", FAM_READ | FAM_WRITE,
			  2, 0);
	ASSERT_EQUALS(0, ec);
	memset(data, 'A', sizeof(data));
	memcpy(expected, data, sizeof(data));
	size = sizeof(data);
	ec = _decb_write(p, data, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data), size);

	for (u_int i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++)
	{
		memset(data, 'B' + i, sizeof(data));
		memcpy(expected + offsets[i], data, lengths[i]);
		if (offsets[i] + lengths[i] > expected_size)
		{
			expected_size = offsets[i] + lengths[i];
		}
		ec = _decb_seek(p, offsets[i], SEEK_SET);
		ASSERT_EQUALS(0, ec);
		size = lengths[i];
		ec = _decb_write(p, data, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(lengths[i], size);
		ec = _decb_gs_pos(p, &pos);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(offsets[i] + lengths[i], pos);
		ec = _decb_close(p);
		ASSERT_EQUALS(0, ec);

		ec = _decb_open(&p, "overwrite.dsk,DATA.BIN", FAM_READ | FAM_WRITE);
		ASSERT_EQUALS(0, ec);
		ec = _decb_gs_size(p, &filesize);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(expected_size, filesize);
		size = sizeof(buffer);
		ec = _decb_read(p, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(expected_size, size);
		ASSERT_MEM_EQUALS(expected, buffer, size);
	}
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	remove("overwrite.dsk");
}

void test_decb_open_and_read()
{
	decb_path_id p;
	error_code ec;

	// test open of a non-existing disk image
	ec = _decb_open(&p, "test_disk_doesnt_exist.dsk,file_doesnt_exist",
			FAM_READ);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test open of an existing file on an existing disk image
	ec = _decb_open(&p, "test.dsk,test4.txt", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// test read of an existing file on an existing disk image
	char buf[256];
	u_int size = 128;

	ec = _decb_read(p, buf, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(16, size);

	// test read after all data has been read
	ec = _decb_read(p, buf, &size);
	ASSERT_EQUALS(EOS_EOF, ec);

	// test close of a validly opened file
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
}

void test_decb_delete()
{
	error_code ec;

	// test deletion of a non-existing disk image
	ec = _decb_kill("test_disk_doesnt_exist.dsk,file_doesnt_exist");
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test deletion of a non-exsting file on an existing disk image
	ec = _decb_kill("test.dsk,test4.txt");
	ASSERT_EQUALS(0, ec);
}

void test_decb_rename()
{
	error_code ec;

	// test rename of a non-existing file in an existing disk image
	ec = _decb_rename("test.dsk,file_doesnt_exist",
			  "another_file_doesnt_exist");
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test rename of an existing file in an existing disk image with an illegal name
	ec = _decb_rename("test.dsk,test3.txt", "test_renamed.txt");
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// test rename of an existing file in an existing disk image with a legal name
	ec = _decb_rename("test.dsk,test3.txt", "testren.txt");
	ASSERT_EQUALS(0, ec);
}

void test_decb_extract_first_segment_multiple_segments()
{
	error_code ec;
	unsigned char *output;
	u_int output_size, load_address, exec_address;
	unsigned char input[] = {
		0x00, 0x00, 0x03, 0x20, 0x00, 0x11, 0x22, 0x33,
		0x00, 0x00, 0x05, 0x40, 0x00,
		0xff, 0x00, 0x00, 0x12, 0x34,
		0xff, 0x00, 0x00, 0x56, 0x78
	};
	unsigned char expected[] = { 0x11, 0x22, 0x33 };

	/* The postamble-like bytes in the second segment are ordinary data. */
	ec = _decb_extract_first_segment(input, sizeof(input), &output,
					 &output_size, &load_address, &exec_address);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(expected), output_size);
	ASSERT_MEM_EQUALS(expected, output, output_size);
	ASSERT_EQUALS(0x2000, load_address);
	ASSERT_EQUALS(0x5678, exec_address);
	free(output);
}

void test_decb_binconcat_last_address()
{
	error_code ec;
	unsigned char *output;
	u_int output_size;
	unsigned char input[] = {
		0x00, 0x00, 0x01, 0xff, 0xff, 0xa5,
		0xff, 0x00, 0x00, 0x12, 0x34
	};
	unsigned char wrapped_input[] = {
		0x00, 0x00, 0x01, 0xff, 0xfe, 0x5a,
		0x00, 0x00, 0x02, 0xff, 0xff, 0xa5, 0xc3,
		0xff, 0x00, 0x00, 0x12, 0x34
	};
	unsigned char wrapped_expected[] = {
		0x00, 0x00, 0x01, 0x00, 0x00, 0xc3,
		0x00, 0x00, 0x02, 0xff, 0xfe, 0x5a, 0xa5,
		0xff, 0x00, 0x00, 0x12, 0x34
	};

	/* A byte at the highest address must survive concatenation. */
	ec = _decb_binconcat(input, sizeof(input), &output, &output_size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(input), output_size);
	ASSERT_MEM_EQUALS(input, output, output_size);
	free(output);

	/* Merge adjacent bytes at the top of memory and preserve wraparound. */
	ec = _decb_binconcat(wrapped_input, sizeof(wrapped_input),
			     &output, &output_size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(wrapped_expected), output_size);
	ASSERT_MEM_EQUALS(wrapped_expected, output, output_size);
	free(output);
}

char *basic_prog = "10 PRINT\"Hello World\"\n15 X=INT(234.54-23)\n20 GOTO 10\n";

void test_decb_token()
{
	error_code ec;

	u_int source_length = strlen(basic_prog);
	unsigned char *source;
	unsigned char *entokenize_buffer;
	u_int entokenize_size;
	int path_type = 0;

	char *detokenize_buffer;
	u_int detokenize_size;

	source = malloc(source_length+1);
	strcpy((char *)source, basic_prog);

	ec = _decb_entoken(source, source_length,
			 &entokenize_buffer, &entokenize_size,
			 path_type);
	ASSERT_EQUALS(0, ec);

	ec = _decb_detoken(entokenize_buffer, entokenize_size,
			 &detokenize_buffer, &detokenize_size);
	ASSERT_EQUALS(0, ec);

	ASSERT_EQUALS(source_length, detokenize_size);

	detokenize_buffer[detokenize_size] = '\0';

	ASSERT_STRING_EQUALS(basic_prog, detokenize_buffer);

	free(source);
	free(entokenize_buffer);
	free(detokenize_buffer);
}

/* Test for issue #52: keywords embedded in variable-like identifiers should
   be tokenized. e.g. "BTHEN" should produce a space, then 'B' byte, then the
   THEN token (0xA7), not the literal string "BTHEN". The space is emitted into
   the token stream so the output round-trips correctly through Color BASIC's
   EDIT command. The same applies to other keywords: AND (0xB0), STEP (0xA9). */
void test_decb_token_keyword_after_var()
{
	error_code ec;
	unsigned char *source;
	unsigned char *entokenize_buffer;
	u_int entokenize_size;
	int path_type = 0;

	char *detokenize_buffer;
	u_int detokenize_size;

	/* --- BTHEN: should tokenize as B + space + THEN, round-trip to "B THEN" --- */
	char *prog_bthen = "10 IF A = BTHEN 30\n";
	char *prog_bthen_expected = "10 IF A = B THEN 30\n";
	u_int len = strlen(prog_bthen);
	source = malloc(len + 1);
	strcpy((char *)source, prog_bthen);

	ec = _decb_entoken(source, len, &entokenize_buffer, &entokenize_size, path_type);
	ASSERT_EQUALS(0, ec);

	/* Verify the THEN token byte (0xA7) is present */
	int found_then = 0;
	for (u_int i = 0; i < entokenize_size; i++)
	{
		if (entokenize_buffer[i] == 0xA7)
		{
			found_then = 1;
			break;
		}
	}
	ASSERT_EQUALS(1, found_then);

	/* Verify 'B' (0x42) + space (0x20) + THEN token (0xA7) appear in sequence */
	int b_space_then = 0;
	for (u_int i = 0; i + 2 < entokenize_size; i++)
	{
		if (entokenize_buffer[i] == 0x42 &&
		    entokenize_buffer[i + 1] == 0x20 &&
		    entokenize_buffer[i + 2] == 0xA7)
		{
			b_space_then = 1;
			break;
		}
	}
	ASSERT_EQUALS(1, b_space_then);

	/* Verify round-trip through detokenizer produces the spaced form */
	ec = _decb_detoken(entokenize_buffer, entokenize_size,
			   &detokenize_buffer, &detokenize_size);
	ASSERT_EQUALS(0, ec);
	detokenize_buffer[detokenize_size] = '\0';
	ASSERT_STRING_EQUALS(prog_bthen_expected, detokenize_buffer);

	free(source);
	free(entokenize_buffer);
	free(detokenize_buffer);

	/* --- BAND: B followed by AND token (0xB0), round-trips to "B AND A" --- */
	char *prog_band = "10 IF BAND A THEN 20\n";
	char *prog_band_expected = "10 IF B AND A THEN 20\n";
	len = strlen(prog_band);
	source = malloc(len + 1);
	strcpy((char *)source, prog_band);

	ec = _decb_entoken(source, len, &entokenize_buffer, &entokenize_size, path_type);
	ASSERT_EQUALS(0, ec);

	int found_and = 0;
	for (u_int i = 0; i < entokenize_size; i++)
	{
		if (entokenize_buffer[i] == 0xB0)
		{
			found_and = 1;
			break;
		}
	}
	ASSERT_EQUALS(1, found_and);

	ec = _decb_detoken(entokenize_buffer, entokenize_size,
			   &detokenize_buffer, &detokenize_size);
	ASSERT_EQUALS(0, ec);
	detokenize_buffer[detokenize_size] = '\0';
	ASSERT_STRING_EQUALS(prog_band_expected, detokenize_buffer);

	free(source);
	free(entokenize_buffer);
	free(detokenize_buffer);

	/* --- XSTEP: X followed by STEP token (0xA9), round-trips to "X STEP" --- */
	char *prog_xstep = "10 PRINT 10*XSTEP5\n";
	char *prog_xstep_expected = "10 PRINT 10*X STEP5\n";
	len = strlen(prog_xstep);
	source = malloc(len + 1);
	strcpy((char *)source, prog_xstep);

	ec = _decb_entoken(source, len, &entokenize_buffer, &entokenize_size, path_type);
	ASSERT_EQUALS(0, ec);

	int found_step = 0;
	for (u_int i = 0; i < entokenize_size; i++)
	{
		if (entokenize_buffer[i] == 0xA9)
		{
			found_step = 1;
			break;
		}
	}
	ASSERT_EQUALS(1, found_step);

	ec = _decb_detoken(entokenize_buffer, entokenize_size,
			   &detokenize_buffer, &detokenize_size);
	ASSERT_EQUALS(0, ec);
	detokenize_buffer[detokenize_size] = '\0';
	ASSERT_STRING_EQUALS(prog_xstep_expected, detokenize_buffer);

	free(source);
	free(entokenize_buffer);
	free(detokenize_buffer);
}

#define DECB_IMAGE "test_decb_format.img"

/* A raw path is the bytes of the whole disk, with one position. */
static void check_decb_raw_path(diskimage_format format)
{
	static const u_char text[] = "across a sector boundary";
	u_char back[sizeof(text)];
	u_char sector[256];
	decb_path_id p;
	u_int size = sizeof(text);
	u_int position;

	remove(DECB_IMAGE);
	ASSERT_EC(0, _decb_dskini_format(DECB_IMAGE, format, 35, NULL, 1, 256, 0));

	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ | FAM_WRITE));
	ASSERT_EQUALS(1, p->israw);

	/* Write across a sector boundary, keeping the rest of both. */
	ASSERT_EC(0, _decb_seek(p, 250, SEEK_SET));
	ASSERT_EC(0, _decb_write(p, (void *) text, &size));
	ASSERT_EQUALS(sizeof(text), size);
	ASSERT_EC(0, _decb_gs_pos(p, &position));
	ASSERT_EQUALS(250 + sizeof(text), position);

	/* Back from where it is, and from the end */
	ASSERT_EC(0, _decb_seek(p, -(int) sizeof(text), SEEK_CUR));
	size = sizeof(back);
	ASSERT_EC(0, _decb_read(p, back, &size));
	ASSERT_EQUALS(sizeof(back), size);
	ASSERT_EQUALS(0, memcmp(text, back, sizeof(text)));

	ASSERT_EC(0, _decb_seek(p, -256, SEEK_END));
	ASSERT_EC(0, _decb_gs_pos(p, &position));
	ASSERT_EQUALS(161280 - 256, position);
	ASSERT_EC(EOS_SE, _decb_seek(p, -200000, SEEK_END));
	ASSERT_EC(EOS_SE, _decb_seek(p, -1000000, SEEK_CUR));

	/* The size and end of a raw path are the disk's. */
	ASSERT_EC(0, _decb_gs_size(p, &size));
	ASSERT_EQUALS(161280, size);
	ASSERT_EC(0, _decb_gs_eof(p));
	ASSERT_EC(0, _decb_seek(p, 161280, SEEK_SET));
	ASSERT_EC(EOS_EOF, _decb_gs_eof(p));
	size = 1;
	ASSERT_EC(EOS_EOF, _decb_read(p, back, &size));
	ASSERT_EC(0, _decb_close(p));

	/* It landed where it says, and left the rest of the sectors. */
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ));
	ASSERT_EC(0, _decb_gs_sector(p, 0, 1, (char *) sector));
	ASSERT_EQUALS(0xFF, sector[249]);
	ASSERT_EQUALS(0, memcmp(text, &sector[250], 6));
	ASSERT_EC(0, _decb_gs_sector(p, 0, 2, (char *) sector));
	ASSERT_EQUALS(0, memcmp(text + 6, sector, sizeof(text) - 6));
	ASSERT_EQUALS(0xFF, sector[sizeof(text) - 6]);
	ASSERT_EC(0, _decb_close(p));
	remove(DECB_IMAGE);
}

void test_decb_raw_path_all_formats()
{
	check_decb_raw_path(DISKIMAGE_RAW);
	check_decb_raw_path(DISKIMAGE_DMK);
	check_decb_raw_path(DISKIMAGE_SDF);
	check_decb_raw_path(DISKIMAGE_VDK);
}

/* Only a raw image grows, and the rest of a new sector is $FF. */
void test_decb_raw_write_past_end()
{
	static const u_char text[] = "past the end";
	u_char sector[256];
	decb_path_id p;
	u_int size = sizeof(text);
	FILE *fp;
	long length;

	remove(DECB_IMAGE);
	ASSERT_EC(0, _decb_dskini_format(DECB_IMAGE, DISKIMAGE_RAW, 35, NULL, 1, 256, 0));
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _decb_seek(p, 161280 + 256 + 10, SEEK_SET));
	ASSERT_EC(0, _decb_write(p, (void *) text, &size));
	ASSERT_EQUALS(sizeof(text), size);
	ASSERT_EC(0, _decb_close(p));

	fp = fopen(DECB_IMAGE, "rb");
	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	ASSERT_EQUALS(161280 + 512, length);
	fseek(fp, 161280 + 256, SEEK_SET);
	length = (long) fread(sector, 1, sizeof(sector), fp);
	fclose(fp);
	ASSERT_EQUALS(256, length);
	ASSERT_EQUALS(0xFF, sector[9]);
	ASSERT_EQUALS(0, memcmp(text, &sector[10], sizeof(text)));
	ASSERT_EQUALS(0xFF, sector[10 + sizeof(text)]);
	remove(DECB_IMAGE);

	/* Other images have the sectors they have. */
	ASSERT_EC(0, _decb_dskini_format(DECB_IMAGE, DISKIMAGE_DMK, 35, NULL, 1, 256, 0));
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _decb_seek(p, 161280 + 10, SEEK_SET));
	size = 5;
	ASSERT_EC(EOS_SECT, _decb_write(p, (void *) text, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(0, _decb_close(p));
	remove(DECB_IMAGE);
}

/* What a disk needs of its sectors doesn't depend on how they are recorded. */
static void check_decb_damage(diskimage_format format)
{
	static u_char data[3000], back[3000];
	decb_path_id p;
	decb_dir_entry entry;
	u_int size = sizeof(data);
	int track, sector, i;

	for (i = 0; i < (int) sizeof(data); i++)
	{
		data[i] = (u_char) (i * 3);
	}

	remove(DECB_IMAGE);
	ASSERT_EC(0, _decb_dskini_format(DECB_IMAGE, format, 35, NULL, 1, 256, 0));
	ASSERT_EC(0, _decb_create(&p, DECB_IMAGE ",FILE.TXT", FAM_WRITE, 0, 0));
	ASSERT_EC(0, _decb_write(p, data, &size));
	ASSERT_EC(0, _decb_close(p));

	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ));
	granule_address(p->dir_entry.first_granule, &track, &sector);
	ASSERT_EC(0, _decb_close(p));

	/* A bad CRC gives the data as recorded. */
	image_damage_sector(DECB_IMAGE, track, 0, sector);
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(0, _decb_read(p, back, &size));
	ASSERT_EQUALS(sizeof(data), size);
	ASSERT_EQUALS((u_char) (data[0] ^ 0xFF), back[0]);
	ASSERT_EQUALS(0, memcmp(data + 1, back + 1, sizeof(data) - 1));
	ASSERT_EC(0, _decb_close(p));

	/* A sector nothing uses changes nothing, and no later sector moves. */
	image_remove_sector(DECB_IMAGE, 1, 0, 5);
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(0, _decb_read(p, back, &size));
	ASSERT_EQUALS(0, memcmp(data + 1, back + 1, sizeof(data) - 1));
	ASSERT_EC(0, _decb_close(p));

	/* A sector of the file fails only reading the file. */
	image_remove_sector(DECB_IMAGE, track, 0, sector + 2);
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(EOS_SECT, _decb_read(p, back, &size));
	ASSERT_EC(0, _decb_close(p));

	/* Without a FAT the directory reads, but not file sizes. */
	image_remove_sector(DECB_IMAGE, 17, 0, 2);
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ));
	ASSERT_EC(0, _decb_readdir(p, &entry));
	ASSERT_EQUALS(0, memcmp(entry.filename, "FILE    ", 8));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ));
	ASSERT_EC(EOS_SECT, _decb_gs_size(p, &size));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(EOS_SECT, _decb_create(&p, DECB_IMAGE ",NEW.TXT", FAM_WRITE, 0, 0));
	ASSERT_EC(EOS_SECT, _decb_kill(DECB_IMAGE ",FILE.TXT"));

	/* A missing directory sector only matters to what gets that far. */
	image_remove_sector(DECB_IMAGE, 17, 0, 4);
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(EOS_SECT, _decb_open(&p, DECB_IMAGE ",NOFILE.TXT", FAM_READ));
	remove(DECB_IMAGE);
}

void test_decb_damage_sdf()
{
	check_decb_damage(DISKIMAGE_SDF);
}

/* Make a disk in a format, and use all that a disk does. */
static void check_decb_round_trip(diskimage_format format, int sides)
{
	static const u_char text[] = "round trip";
	u_char back[sizeof(text)];
	decb_path_id p;
	char buffer[256];
	u_int size = sizeof(text);
	u_int free_granules;

	remove(DECB_IMAGE);
	if (sides == 1)
	{
		ASSERT_EC(0, _decb_dskini_format(DECB_IMAGE, format, 35, NULL, 1, 256, 0));
	}
	else
	{
		ASSERT_EC(0, _diskimage_create(DECB_IMAGE, format, sides, 35, 256, 18, 0, 0, 0xFF));
		ASSERT_EC(0, _decb_dskini(DECB_IMAGE, 35, NULL, 1, 256, 0));
	}

	ASSERT_EC(0, _decb_create(&p, DECB_IMAGE ",FILE.TXT", FAM_WRITE, 0, 0));
	ASSERT_EC(0, _decb_write(p, (void *) text, &size));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(0, _decb_rename(DECB_IMAGE ",FILE.TXT", "OTHER.TXT"));

	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",OTHER.TXT", FAM_READ));
	size = sizeof(back);
	ASSERT_EC(0, _decb_read(p, back, &size));
	ASSERT_EQUALS(sizeof(text), size);
	ASSERT_EQUALS(0, memcmp(text, back, size));

	/* Past the last track is not a sector. */
	ASSERT_EC(EOS_SECT, _decb_gs_sector(p, 35, 1, buffer));
	ASSERT_EC(0, _decb_close(p));

	ASSERT_EC(0, TSDECBFree(DECB_IMAGE, &free_granules));
	ASSERT_EQUALS(67, free_granules);
	ASSERT_EC(0, _decb_kill(DECB_IMAGE ",OTHER.TXT"));
	ASSERT_EC(0, TSDECBFree(DECB_IMAGE, &free_granules));
	ASSERT_EQUALS(68, free_granules);
	remove(DECB_IMAGE);
}

void test_decb_round_trip_all_formats()
{
	check_decb_round_trip(DISKIMAGE_DMK, 1);
	check_decb_round_trip(DISKIMAGE_SDF, 1);
	check_decb_round_trip(DISKIMAGE_VDK, 1);

	/* A JVC image needs a reason for a header, and side 1 goes unused. */
	check_decb_round_trip(DISKIMAGE_JVC, 2);
	check_decb_round_trip(DISKIMAGE_DMK, 2);
	check_decb_round_trip(DISKIMAGE_SDF, 2);
}

static void check_decb_write_protect(diskimage_format format)
{
	u_char data[16] = { 0 }, back[16];
	decb_path_id p;
	u_int size = sizeof(data);

	remove(DECB_IMAGE);
	ASSERT_EC(0, _decb_dskini_format(DECB_IMAGE, format, 35, NULL, 1, 256, 0));
	ASSERT_EC(0, _decb_create(&p, DECB_IMAGE ",FILE.TXT", FAM_WRITE, 0, 0));
	ASSERT_EC(0, _decb_write(p, data, &size));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(0, _diskimage_set_write_protect(DECB_IMAGE, 1));

	/* Reading is still fine, and a path that could write closes cleanly. */
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",FILE.TXT", FAM_READ | FAM_WRITE));
	size = sizeof(back);
	ASSERT_EC(0, _decb_read(p, back, &size));
	ASSERT_EC(0, _decb_close(p));

	ASSERT_EC(EOS_WP, _decb_create(&p, DECB_IMAGE ",NEW.TXT", FAM_WRITE, 0, 0));
	ASSERT_EC(EOS_WP, _decb_kill(DECB_IMAGE ",FILE.TXT"));
	ASSERT_EC(EOS_WP, _decb_rename(DECB_IMAGE ",FILE.TXT", "OTHER.TXT"));
	ASSERT_EC(EOS_WP, _decb_dskini(DECB_IMAGE, 35, NULL, 1, 256, 0));

	/* A raw path is refused too. */
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ | FAM_WRITE));
	size = sizeof(back);
	ASSERT_EC(EOS_WP, _decb_write(p, back, &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(0, _decb_close(p));

	ASSERT_EC(0, _diskimage_set_write_protect(DECB_IMAGE, 0));
	ASSERT_EC(0, _decb_kill(DECB_IMAGE ",FILE.TXT"));
	remove(DECB_IMAGE);
}

void test_decb_write_protect_all_formats()
{
	check_decb_write_protect(DISKIMAGE_DMK);
	check_decb_write_protect(DISKIMAGE_SDF);
	check_decb_write_protect(DISKIMAGE_VDK);
}

/* Sectors of another size can't be Disk BASIC's. */
void test_decb_wrong_sector_size()
{
	char sector[256];
	decb_path_id p;

	remove(DECB_IMAGE);
	ASSERT_EC(0, _diskimage_create(DECB_IMAGE, DISKIMAGE_DMK, 1, 35, 512, 9, 0, 0, 0xFF));
	ASSERT_EC(0, _decb_open(&p, DECB_IMAGE ",", FAM_READ | FAM_WRITE));
	ASSERT_EC(EOS_SECT, p->fat_ec);
	ASSERT_EC(EOS_SECT, _decb_gs_sector(p, 0, 1, sector));
	ASSERT_EC(EOS_SECT, _decb_ss_sector(p, 0, 1, sector));
	ASSERT_EC(0, _decb_close(p));
	remove(DECB_IMAGE);
}

/*
 * "rsdos mame.dmk" was made by Disk Extended Color BASIC 1.1 under MAME, by a
 * program that writes each machine code file from a pattern, so that tests can
 * recompute it.  What this expects of its directory is what DIR and FREE(0) said.
 */
#define REAL_DISK "../../../unittest/testdata/rsdos mame.dmk"
#define REAL_COPY "test_real_rsdos.dmk"
#define REAL_FREE_GRANULES 39

static const struct
{
	const char *name;
	int type;
	int ascii;
	int granules;
} real_directory[] = {
	{ "CROSS.BIN", 2, 0x00, 7 },
	{ "ONESECT.BIN", 2, 0x00, 1 },
	{ "SECTPLUS.BIN", 2, 0x00, 1 },
	{ "ONEGRAN.BIN", 2, 0x00, 2 },
	{ "FRAG.BIN", 2, 0x00, 4 },
	{ "KEEP.BIN", 2, 0x00, 2 },
	{ "EMPTY.DAT", 1, 0xFF, 1 },
	{ "AFTER.BIN", 2, 0x00, 3 },
	{ "ONEBYTE.DAT", 1, 0xFF, 1 },
	{ "LINES.DAT", 1, 0xFF, 3 },
	{ "TOKEN.BAS", 0, 0x00, 1 },
	{ "ASCII.BAS", 0, 0xFF, 1 },
	{ "EDTASM.TXT", 3, 0xFF, 1 },
	{ "LAST.DAT", 1, 0xFF, 1 },
};

static const struct
{
	const char *name;
	int seed;
	int size;
} real_patterns[] = {
	{ "CROSS.BIN", 9, 14000 },
	{ "ONESECT.BIN", 1, 246 },
	{ "SECTPLUS.BIN", 2, 247 },
	{ "ONEGRAN.BIN", 3, 2294 },
	{ "FRAG.BIN", 8, 9000 },
	{ "KEEP.BIN", 5, 2294 },
	{ "AFTER.BIN", 7, 4598 },
};

static void copy_real_disk(void)
{
	static char buffer[4096];
	FILE *in = fopen(REAL_DISK, "rb");
	FILE *out = fopen(REAL_COPY, "wb");
	size_t got;

	ASSERT("the real disk is there", in != NULL && out != NULL);
	while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0)
	{
		fwrite(buffer, 1, got, out);
	}
	fclose(in);
	fclose(out);
}

/* Read a whole file through a pathlist. */
static error_code read_real_file(const char *image, const char *name, u_char *buffer, u_int capacity, u_int *size)
{
	char pathlist[128];
	decb_path_id p;
	error_code ec;

	snprintf(pathlist, sizeof(pathlist), "%s,%s", image, name);
	ec = _decb_open(&p, pathlist, FAM_READ);
	if (ec != 0)
	{
		return ec;
	}
	*size = capacity;
	ec = _decb_read(p, buffer, size);
	if (ec == EOS_EOF)
	{
		*size = 0;
		ec = 0;
	}
	_decb_close(p);

	return ec;
}

/* Check a SAVEM file: its header, the pattern, and its trailer. */
static void check_real_pattern(const u_char *data, u_int size, int seed, int length)
{
	int i;

	ASSERT_EQUALS(length + 10, size);
	ASSERT_EQUALS(0x00, data[0]);
	ASSERT_EQUALS(length, data[1] << 8 | data[2]);
	ASSERT_EQUALS(0x4000, data[3] << 8 | data[4]);
	for (i = 0; i < length; i++)
	{
		int byte = (i & 1) ? (i & 255) : (i >> 8);

		ASSERT_EQUALS((byte + seed) & 255, data[5 + i]);
	}
	ASSERT_EQUALS(0xFF, data[5 + length]);
	ASSERT_EQUALS(0x4000, data[8 + length] << 8 | data[9 + length]);
}

void test_real_rsdos_is_recognized()
{
	u_int filesystems;
	_path_type type;

	ASSERT_EC(0, _coco_detect_filesystems(REAL_DISK, &filesystems));
	ASSERT_EQUALS(COCO_FS_DECB, filesystems);
	ASSERT_EC(0, _coco_identify_image(REAL_DISK ",", &type));
	ASSERT_EQUALS(DECB, type);

	/* Real DSKINI leaves all of track 17 $FF, like an unused track, and that
	   is deliberately not taken for a disk.  This is what that fixture holds. */
	ASSERT_EC(0, _coco_detect_filesystems("../../../unittest/testdata/rsdos artifact.dmk", &filesystems));
	ASSERT_EQUALS(0, filesystems);
}

void test_real_rsdos_directory_matches_the_machine()
{
	decb_path_id p;
	decb_dir_entry entry;
	u_int free_granules;
	int killed_at = -1, live = 0, position = 0;
	error_code ec;

	ASSERT_EC(0, _decb_open(&p, REAL_DISK ",", FAM_READ));
	while ((ec = _decb_readdir(p, &entry)) == 0)
	{
		if (entry.filename[0] == 0xFF)
		{
			break;
		}
		if (entry.filename[0] == 0)
		{
			killed_at = position;
		}
		else
		{
			u_char name[13];
			int granule = entry.first_granule;
			int granules = 1;

			ASSERT("no more files than DIR showed", live < (int) (sizeof(real_directory) / sizeof(real_directory[0])));
			ASSERT_EC(0, _decb_ncpy_name(entry, name, sizeof(name)));
			ASSERT_STRING_EQUALS(real_directory[live].name, (char *) name);
			ASSERT_EQUALS(real_directory[live].type, entry.file_type);
			ASSERT_EQUALS(real_directory[live].ascii, entry.ascii_flag);
			while (p->FAT[granule] < 0xC0)
			{
				granule = p->FAT[granule];
				granules++;
			}
			ASSERT_EQUALS(real_directory[live].granules, granules);
			live++;
		}
		position++;
	}
	ASSERT_EC(0, _decb_close(p));

	ASSERT_EQUALS(sizeof(real_directory) / sizeof(real_directory[0]), live);
	ASSERT("a killed entry is passed over, with a file after it", killed_at >= 8 && killed_at < position - 1);

	ASSERT_EC(0, TSDECBFree(REAL_DISK, &free_granules));
	ASSERT_EQUALS(REAL_FREE_GRANULES, free_granules);
}

void test_real_rsdos_files_match_their_patterns()
{
	static u_char data[20000];
	u_int size;
	size_t i;

	for (i = 0; i < sizeof(real_patterns) / sizeof(real_patterns[0]); i++)
	{
		ASSERT_EC(0, read_real_file(REAL_DISK, real_patterns[i].name, data, sizeof(data), &size));
		check_real_pattern(data, size, real_patterns[i].seed, real_patterns[i].size);
	}

	/* Sizes at the edges, as the ROM records them */
	ASSERT_EC(0, _decb_gs_size_pathlist(REAL_DISK ",ONESECT.BIN", &size));
	ASSERT_EQUALS(256, size);
	ASSERT_EC(0, _decb_gs_size_pathlist(REAL_DISK ",SECTPLUS.BIN", &size));
	ASSERT_EQUALS(257, size);
	ASSERT_EC(0, _decb_gs_size_pathlist(REAL_DISK ",ONEGRAN.BIN", &size));
	ASSERT_EQUALS(2304, size);
	ASSERT_EC(0, _decb_gs_size_pathlist(REAL_DISK ",EMPTY.DAT", &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(0, _decb_gs_size_pathlist(REAL_DISK ",ONEBYTE.DAT", &size));
	ASSERT_EQUALS(1, size);

	ASSERT_EC(0, read_real_file(REAL_DISK, "EMPTY.DAT", data, sizeof(data), &size));
	ASSERT_EQUALS(0, size);
	ASSERT_EC(0, read_real_file(REAL_DISK, "ONEBYTE.DAT", data, sizeof(data), &size));
	ASSERT_EQUALS(1, size);
	ASSERT_EQUALS('A', data[0]);
}

/* Reading in small pieces gets the same bytes, through every granule. */
void test_real_rsdos_reads_in_pieces()
{
	static u_char whole[20000], piece[20000];
	decb_path_id p;
	u_int whole_size, total = 0, size;
	error_code ec;

	ASSERT_EC(0, read_real_file(REAL_DISK, "AFTER.BIN", whole, sizeof(whole), &whole_size));
	ASSERT_EC(0, _decb_open(&p, REAL_DISK ",AFTER.BIN", FAM_READ));
	for (;;)
	{
		size = 97;
		ec = _decb_read(p, piece + total, &size);
		if (ec != 0)
		{
			break;
		}
		total += size;
	}
	ASSERT_EQUALS(EOS_EOF, ec);
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EQUALS(whole_size, total);
	ASSERT_EQUALS(0, memcmp(whole, piece, total));
}

void test_real_rsdos_reads_lines()
{
	char line[64], expected[64];
	decb_path_id p;
	u_int size;
	int number = 0;
	error_code ec;

	/* PRINT #1,"LINE";L puts a space after a number, and a CR ends each line. */
	ASSERT_EC(0, _decb_open(&p, REAL_DISK ",LINES.DAT", FAM_READ));
	for (;;)
	{
		size = sizeof(line);
		ec = _decb_readln(p, line, &size);
		if (ec != 0)
		{
			break;
		}
		number++;
		snprintf(expected, sizeof(expected), "LINE %d \r", number);
		ASSERT_EQUALS(strlen(expected), size);
		ASSERT_EQUALS(0, memcmp(expected, line, size));
	}
	ASSERT_EQUALS(EOS_EOF, ec);
	ASSERT_EQUALS(500, number);
	ASSERT_EC(0, _decb_close(p));
}

/* The real tokenized program lists as what BASIC itself saved as ASCII. */
void test_real_rsdos_detokenizes()
{
	static u_char token[4096], ascii[4096];
	char *text = NULL;
	unsigned char *again = NULL;
	u_int token_size, ascii_size, text_size, again_size;
	u_int a = 0, t = 0;

	ASSERT_EC(0, read_real_file(REAL_DISK, "TOKEN.BAS", token, sizeof(token), &token_size));
	ASSERT_EC(0, read_real_file(REAL_DISK, "ASCII.BAS", ascii, sizeof(ascii), &ascii_size));
	ASSERT_EQUALS(0, _decb_detect_tokenized(token, token_size));
	ASSERT_NEQUALS(0, _decb_detect_tokenized(ascii, ascii_size));
	ASSERT_EC(0, _decb_detoken(token, token_size, &text, &text_size));

	/* BASIC's file starts with a CR, and ends lines in CR, not LF. */
	if (ascii[0] == 0x0D)
	{
		a = 1;
	}
	while (a < ascii_size && t < text_size)
	{
		char from_ascii = ascii[a] == 0x0D ? '\n' : (char) ascii[a];

		ASSERT_EQUALS(from_ascii, text[t]);
		a++;
		t++;
	}
	ASSERT_EQUALS(ascii_size, a);
	ASSERT_EQUALS(text_size, t);

	/* And that listing tokenizes to exactly what BASIC saved. */
	ASSERT_EC(0, _decb_entoken((unsigned char *) text, text_size, &again, &again_size, 1));
	ASSERT_EQUALS(token_size, again_size);
	ASSERT_EQUALS(0, memcmp(token, again, token_size));
	free(again);
	free(text);
}

/* Change a copy of the real disk, and keep what the ROM put there. */
void test_real_rsdos_can_be_changed()
{
	static u_char data[20000], text[] = "written by toolshed";
	unsigned char reserved[16];
	decb_path_id p;
	decb_dir_entry entry;
	u_int size = sizeof(text);
	u_int free_granules;
	error_code ec;
	int found = 0;

	copy_real_disk();

	/* A new file takes the killed entry and free granules. */
	ASSERT_EC(0, _decb_create(&p, REAL_COPY ",NEW.TXT", FAM_WRITE, 3, 0xFF));
	ASSERT_EC(0, _decb_write(p, text, &size));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(0, TSDECBFree(REAL_COPY, &free_granules));
	ASSERT_EQUALS(REAL_FREE_GRANULES - 1, free_granules);
	ASSERT_EC(0, read_real_file(REAL_COPY, "NEW.TXT", data, sizeof(data), &size));
	ASSERT_EQUALS(sizeof(text), size);
	ASSERT_EQUALS(0, memcmp(text, data, size));

	/* Other files are as they were. */
	ASSERT_EC(0, read_real_file(REAL_COPY, "CROSS.BIN", data, sizeof(data), &size));
	check_real_pattern(data, size, 9, 14000);

	/* EDTASM keeps something in the part of the entry BASIC leaves unused. */
	ASSERT_EC(0, _decb_open(&p, REAL_COPY ",", FAM_READ));
	while ((ec = _decb_readdir(p, &entry)) == 0 && memcmp(entry.filename, "EDTASM  ", 8) != 0)
	{
	}
	ASSERT_EQUALS(0, ec);
	memcpy(reserved, &entry.file_status, sizeof(reserved));
	ASSERT_EC(0, _decb_close(p));
	ASSERT_EC(0, _decb_rename(REAL_COPY ",EDTASM.TXT", "SOURCE.TXT"));
	ASSERT_EC(0, _decb_open(&p, REAL_COPY ",", FAM_READ));
	while ((ec = _decb_readdir(p, &entry)) == 0)
	{
		if (memcmp(entry.filename, "SOURCE  ", 8) == 0)
		{
			found = 1;
			ASSERT_EQUALS(0, memcmp(reserved, &entry.file_status, sizeof(reserved)));
		}
	}
	ASSERT_EQUALS(1, found);
	ASSERT_EC(0, _decb_close(p));

	/* Killing gives back all its granules, across track 17. */
	ASSERT_EC(0, _decb_kill(REAL_COPY ",CROSS.BIN"));
	ASSERT_EC(0, TSDECBFree(REAL_COPY, &free_granules));
	ASSERT_EQUALS(REAL_FREE_GRANULES - 1 + 7, free_granules);
	ASSERT_EC(EOS_PNNF, read_real_file(REAL_COPY, "CROSS.BIN", data, sizeof(data), &size));
	ASSERT_EC(0, read_real_file(REAL_COPY, "AFTER.BIN", data, sizeof(data), &size));
	check_real_pattern(data, size, 7, 4598);
	remove(REAL_COPY);
}

/* A file the ROM wrote at an exact size can be added to. */
static void check_real_append(const char *name, int seed, int length)
{
	static u_char data[20000];
	static const u_char more[] = "more";
	char pathlist[64];
	decb_path_id p;
	u_int size = sizeof(more);
	u_int before = (u_int) length + 10;

	copy_real_disk();
	snprintf(pathlist, sizeof(pathlist), REAL_COPY ",%s", name);
	ASSERT_EC(0, _decb_open(&p, pathlist, FAM_READ | FAM_WRITE));
	ASSERT_EC(0, _decb_seek(p, (int) before, SEEK_SET));
	ASSERT_EC(0, _decb_write(p, (void *) more, &size));
	ASSERT_EQUALS(sizeof(more), size);
	ASSERT_EC(0, _decb_close(p));

	ASSERT_EC(0, _decb_gs_size_pathlist(pathlist, &size));
	ASSERT_EQUALS(before + sizeof(more), size);
	ASSERT_EC(0, read_real_file(REAL_COPY, name, data, sizeof(data), &size));
	ASSERT_EQUALS(before + sizeof(more), size);
	ASSERT_EQUALS(0, memcmp(more, data + before, sizeof(more)));
	check_real_pattern(data, before, seed, length);
	remove(REAL_COPY);
}

void test_real_rsdos_exact_sizes_can_grow()
{
	check_real_append("ONESECT.BIN", 1, 246);
	check_real_append("ONEGRAN.BIN", 3, 2294);
}

/* A file's granules can be missing on the far side of track 17 alone. */
void test_real_rsdos_missing_sector_across_track_17()
{
	static u_char data[20000];
	u_int size;

	copy_real_disk();

	/* Granule 34 is the first after the directory track: track 18. */
	image_remove_sector(REAL_COPY, 18, 0, 1);
	ASSERT_EC(EOS_SECT, read_real_file(REAL_COPY, "CROSS.BIN", data, sizeof(data), &size));

	ASSERT_EC(0, read_real_file(REAL_COPY, "AFTER.BIN", data, sizeof(data), &size));
	check_real_pattern(data, size, 7, 4598);
	remove(REAL_COPY);
}

int main()
{
	remove("test.dsk");
	remove("filldisk.dsk");
	RUN(test_decb_dmk_read_write);
	RUN(test_decb_colon_needs_raw_image);
	RUN(test_decb_only_needed_sectors_are_errors);
	RUN(test_decb_dir_lists_names_without_fat);
	RUN(test_decb_bad_crc_is_read);
	RUN(test_decb_write_protect);
	RUN(test_decb_dskini);
	RUN(test_decb_create);
	RUN(test_decb_read);
	RUN(test_decb_readln_across_granules);
	RUN(test_decb_write);
	RUN(test_decb_overwrite);
	RUN(test_decb_open_and_read);
	RUN(test_decb_delete);
	RUN(test_decb_rename);
	RUN(test_decb_binconcat_last_address);
	RUN(test_decb_extract_first_segment_multiple_segments);
	RUN(test_decb_token);
	RUN(test_decb_token_keyword_after_var);

	RUN(test_decb_raw_path_all_formats);
	RUN(test_decb_raw_write_past_end);
	RUN(test_decb_damage_sdf);
	RUN(test_decb_round_trip_all_formats);
	RUN(test_decb_write_protect_all_formats);
	RUN(test_decb_wrong_sector_size);
	RUN(test_real_rsdos_is_recognized);
	RUN(test_real_rsdos_directory_matches_the_machine);
	RUN(test_real_rsdos_files_match_their_patterns);
	RUN(test_real_rsdos_reads_in_pieces);
	RUN(test_real_rsdos_reads_lines);
	RUN(test_real_rsdos_detokenizes);
	RUN(test_real_rsdos_can_be_changed);
	RUN(test_real_rsdos_exact_sizes_can_grow);
	RUN(test_real_rsdos_missing_sector_across_track_17);
	return TEST_REPORT();
}

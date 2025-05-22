/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>

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
			  "test.dsk,file_doesnt_exist_and_is_much_longer_than_rbf_limit_of_8_characters/and_this_is_an_even_longer_name_than_the_8_character_limit_in9_rbf_because_it_has_more_characters",
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

int main()
{
	remove("test.dsk");
	remove("filldisk.dsk");
	RUN(test_decb_dskini);
	RUN(test_decb_create);
	RUN(test_decb_read);
	RUN(test_decb_write);
	RUN(test_decb_open_and_read);
	RUN(test_decb_delete);
	RUN(test_decb_rename);
	RUN(test_decb_token);

	return TEST_REPORT();
}

/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>

#define TEST_STRING "Line1\nLine2\n"
#define TEST_PROGRAM "10 PRINT \"TEST \\xff\"\n20 GOTO 10\n"
char *test_string = TEST_STRING;
unsigned int test_string_size = sizeof TEST_STRING;
char *basic_program = TEST_PROGRAM;
unsigned int test_program_size = sizeof TEST_PROGRAM;

void test_cecb_bulkerase()
{
	error_code ec;

	// test creation of a non-existent wave file
	ec = _cecb_bulkerase("test8.wav", 44100, 8, 1.0);
	ASSERT_EQUALS(0, ec);

	// test creation of a non-existent wave file
	ec = _cecb_bulkerase("test16.wav", 44100, 16, 1.0);
	ASSERT_EQUALS(0, ec);

	// test creation of a non-existent wave file
	ec = _cecb_bulkerase("test24.wav", 44100, 24, 1.0);
	ASSERT_EQUALS(0, ec);

	// test creation of a non-existent wave file
	ec = _cecb_bulkerase("test32.wav", 44100, 32, 1.0);
	ASSERT_EQUALS(0, ec);

	// test creation of a non-existent cas file
	ec = _cecb_bulkerase("test.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);

	// test creation of a non-existent c10 file
	ec = _cecb_bulkerase("test.c10", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
}

void test_cecb_create()
{
	cecb_path_id p_wav8, p_wav16, p_wav24, p_wav32;
	cecb_path_id p_cas;
	cecb_path_id p_c10;
	error_code ec;
	unsigned int size;

	// Open file written in previous written
	ec = _cecb_create(&p_wav8, "test8.wav,FILE", FAM_READ | FAM_WRITE, 1,
			  255, 0, 0x55, 0x65);
	ASSERT_EQUALS(0, ec);

	// Open file written in previous written
	ec = _cecb_create(&p_wav16, "test16.wav,FILE", FAM_READ | FAM_WRITE, 1,
			  255, 0, 0x55, 0x65);
	ASSERT_EQUALS(0, ec);

	// Open file written in previous written
	ec = _cecb_create(&p_wav24, "test24.wav,FILE", FAM_READ | FAM_WRITE, 1,
			  255, 0, 0x55, 0x65);
	ASSERT_EQUALS(0, ec);

	// Open file written in previous written
	ec = _cecb_create(&p_wav32, "test32.wav,FILE", FAM_READ | FAM_WRITE, 1,
			  255, 0, 0x55, 0x65);
	ASSERT_EQUALS(0, ec);

	// Open file written in previous written
	ec = _cecb_create(&p_cas, "test.cas,FILE", FAM_READ | FAM_WRITE, 1,
			  255, 0, 0x55, 0x65);
	ASSERT_EQUALS(0, ec);

	// Open file written in previous written
	ec = _cecb_create(&p_c10, "test.c10,FILE", FAM_READ | FAM_WRITE, 1,
			  255, 0, 0x55, 0x65);
	ASSERT_EQUALS(0, ec);

	// Write data to file
	size = test_string_size;
	ec = _cecb_write(p_wav8, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);

	// Write data to file
	size = test_string_size;
	ec = _cecb_write(p_wav16, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);

	// Write data to file
	size = test_string_size;
	ec = _cecb_write(p_wav24, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);

	// Write data to file
	size = test_string_size;
	ec = _cecb_write(p_wav32, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);

	// Write data to file
	size = test_string_size;
	ec = _cecb_write(p_cas, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);

	// Write data to file
	size = test_string_size;
	ec = _cecb_write(p_c10, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);

	ec = _cecb_close(p_wav8);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_wav16);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_wav24);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_wav32);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_cas);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_c10);
	ASSERT_EQUALS(0, ec);
}

void test_cecb_read()
{
	cecb_path_id p_wav8, p_wav16, p_wav24, p_wav32;
	cecb_path_id p_cas;
	cecb_path_id p_c10;
	error_code ec;
	char buffer[512];
	unsigned int size;

	// Opening in write mode is not supported
	ec = _cecb_open(&p_wav8, "DoesNoteExit.wav,FILE",
			FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(EOS_IC, ec);

	// Opening in write mode is not supported
	ec = _cecb_open(&p_wav8, "DoesNoteExit.wav,FILE", FAM_READ);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// Opening existing file
	ec = _cecb_open(&p_cas, "DoesNoteExit.cas,FILE", FAM_READ);
	ASSERT_EQUALS(EOS_BPNAM, ec);

	// Opening existing file
	ec = _cecb_open(&p_wav8, "test8.wav,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// Opening existing file
	ec = _cecb_open(&p_wav16, "test16.wav,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// Opening existing file
	ec = _cecb_open(&p_wav24, "test24.wav,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// Opening existing file
	ec = _cecb_open(&p_wav32, "test32.wav,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// Opening existing file
	ec = _cecb_open(&p_cas, "test.cas,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// Opening existing file
	ec = _cecb_open(&p_c10, "test.c10,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);

	// Read file into buffer
	size = 512;
	ec = _cecb_read(p_wav8, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_STRING_EQUALS(test_string, buffer);

	// Read file into buffer
	size = 512;
	ec = _cecb_read(p_wav16, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_STRING_EQUALS(test_string, buffer);

	// Read file into buffer
	size = 512;
	ec = _cecb_read(p_wav24, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_STRING_EQUALS(test_string, buffer);

	// Read file into buffer
	size = 512;
	ec = _cecb_read(p_wav32, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_STRING_EQUALS(test_string, buffer);

	// Read file into buffer
	size = 512;
	buffer[0] = 0;
	ec = _cecb_read(p_cas, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_STRING_EQUALS(test_string, buffer);

	// Read file into buffer
	size = 512;
	buffer[0] = 0;
	ec = _cecb_read(p_c10, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_STRING_EQUALS(test_string, buffer);

	ec = _cecb_close(p_wav8);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_wav16);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_wav24);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_wav32);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_cas);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_close(p_c10);
	ASSERT_EQUALS(0, ec);
}

void test_cecb_token()
{
	error_code ec;
	unsigned char *ent_outbuf;
	u_int ent_out_size;

	char *det_outbuf;
	u_int det_out_size;

	ec = _cecb_entoken((unsigned char *)basic_program, test_program_size, &ent_outbuf, &ent_out_size, 0);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_detoken(ent_outbuf, ent_out_size, &det_outbuf, &det_out_size);
	ASSERT_EQUALS(0, ec);
	
	ASSERT_EQUALS(test_program_size, det_out_size-2);

	ec = memcmp(basic_program, det_outbuf, test_program_size-1);
	ASSERT_EQUALS(0, ec);

	free(ent_outbuf);
	free(det_outbuf);
}

int main()
{
	remove("test.c10");
	remove("test.cas");
	remove("test8.wav");
	remove("test16.wav");
	remove("test24.wav");
	remove("test32.wav");

	RUN(test_cecb_bulkerase);
	RUN(test_cecb_create);
	RUN(test_cecb_read);
	RUN(test_cecb_token);

	return TEST_REPORT();
}

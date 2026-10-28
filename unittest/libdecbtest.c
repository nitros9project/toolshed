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

int main()
{
	remove("test.dsk");
	remove("filldisk.dsk");
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

	return TEST_REPORT();
}

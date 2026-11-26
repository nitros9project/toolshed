/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>
#include <stdint.h>
#include <sys/stat.h>
#include <limits.h>

static unsigned int int4le(const unsigned char *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int) p[3] << 24;
}

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

void test_cecb_readln()
{
	cecb_path_id path;
	error_code ec;
	char data[] = "First\rSecond\r";
	char buffer[32];
	unsigned int size;

	ec = _cecb_bulkerase("readln.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_create(&path, "readln.cas,LINES", FAM_WRITE, 1,
			  255, 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	size = sizeof(data) - 1;
	ec = _cecb_write(path, data, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(data) - 1, size);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_open(&path, "readln.cas,LINES", FAM_READ);
	ASSERT_EQUALS(0, ec);

	/* Stop at the first CR even when the following buffer byte is zero. */
	memset(buffer, 0, sizeof(buffer));
	size = sizeof(buffer);
	ec = _cecb_readln(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(6, size);
	ASSERT_MEM_EQUALS("First\r", buffer, size);
	ASSERT_EQUALS(0, buffer[size]);

	/* Old CRs in the buffer must not cause an early stop. */
	memset(buffer, '\r', sizeof(buffer));
	size = sizeof(buffer);
	ec = _cecb_readln(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(7, size);
	ASSERT_MEM_EQUALS("Second\r", buffer, size);

	size = sizeof(buffer);
	ec = _cecb_readln(path, buffer, &size);
	ASSERT_EQUALS(EOS_EOF, ec);
	ASSERT_EQUALS(0, size);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);
	remove("readln.cas");
}

void test_cecb_machine_language_addresses()
{
	cecb_path_id path;
	cecb_file_stat stat;
	error_code ec;

	ec = _cecb_bulkerase("addresses.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_create(&path, "addresses.cas,ML", FAM_WRITE, 2,
			  0, 0, 0x1234, 0xabcd);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);

	/* Read back the addresses stored in the cassette directory header. */
	ec = _cecb_open(&path, "addresses.cas,ML", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_gs_fd(path, &stat);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);
	remove("addresses.cas");

	ASSERT_EQUALS(0x1234, stat.ml_load_address);
	ASSERT_EQUALS(0xabcd, stat.ml_exec_address);
}

void test_cecb_rewind()
{
	char *files[] = { "test.cas,FILE", "test.c10,FILE",
			  "test8.wav,FILE", "test16.wav,FILE",
			  "test24.wav,FILE", "test32.wav,FILE" };
	cecb_path_id path;
	error_code ec;
	char buffer[512];
	unsigned int size, pos;
	unsigned int i;

	for (i = 0; i < sizeof(files) / sizeof(files[0]); i++)
	{
		ec = _cecb_open(&path, files[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		size = 4;
		ec = _cecb_read(path, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(4, size);
		ASSERT_MEM_EQUALS(test_string, buffer, size);

		/* Rewinding must discard the partially consumed data block. */
		ec = _cecb_seek(path, 0, SEEK_SET);
		ASSERT_EQUALS(0, ec);
		ec = _cecb_gs_pos(path, &pos);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(0, pos);
		size = sizeof(buffer);
		ec = _cecb_read(path, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(test_string_size, size);
		ASSERT_MEM_EQUALS(test_string, buffer, size);

		/* Rewinding after EOF must also make the whole file readable. */
		ASSERT_EQUALS(EOS_EOF, _cecb_gs_eof(path));
		ec = _cecb_seek(path, 0, SEEK_SET);
		ASSERT_EQUALS(0, ec);
		size = sizeof(buffer);
		ec = _cecb_read(path, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(test_string_size, size);
		ASSERT_MEM_EQUALS(test_string, buffer, size);
		ec = _cecb_close(path);
		ASSERT_EQUALS(0, ec);
	}
}

void test_cecb_rejected_seek()
{
	cecb_path_id path;
	error_code ec;
	char buffer[512];
	unsigned int size, pos;

	ec = _cecb_open(&path, "test.cas,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = 4;
	ec = _cecb_read(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(4, size);

	/* An unsupported absolute seek must leave the stream unchanged. */
	ec = _cecb_seek(path, 2, SEEK_SET);
	ASSERT_NEQUALS(0, ec);
	ec = _cecb_gs_pos(path, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(4, pos);
	size = sizeof(buffer);
	ec = _cecb_read(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size - 4, size);
	ASSERT_MEM_EQUALS(test_string + 4, buffer, size);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);
}

void test_cecb_wav_trailing_chunk()
{
	/* An even-sized RIFF chunk following the audio data. */
	const unsigned char chunk[] = {
		'J', 'U', 'N', 'K', 4, 0, 0, 0, 't', 'e', 's', 't'
	};
	unsigned char result[sizeof(chunk)];
	cecb_path_id path;
	error_code ec;
	FILE *file;
	long original_size, final_size, data_end;
	unsigned int riff_size, size;
	size_t count;

	ec = _cecb_bulkerase("trailing.wav", 44100, 8, 1.0);
	ASSERT_EQUALS(0, ec);
	file = fopen("trailing.wav", "rb+");
	ASSERT("open WAV for metadata", file != NULL);
	ec = fseek(file, 0, SEEK_END);
	ASSERT_EQUALS(0, ec);
	original_size = ftell(file);
	count = fwrite(chunk, 1, sizeof(chunk), file);
	ASSERT_EQUALS(sizeof(chunk), count);
	ec = fseek(file, 4, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	fwrite_le_int(original_size + sizeof(chunk) - 8, file);
	ec = fclose(file);
	ASSERT_EQUALS(0, ec);

	ec = _cecb_create(&path, "trailing.wav,FILE", FAM_WRITE,
			  1, 255, 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	size = test_string_size;
	ec = _cecb_write(path, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);

	/* Reopen to locate the new end of audio, then check exact metadata
	   and physical size: no heap bytes may be appended after the chunk. */
	ec = _cecb_open(&path, "trailing.wav,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);
	data_end = path->wav_data_start + path->wav_data_length;
	riff_size = path->wav_riff_size;
	ec = fseek(path->fd, data_end, SEEK_SET);
	ASSERT_EQUALS(0, ec);
	count = fread(result, 1, sizeof(result), path->fd);
	ec = fseek(path->fd, 0, SEEK_END);
	ASSERT_EQUALS(0, ec);
	final_size = ftell(path->fd);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);
	remove("trailing.wav");
	ASSERT_EQUALS(sizeof(chunk), count);
	ASSERT_MEM_EQUALS(chunk, result, sizeof(chunk));
	ASSERT_EQUALS(data_end + (long) sizeof(chunk), final_size);
	ASSERT_EQUALS(final_size - 8, riff_size);
}

/* The header bulkerase writes must agree with the file it writes. */
void test_cecb_bulkerase_header()
{
	const int bits[] = {8, 16, 24, 32};
	unsigned char header[44];
	struct stat info;
	size_t count;
	FILE *file;
	int i, stat_ec;
	error_code ec;

	for (i = 0; i < 4; i++)
	{
		ec = _cecb_bulkerase("header.wav", 44100, bits[i], 0.25);
		ASSERT_EQUALS(0, ec);
		stat_ec = stat("header.wav", &info);
		ASSERT_EQUALS(0, stat_ec);
		file = fopen("header.wav", "rb");
		ASSERT("open WAV header", file != NULL);
		count = fread(header, 1, sizeof(header), file);
		fclose(file);
		remove("header.wav");

		ASSERT_EQUALS(sizeof(header), count);
		ASSERT_EQUALS((unsigned int) info.st_size - 8, int4le(header + 4));
		ASSERT_EQUALS(16, int4le(header + 16));
		ASSERT_EQUALS(bits[i] / 8, header[32] | header[33] << 8);
		ASSERT_EQUALS(bits[i], header[34] | header[35] << 8);
		ASSERT_EQUALS((unsigned int) info.st_size - 44, int4le(header + 40));
	}
}

static void put_le(FILE *file, unsigned long long value, int size)
{
	int i;

	for (i = 0; i < size; i++)
		fputc((int) ((value >> (8 * i)) & 0xff), file);
}

/* Write a tape holding test_string as 16-bit PCM, then copy its audio to
   'name' in another format: PCM or float (format 3), with the fmt chunk
   given 'fmt_size' bytes and, if 'extra' is set, an odd-sized LIST chunk
   placed between the fmt and data chunks. */
static void make_converted_tape(const char *name, int format, int bits,
				int fmt_size, int extra)
{
	unsigned char header[44];
	unsigned int data_length, samples, i, size;
	unsigned int out_length;
	cecb_path_id path;
	error_code ec;
	size_t count;
	FILE *in, *out;

	ec = _cecb_bulkerase("convert16.wav", 44100, 16, 0.5);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_create(&path, "convert16.wav,FILE", FAM_WRITE, 1, 255, 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	size = test_string_size;
	ec = _cecb_write(path, test_string, &size);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);

	in = fopen("convert16.wav", "rb");
	ASSERT("open 16-bit tape", in != NULL);
	count = fread(header, 1, sizeof(header), in);
	ASSERT_EQUALS(sizeof(header), count);
	data_length = int4le(header + 40);
	samples = data_length / 2;
	out_length = samples * (bits / 8);

	out = fopen(name, "wb");
	ASSERT("create converted tape", out != NULL);
	fwrite("RIFF", 1, 4, out);
	put_le(out, 4 + 8 + fmt_size + (extra ? 8 + 6 : 0) + 8 + out_length, 4);
	fwrite("WAVEfmt ", 1, 8, out);
	put_le(out, fmt_size, 4);
	put_le(out, format, 2);
	put_le(out, 1, 2);
	put_le(out, 44100, 4);
	put_le(out, 44100 * (bits / 8), 4);
	put_le(out, bits / 8, 2);
	put_le(out, bits, 2);
	for (i = 16; i < (unsigned int) fmt_size; i++)
		fputc(0, out);
	if (extra)
	{
		/* Five bytes and a pad byte. */
		fwrite("LIST", 1, 4, out);
		put_le(out, 5, 4);
		fwrite("INFO!", 1, 5, out);
		fputc(0, out);
	}
	fwrite("data", 1, 4, out);
	put_le(out, out_length, 4);

	for (i = 0; i < samples; i++)
	{
		int lo = fgetc(in), hi = fgetc(in);
		short sample = (short) (lo | hi << 8);
		double value = sample / 32768.0;

		if (format == 1)
			put_le(out, (unsigned short) sample, 2);
		else if (bits == 32)
		{
			float f = (float) value;
			uint32_t u;

			memcpy(&u, &f, sizeof(u));
			put_le(out, u, 4);
		}
		else
		{
			uint64_t u;

			memcpy(&u, &value, sizeof(u));
			put_le(out, u, 8);
		}
	}

	fclose(in);
	fclose(out);
	remove("convert16.wav");
}

static void check_converted_tape(const char *name)
{
	char pathlist[64], buffer[64];
	cecb_path_id path;
	error_code ec, read_ec, close_ec;
	unsigned int size;
	int length;

	length = snprintf(pathlist, sizeof(pathlist), "%s,FILE", name);
	ASSERT("pathlist fits", length > 0 && (size_t) length < sizeof(pathlist));
	ec = _cecb_open(&path, pathlist, FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(buffer);
	read_ec = _cecb_read(path, buffer, &size);
	close_ec = _cecb_close(path);
	ASSERT_EQUALS(0, read_ec);
	ASSERT_EQUALS(0, close_ec);
	ASSERT_EQUALS(test_string_size, size);
	ASSERT_MEM_EQUALS(test_string, buffer, size);
}

/* Audio editors put chunks such as LIST between fmt and data. */
void test_cecb_wav_chunk_before_data()
{
	make_converted_tape("listchunk.wav", 1, 16, 16, 1);
	check_converted_tape("listchunk.wav");
	remove("listchunk.wav");
}

/* Float tapes are read, and writing to one is refused, not fatal. */
void test_cecb_wav_float()
{
	cecb_path_id path;
	error_code ec;

	make_converted_tape("float32.wav", 3, 32, 18, 0);
	check_converted_tape("float32.wav");
	ec = _cecb_create(&path, "float32.wav,NEW", FAM_WRITE, 1, 255, 0, 0, 0);
	ASSERT_EQUALS(EOS_WT, ec);
	remove("float32.wav");

	make_converted_tape("float64.wav", 3, 64, 16, 1);
	check_converted_tape("float64.wav");
	remove("float64.wav");
}

void test_cecb_write_read_only()
{
	cecb_path_id path;
	error_code ec;
	char buffer[512];
	char data[] = "X";
	unsigned int size, pos;

	ec = _cecb_open(&path, "test.cas,FILE", FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = 4;
	ec = _cecb_read(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(4, size);

	/* Reject the write without changing the buffered data or position. */
	size = sizeof(data) - 1;
	ec = _cecb_write(path, data, &size);
	ASSERT_EQUALS(EOS_BMODE, ec);
	ec = _cecb_gs_pos(path, &pos);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(4, pos);
	size = sizeof(buffer);
	ec = _cecb_read(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(test_string_size - 4, size);
	ASSERT_MEM_EQUALS(test_string + 4, buffer, size);
	ec = _cecb_close(path);
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

/* Write one file holding 'data' to the end of a cassette image. */
static void append_cas_file(const char *pathlist, const char *data,
			    unsigned int length)
{
	cecb_path_id path;
	error_code ec;
	unsigned int size = length;

	ec = _cecb_create(&path, (char *) pathlist, FAM_WRITE, 1, 255, 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _cecb_write(path, (void *) data, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(length, size);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);
}

static long file_size(const char *name)
{
	struct stat info;

	if (stat(name, &info) != 0)
		return -1;

	return (long) info.st_size;
}

/* A name has no extension, so none may be added, and a full
   eight character name must fit the caller's buffer. */
void test_cecb_ncpy_name()
{
	cecb_dir_entry entry;
	u_char name[32];
	error_code ec;

	memset(&entry, 0, sizeof(entry));
	memcpy(entry.filename, "FILE    ", 8);
	memset(name, 'x', sizeof(name));
	ec = _cecb_ncpy_name(entry, name, sizeof(name));
	ASSERT_EQUALS(0, ec);
	ASSERT_STRING_EQUALS("FILE", (char *) name);

	memcpy(entry.filename, "ABCDEFGH", 8);
	memset(name, 'x', sizeof(name));
	ec = _cecb_ncpy_name(entry, name, sizeof(name));
	ASSERT_EQUALS(0, ec);
	ASSERT_STRING_EQUALS("ABCDEFGH", (char *) name);
}

/* Every byte written to a cassette image reaches the file: a file
   written to an empty image ends with the whole of its trailing
   silence (0.58 seconds, 11 bytes), not one byte short. */
void test_cecb_cas_last_byte()
{
	/* 10 silence, 130 leader, 21 header block, 11 gap, 130 leader,
	   7 data block, 6 end block, 11 silence. */
	const long expected = 10 + 130 + 21 + 11 + 130 + 7 + 6 + 11;
	error_code ec;

	remove("lastbyte.cas");
	ec = _cecb_bulkerase("lastbyte.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	append_cas_file("lastbyte.cas,ONE", "X", 1);
	ASSERT_EQUALS(expected, file_size("lastbyte.cas"));
	remove("lastbyte.cas");
}

/* Writing that starts part way through a byte of the image keeps the
   bits of that byte that come before it: appending to a tape that ends
   in a $55 trailer byte must not wipe that byte's low seven bits. */
void test_cecb_cas_append_keeps_bits()
{
	unsigned char byte = 0x55;
	error_code ec;
	FILE *file;
	size_t count;
	int c;

	remove("keepbits.cas");
	file = fopen("keepbits.cas", "wb");
	ASSERT("create image", file != NULL);
	count = fwrite(&byte, 1, 1, file);
	fclose(file);
	ASSERT_EQUALS(1, count);

	/* Append, as cecb copy does. The first bit written is silence. */
	cecb_start_sample = LONG_MAX;
	append_cas_file("keepbits.cas,FILE", "X", 1);
	cecb_start_sample = 0;

	file = fopen("keepbits.cas", "rb");
	ASSERT("open image", file != NULL);
	c = fgetc(file);
	fclose(file);
	ASSERT_EQUALS(0x55, c);

	/* And the file written after it can still be read. */
	{
		cecb_path_id path;
		char buffer[8];
		unsigned int size = sizeof(buffer);

		ec = _cecb_open(&path, "keepbits.cas,FILE", FAM_READ);
		ASSERT_EQUALS(0, ec);
		ec = _cecb_read(path, buffer, &size);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(1, size);
		ASSERT_EQUALS('X', buffer[0]);
		ec = _cecb_close(path);
		ASSERT_EQUALS(0, ec);
	}
	remove("keepbits.cas");
}

/* The global start position (cecb -s) names a bit in a CAS image;
   reading starts there, so a file before it is not seen. */
void test_cecb_cas_start_bit()
{
	cecb_path_id path;
	error_code ec;
	char buffer[64];
	unsigned int size;
	long first_end;

	remove("startbit.cas");
	ec = _cecb_bulkerase("startbit.cas", 0, 0, 0);
	ASSERT_EQUALS(0, ec);
	cecb_start_sample = LONG_MAX;
	append_cas_file("startbit.cas,SAME", "first", 5);
	first_end = file_size("startbit.cas");
	ASSERT("first file written", first_end > 0);
	append_cas_file("startbit.cas,SAME", "second", 6);
	cecb_start_sample = 0;

	/* From the start, the first of the two files is found. */
	ec = _cecb_open(&path, "startbit.cas,SAME", FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(buffer);
	ec = _cecb_read(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(5, size);
	ASSERT_MEM_EQUALS("first", buffer, size);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);

	/* Starting past the first file finds the second. */
	cecb_start_sample = first_end * 8;
	ec = _cecb_open(&path, "startbit.cas,SAME", FAM_READ);
	cecb_start_sample = 0;
	ASSERT_EQUALS(0, ec);
	size = sizeof(buffer);
	ec = _cecb_read(path, buffer, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(6, size);
	ASSERT_MEM_EQUALS("second", buffer, size);
	ec = _cecb_close(path);
	ASSERT_EQUALS(0, ec);
	remove("startbit.cas");
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
	RUN(test_cecb_readln);
	RUN(test_cecb_machine_language_addresses);
	RUN(test_cecb_rewind);
	RUN(test_cecb_rejected_seek);
	RUN(test_cecb_wav_trailing_chunk);
	RUN(test_cecb_write_read_only);
	RUN(test_cecb_token);
	RUN(test_cecb_bulkerase_header);
	RUN(test_cecb_wav_chunk_before_data);
	RUN(test_cecb_wav_float);
	RUN(test_cecb_ncpy_name);
	RUN(test_cecb_cas_last_byte);
	RUN(test_cecb_cas_append_keeps_bits);
	RUN(test_cecb_cas_start_bit);

	return TEST_REPORT();
}

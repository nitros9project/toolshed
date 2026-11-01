/* Tests for the dgndos command. */

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "tinytest.h"
#include "toolshed.h"

#define DGNDOS "../dgndos/dgndos "
#define QUIET " > /dev/null 2>&1"
#define TEST_IMAGE "dgndoscmd.vdk"
#define MAME_IMAGE "\"../../../unittest/testdata/dragondos mame.vdk\""
#define OUTPUT_FILE "dgndoscmd_output.txt"
#define NATIVE_FILE "dgndoscmd_native.bin"
#define NATIVE_OTHER "dgndoscmd_other.bin"
#define NATIVE_DIR "dgndoscmd_dir"
#define CAPTURE " > " OUTPUT_FILE " 2>&1"
#define KEEP_FILE "dgndoscmd_keep.bin"
#define KEEP_NAMED "dgndoscmd_keep.DSK"

static char output[16384];
static u_char data[65536];
static u_char back[65536];

/* Run a command and get its exit status. */
static int run(const char *command)
{
	int status = system(command);

	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int run_status;
#define ASSERT_RUN(expected, command) \
	{ \
		run_status = run(command); \
		ASSERT_EQUALS(expected, run_status); \
	}

static void read_output(void)
{
	FILE *fp = fopen(OUTPUT_FILE, "rb");
	size_t got = 0;

	if (fp != NULL)
	{
		got = fread(output, 1, sizeof(output) - 1, fp);
		fclose(fp);
	}
	output[got] = '\0';
}

static long file_size(const char *pathname, u_char *buffer, long size)
{
	FILE *fp = fopen(pathname, "rb");
	long got;

	if (fp == NULL)
	{
		return -1;
	}
	got = (long)fread(buffer, 1, size, fp);
	fclose(fp);
	return got;
}

static void write_native(const char *pathname, u_int size)
{
	FILE *fp = fopen(pathname, "wb");
	u_int i;

	for (i = 0; i < size; i++)
	{
		data[i] = (u_char)(i * 13 + (i >> 8));
	}
	fwrite(data, 1, size, fp);
	fclose(fp);
}

static void clean_up(void)
{
	remove(TEST_IMAGE);
	remove(OUTPUT_FILE);
	remove(NATIVE_FILE);
	remove(NATIVE_OTHER);
	remove(KEEP_FILE);
	remove(KEEP_NAMED);
	remove(NATIVE_DIR "/" NATIVE_FILE);
	rmdir(NATIVE_DIR);
}

static void test_help(void)
{
	ASSERT_RUN(0, DGNDOS CAPTURE);
	read_output();
	ASSERT("the commands are listed", strstr(output, "dskinit") != NULL && strstr(output, "rename") != NULL);

	ASSERT_RUN(0, DGNDOS "copy" CAPTURE);
	read_output();
	ASSERT("a command with no arguments gives help", strstr(output, "Syntax: copy") != NULL);

	ASSERT_RUN(1, DGNDOS "nosuchcommand" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("dgndos: unknown command 'nosuchcommand'\n", output);
	ASSERT_RUN(1, DGNDOS "dir -z x" QUIET);
	remove(OUTPUT_FILE);
}

static void test_dskinit(void)
{
	static u_char image[1024 * 1024];

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	ASSERT_EQUALS(12 + 40 * 18 * 256, file_size(TEST_IMAGE, image, sizeof(image)));
	ASSERT_MEM_EQUALS("dk", image, 2);
	ASSERT_RUN(EOS_FAE, DGNDOS "dskinit " TEST_IMAGE QUIET);
	remove(TEST_IMAGE);

	/* Options win over the extension. */
	ASSERT_RUN(0, DGNDOS "dskinit -t80 -s2 -fraw " TEST_IMAGE);
	ASSERT_EQUALS(80 * 2 * 18 * 256, file_size(TEST_IMAGE, image, sizeof(image)));
	ASSERT_RUN(0, DGNDOS "free " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("Free sectors: 2844 (728064 bytes)\n", output);
	remove(TEST_IMAGE);

	ASSERT_RUN(1, DGNDOS "dskinit -t20 " TEST_IMAGE QUIET);
	ASSERT_RUN(1, DGNDOS "dskinit -s3 " TEST_IMAGE QUIET);
	ASSERT_RUN(1, DGNDOS "dskinit -fnope " TEST_IMAGE QUIET);
	ASSERT_RUN(0, DGNDOS "dskinit -fdmk " TEST_IMAGE);
	ASSERT_RUN(0, DGNDOS "free " TEST_IMAGE QUIET);
	clean_up();
}

static void test_dir_of_mame_disk(void)
{
	ASSERT_RUN(0, DGNDOS "dir " MAME_IMAGE CAPTURE);
	read_output();
	ASSERT("the heading names the disk", strstr(output, "Directory of: ../../../unittest/testdata/dragondos mame.vdk,\n\n") != NULL);
	ASSERT("the first file is listed", strstr(output, "\nHW1      BAS  .       41\n") != NULL);
	ASSERT("the last file is listed", strstr(output, "\nHW5      BAS  .       41\n") != NULL);
	ASSERT("the total is given", strstr(output, "\n5 files, 679 sectors free (173824 bytes)\n") != NULL);
	remove(OUTPUT_FILE);

	ASSERT_RUN(EOS_PNNF, DGNDOS "dir dgndoscmd_missing.vdk" QUIET);
}

static void test_copy_round_trip(void)
{
	long size;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 5000);

	ASSERT_RUN(EOS_PNNF, DGNDOS "copy dgndoscmd_missing " TEST_IMAGE ",X" QUIET);
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",FILE.BIN");
	ASSERT_RUN(EOS_FAE, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",FILE.BIN" QUIET);
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",FILE.BIN");
	ASSERT_RUN(EOS_BPNAM, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",B@D" QUIET);

	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",file.bin " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(5000, size);
	ASSERT_MEM_EQUALS(data, back, 5000);

	/* Within a disk too. */
	remove(NATIVE_OTHER);
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",FILE.BIN " TEST_IMAGE ",SECOND");
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",SECOND " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(5000, size);
	ASSERT_MEM_EQUALS(data, back, 5000);
	clean_up();
}

static void test_copy_to_directories(void)
{
	long size;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 300);
	write_native(NATIVE_OTHER, 300);

	/* These names are too long for Dragon DOS, so they're cut short. */
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",");
	ASSERT_RUN(0, DGNDOS "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the name was cut short", strstr(output, "\nDGNDOSCM      .      300\n") != NULL);

	ASSERT_RUN(1, DGNDOS "copy " NATIVE_FILE " " NATIVE_OTHER " " TEST_IMAGE ",SINGLE" QUIET);

	run_status = mkdir(NATIVE_DIR, 0755);
	ASSERT_EQUALS(0, run_status);
	remove(NATIVE_FILE);
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",DGNDOSCM " NATIVE_DIR "/" NATIVE_FILE);
	size = file_size(NATIVE_DIR "/" NATIVE_FILE, back, sizeof(back));
	ASSERT_EQUALS(300, size);
	remove(NATIVE_DIR "/" NATIVE_FILE);

	ASSERT_RUN(0, DGNDOS "copy " NATIVE_OTHER " " NATIVE_DIR);
	size = file_size(NATIVE_DIR "/" NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(300, size);
	remove(NATIVE_DIR "/" NATIVE_OTHER);
	clean_up();
}

static void test_copy_too_large_leaves_nothing(void)
{
	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit -t21 -fraw " TEST_IMAGE);
	ASSERT_RUN(0, DGNDOS "free " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("Free sectors: 342 (87552 bytes)\n", output);

	write_native(NATIVE_FILE, sizeof(data));
	ASSERT_RUN(0, "cat " NATIVE_FILE " " NATIVE_FILE " > " NATIVE_OTHER);
	ASSERT_RUN(EOS_DF, DGNDOS "copy " NATIVE_OTHER " " TEST_IMAGE ",BIG" QUIET);
	ASSERT_RUN(EOS_PNNF, DGNDOS "fstat " TEST_IMAGE ",BIG" QUIET);
	ASSERT_RUN(0, DGNDOS "free " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("Free sectors: 342 (87552 bytes)\n", output);
	clean_up();
}

static void test_list_translates_line_ends(void)
{
	FILE *fp;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	fp = fopen(NATIVE_FILE, "wb");
	fputs("ONE\nTWO\n", fp);
	fclose(fp);

	ASSERT_RUN(0, DGNDOS "copy -l " NATIVE_FILE " " TEST_IMAGE ",TEXT");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",TEXT" CAPTURE);
	read_output();
	ASSERT("the file has the same length", strstr(output, "File size          : 8 bytes") != NULL);
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",TEXT " NATIVE_OTHER);
	ASSERT_EQUALS(8, file_size(NATIVE_OTHER, back, sizeof(back)));
	ASSERT_MEM_EQUALS("ONE\rTWO\r", back, 8);

	ASSERT_RUN(0, DGNDOS "list " TEST_IMAGE ",TEXT" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("ONE\nTWO\n", output);

	ASSERT_RUN(EOS_PNNF, DGNDOS "list " TEST_IMAGE ",NOPE" QUIET);
	clean_up();
}

static void test_fstat_and_dump(void)
{
	ASSERT_RUN(0, DGNDOS "fstat " MAME_IMAGE ",HW2.BAS" CAPTURE);
	read_output();
	ASSERT("the name", strstr(output, "Filename           : HW2.BAS\n") != NULL);
	ASSERT("the size", strstr(output, "File size          : 41 bytes\n") != NULL);
	ASSERT("the entry", strstr(output, "First entry        : 1\n") != NULL);
	ASSERT("the extent", strstr(output, "Extents            : [252:1] \n") != NULL);
	ASSERT("the entries", strstr(output, "Directory entries  : 1\n") != NULL);
	ASSERT("the type", strstr(output, "File type          : 1 (BASIC program)\n") != NULL);
	ASSERT("the load address", strstr(output, "Load address       : $2401\n") != NULL);
	ASSERT("the data length", strstr(output, "Data length        : 32 bytes\n") != NULL);
	ASSERT("the exec address", strstr(output, "Exec address       : $8B8D\n") != NULL);

	ASSERT_RUN(0, DGNDOS "dump " MAME_IMAGE ",HW2.BAS" CAPTURE);
	read_output();
	ASSERT("the file's first byte is shown", strstr(output, "55") != NULL);
	remove(OUTPUT_FILE);
}

static void test_attr_rename_kill(void)
{
	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 100);
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",ONE.BIN");
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",TWO.BIN");

	ASSERT_RUN(0, DGNDOS "attr " TEST_IMAGE ",ONE.BIN" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS(TEST_IMAGE ",ONE.BIN: not protected\n", output);

	ASSERT_RUN(0, DGNDOS "attr -p " TEST_IMAGE ",ONE.BIN");
	ASSERT_RUN(0, DGNDOS "attr " TEST_IMAGE ",ONE.BIN" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS(TEST_IMAGE ",ONE.BIN: protected\n", output);
	ASSERT_RUN(0, DGNDOS "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the listing shows protection", strstr(output, "\nONE      BIN  P      100\n") != NULL);

	ASSERT_RUN(EOS_FNA, DGNDOS "kill " TEST_IMAGE ",ONE.BIN" CAPTURE);
	read_output();
	ASSERT("the error is described", strstr(output, "permissions") != NULL);
	ASSERT_RUN(EOS_FNA, DGNDOS "rename " TEST_IMAGE ",ONE.BIN NEW.BIN" QUIET);
	ASSERT_RUN(EOS_FNA, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",ONE.BIN" QUIET);

	ASSERT_RUN(0, DGNDOS "attr -u " TEST_IMAGE ",ONE.BIN");
	ASSERT_RUN(EOS_FAE, DGNDOS "rename " TEST_IMAGE ",ONE.BIN TWO.BIN" QUIET);
	ASSERT_RUN(EOS_BPNAM, DGNDOS "rename " TEST_IMAGE ",ONE.BIN B@D" QUIET);
	ASSERT_RUN(0, DGNDOS "rename " TEST_IMAGE ",ONE.BIN new.bin");

	ASSERT_RUN(0, DGNDOS "kill " TEST_IMAGE ",NEW.BIN " TEST_IMAGE ",TWO.BIN");
	ASSERT_RUN(EOS_PNNF, DGNDOS "kill " TEST_IMAGE ",TWO.BIN" QUIET);
	ASSERT_RUN(0, DGNDOS "free " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("Free sectors: 684 (175104 bytes)\n", output);
	clean_up();
}

/* Remove a sector from a DMK image by deleting its ID pointer. */
static void dmk_remove_sector(u_char *image, u_int cylinder, u_int sector)
{
	long track_length = image[2] | (image[3] << 8);
	u_char *track = image + 16 + cylinder * track_length;
	int i;

	for (i = 0; i < 64; i++)
	{
		long pointer = (track[i * 2] | (track[i * 2 + 1] << 8)) & 0x3FFF;

		if (pointer != 0 && track[pointer + 3] == sector)
		{
			memmove(track + i * 2, track + (i + 1) * 2, (size_t)(63 - i) * 2);
			track[126] = track[127] = 0;
			return;
		}
	}
}

/* A disk that lacks part of its directory lists what it can, then reports it. */
static void test_dir_of_damaged_image(void)
{
	static u_char image[1024 * 1024];
	FILE *fp;
	long size;
	int i;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit -fdmk " TEST_IMAGE);
	write_native(NATIVE_FILE, 10);
	for (i = 0; i < 12; i++)
	{
		char command[256];

		sprintf(command, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",F%d", i);
		ASSERT_RUN(0, command);
	}

	/* Without one copy of the first directory sector, the other answers. */
	size = file_size(TEST_IMAGE, image, sizeof(image));
	dmk_remove_sector(image, 20, 3);
	fp = fopen(TEST_IMAGE, "wb");
	fwrite(image, 1, size, fp);
	fclose(fp);
	ASSERT_RUN(0, DGNDOS "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the copy's files are listed", strstr(output, "\nF0 ") != NULL);
	ASSERT("the next sector's files are listed", strstr(output, "\nF11 ") != NULL);

	/* Without either, its files are lost but those after are listed. */
	dmk_remove_sector(image, 16, 3);
	fp = fopen(TEST_IMAGE, "wb");
	fwrite(image, 1, size, fp);
	fclose(fp);
	ASSERT_RUN(EOS_SECT, DGNDOS "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the lost sector's files are not listed", strstr(output, "\nF0 ") == NULL);
	ASSERT("the next sector's files are listed", strstr(output, "\nF11 ") != NULL);
	ASSERT("the total counts what was listed", strstr(output, "\n2 files, ") != NULL);
	ASSERT("the error is described", strstr(output, "sector not found") != NULL);
	clean_up();
}

static const char hello_world[] = "10 PRINT \"HELLO WORLD\"\n20 GOTO 10\n";

static void test_list_tokens(void)
{
	ASSERT_RUN(0, DGNDOS "list -t " MAME_IMAGE ",HW1.BAS" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS(hello_world, output);

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 100);
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",DATA");
	ASSERT_RUN(EOS_WT, DGNDOS "list -t " TEST_IMAGE ",DATA" QUIET);
	clean_up();
}

/* Text in and out of a disk, by way of tokens. */
static void test_copy_tokens(void)
{
	static u_char mame[1024 * 1024];
	static u_char image[1024 * 1024];
	FILE *fp;
	long size;
	int i;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	fp = fopen(NATIVE_FILE, "wb");
	fputs(hello_world, fp);
	fclose(fp);

	/* Saving the same program as MAME did gives the same disk. */
	for (i = 1; i <= 5; i++)
	{
		char command[256];

		sprintf(command, DGNDOS "copy -t " NATIVE_FILE " " TEST_IMAGE ",HW%d.BAS", i);
		ASSERT_RUN(0, command);
	}
	size = file_size(TEST_IMAGE, image, sizeof(image));
	ASSERT_EQUALS(12 + 40 * 18 * 256, size);
	ASSERT_EQUALS(size, file_size("../../../unittest/testdata/dragondos mame.vdk", mame, sizeof(mame)));
	ASSERT_MEM_EQUALS(mame + 12, image + 12, size - 12);

	/* Back to the host as text. */
	ASSERT_RUN(0, DGNDOS "copy -t " TEST_IMAGE ",HW1.BAS " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(sizeof(hello_world) - 1, size);
	ASSERT_MEM_EQUALS(hello_world, back, size);
	ASSERT_RUN(EOS_FAE, DGNDOS "copy -t " NATIVE_FILE " " TEST_IMAGE ",HW1.BAS" QUIET);
	ASSERT_RUN(0, DGNDOS "copy -t -r " NATIVE_FILE " " TEST_IMAGE ",HW1.BAS");

	/* As text on the disk, its lines end the way Dragon DOS's do. */
	ASSERT_RUN(0, DGNDOS "copy -t " TEST_IMAGE ",HW1.BAS " TEST_IMAGE ",HW1.TXT");
	remove(NATIVE_OTHER);
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",HW1.TXT " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(sizeof(hello_world) - 1, size);
	ASSERT_MEM_EQUALS("10 PRINT \"HELLO WORLD\"\r20 GOTO 10\r", back, size);

	/* And that text tokenizes back again. */
	ASSERT_RUN(0, DGNDOS "copy -t " TEST_IMAGE ",HW1.TXT " TEST_IMAGE ",HW6.BAS");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",HW6.BAS" CAPTURE);
	read_output();
	ASSERT("a BASIC program again", strstr(output, "File type          : 1 (BASIC program)\n") != NULL);

	/* Text that isn't a program. */
	fp = fopen(NATIVE_FILE, "wb");
	fputs("not a program\n", fp);
	fclose(fp);
	ASSERT_RUN(1, DGNDOS "copy -t " NATIVE_FILE " " TEST_IMAGE ",BAD" CAPTURE);
	read_output();
	ASSERT("the error is described", strstr(output, "Error: 257") != NULL);
	ASSERT_RUN(EOS_PNNF, DGNDOS "fstat " TEST_IMAGE ",BAD" QUIET);
	clean_up();
}

static void write_bytes(const char *pathname, const u_char *bytes, size_t size)
{
	FILE *fp = fopen(pathname, "wb");

	fwrite(bytes, 1, size, fp);
	fclose(fp);
}

static void test_fstat_file_headers(void)
{
	static u_char image[1024 * 1024];
	static const u_char machine_code[] = { 0x55, 0x02, 0x30, 0x00, 0x01, 0x00, 0x30, 0x05, 0xAA, 0x39 };
	static const u_char unknown_type[] = { 0x55, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA };
	static const u_char no_end_mark[] = { 0x55, 0x02, 0x30, 0x00, 0x00, 0x10, 0x30, 0x05, 0xAB };
	static const u_char too_short[] = { 0x55, 0x02, 0x30, 0x00, 0x00, 0x10, 0x30, 0x05 };
	FILE *fp;
	long size;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit -fdmk " TEST_IMAGE);

	write_bytes(NATIVE_FILE, machine_code, sizeof(machine_code));
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",CODE.BIN");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",CODE.BIN" CAPTURE);
	read_output();
	ASSERT("the type", strstr(output, "File type          : 2 (Machine code)\n") != NULL);
	ASSERT("the load address", strstr(output, "Load address       : $3000\n") != NULL);
	ASSERT("the data length", strstr(output, "Data length        : 256 bytes\n") != NULL);
	ASSERT("the exec address", strstr(output, "Exec address       : $3005\n") != NULL);

	write_bytes(NATIVE_FILE, unknown_type, sizeof(unknown_type));
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",ODD");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",ODD" CAPTURE);
	read_output();
	ASSERT("an undefined type", strstr(output, "File type          : 7 (Unknown)\n") != NULL);

	write_bytes(NATIVE_FILE, no_end_mark, sizeof(no_end_mark));
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",NOEND");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",NOEND" CAPTURE);
	read_output();
	ASSERT("no end mark", strstr(output, "File header        : None\n") != NULL);

	write_bytes(NATIVE_FILE, too_short, sizeof(too_short));
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",SHORT");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",SHORT" CAPTURE);
	read_output();
	ASSERT("too short", strstr(output, "File header        : None\n") != NULL);

	write_bytes(NATIVE_FILE, too_short, 0);
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " TEST_IMAGE ",EMPTY");
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",EMPTY" CAPTURE);
	read_output();
	ASSERT("empty", strstr(output, "File header        : None\n") != NULL);

	/* The first file went at LSN 324.  Without it the rest still reports. */
	size = file_size(TEST_IMAGE, image, sizeof(image));
	dmk_remove_sector(image, 18, 1);
	fp = fopen(TEST_IMAGE, "wb");
	fwrite(image, 1, size, fp);
	fclose(fp);
	ASSERT_RUN(0, DGNDOS "fstat " TEST_IMAGE ",CODE.BIN" CAPTURE);
	read_output();
	ASSERT("the header is out of reach", strstr(output, "File header        : Unreadable\n") != NULL);
	ASSERT("the size is still known", strstr(output, "File size          : 10 bytes\n") != NULL);
	clean_up();
}

/* Refusing leaves the file as it was, and says why once. */
static void check_refused(const char *target, const char *reason)
{
	static u_char before[1024 * 1024];
	static u_char after[1024 * 1024];
	static const char *const formats[] = { "copy ", "copy -r ", "copy -l -r ", "copy -t -r " };
	long size = file_size(target, before, sizeof(before));
	size_t i;

	ASSERT("the file to protect exists", size >= 0);
	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
	{
		char command[256];

		sprintf(command, DGNDOS "%s" TEST_IMAGE ",%s %s" CAPTURE, formats[i], i == 3 ? "PROGRAM" : "FILE", target);
		ASSERT_RUN(1, command);
		read_output();
		ASSERT("it says why", strstr(output, "looks like a disk image") != NULL);
		ASSERT(reason, strstr(output, reason) != NULL);
		ASSERT("it says so once", strstr(output, "Error:") == NULL && strstr(output, "error ") == NULL);
		ASSERT_EQUALS(size, file_size(target, after, sizeof(after)));
		ASSERT_MEM_EQUALS(before, after, size);
	}
}

static void test_copy_refuses_to_overwrite_disk_images(void)
{
	static const char *const image_formats[][2] =
	{
		{ "vdk", "VDK header" }, { "dmk", "DMK header" }, { "sdf", "SDF header" },
		{ "raw", "whole number of sectors" }
	};
	size_t i;
	char command[256];
	FILE *fp;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 100);
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",FILE");
	fp = fopen(NATIVE_FILE, "wb");
	fputs("10 PRINT\n", fp);
	fclose(fp);
	ASSERT_RUN(0, DGNDOS "copy -t " NATIVE_FILE " " TEST_IMAGE ",PROGRAM");

	/* Real images with names that give nothing away. */
	for (i = 0; i < sizeof(image_formats) / sizeof(image_formats[0]); i++)
	{
		remove(KEEP_FILE);
		sprintf(command, DGNDOS "dskinit -f%s " KEEP_FILE, image_formats[i][0]);
		ASSERT_RUN(0, command);
		check_refused(KEEP_FILE, image_formats[i][1]);
	}
	remove(KEEP_FILE);
	ASSERT_RUN(0, DGNDOS "dskinit -s2 -fjvc " KEEP_FILE);
	check_refused(KEEP_FILE, "JVC header");
	remove(KEEP_FILE);

	/* One track of anything, whole sectors long. */
	write_native(KEEP_FILE, 18 * 256);
	check_refused(KEEP_FILE, "whole number of sectors");
	remove(KEEP_FILE);

	/* The name alone, whatever the case, even of an empty file. */
	fp = fopen(KEEP_NAMED, "wb");
	fclose(fp);
	check_refused(KEEP_NAMED, "by its name");

	/* Into a directory, under the source's own name. */
	run_status = mkdir(NATIVE_DIR, 0755);
	ASSERT_EQUALS(0, run_status);
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",FILE " TEST_IMAGE ",DISK.VDK");
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",DISK.VDK " NATIVE_DIR "/DISK.VDK");
	ASSERT_RUN(1, DGNDOS "copy " TEST_IMAGE ",DISK.VDK " NATIVE_DIR QUIET);

	/* Of several, the others still copy, and the failure is still reported. */
	ASSERT_RUN(1, DGNDOS "copy " TEST_IMAGE ",DISK.VDK " TEST_IMAGE ",FILE " NATIVE_DIR QUIET);
	ASSERT_EQUALS(100, file_size(NATIVE_DIR "/FILE", back, sizeof(back)));
	remove(NATIVE_DIR "/FILE");
	remove(NATIVE_DIR "/DISK.VDK");

	remove(KEEP_NAMED);
	clean_up();
}

/* Everything else on the host can still be replaced. */
static void test_copy_replaces_other_host_files(void)
{
	static const u_int sizes[] = { 0, 9, 256, 512, 18 * 256 - 256, 18 * 256 + 1, 5000 };
	size_t i;
	long size;

	clean_up();
	ASSERT_RUN(0, DGNDOS "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 100);
	ASSERT_RUN(0, DGNDOS "copy " NATIVE_FILE " " TEST_IMAGE ",FILE");

	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		remove(NATIVE_OTHER);
		write_native(NATIVE_OTHER, sizes[i]);
		ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",FILE " NATIVE_OTHER);
		size = file_size(NATIVE_OTHER, back, sizeof(back));
		ASSERT_EQUALS(100, size);
	}

	ASSERT_RUN(0, DGNDOS "copy -r " TEST_IMAGE ",FILE " NATIVE_OTHER);
	ASSERT_RUN(0, DGNDOS "copy -r " NATIVE_FILE " " NATIVE_OTHER);

	/* A name that isn't there yet can look like anything. */
	remove(KEEP_NAMED);
	ASSERT_RUN(0, DGNDOS "copy " TEST_IMAGE ",FILE " KEEP_NAMED);
	remove(KEEP_NAMED);

	clean_up();
}

int main(void)
{
	RUN(test_help);
	RUN(test_dskinit);
	RUN(test_dir_of_mame_disk);
	RUN(test_copy_round_trip);
	RUN(test_copy_to_directories);
	RUN(test_copy_too_large_leaves_nothing);
	RUN(test_list_translates_line_ends);
	RUN(test_fstat_and_dump);
	RUN(test_attr_rename_kill);
	RUN(test_dir_of_damaged_image);
	RUN(test_fstat_file_headers);
	RUN(test_list_tokens);
	RUN(test_copy_tokens);
	RUN(test_copy_refuses_to_overwrite_disk_images);
	RUN(test_copy_replaces_other_host_files);
	return TEST_REPORT();
}

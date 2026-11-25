/* Tests for the cflex command. */

#include <string.h>
#include "testposix.h"
#include <utime.h>
#include <sys/stat.h>

#include "tinytest.h"
#include "flexpath.h"
#include "imagedamage.h"
#include "toolshed.h"
#include "commandrun.h"

#define FLEXCMD "../cflex/cflex "
#define QUIET " > /dev/null 2>&1"
#define TEST_IMAGE "flexcmd.dsk"
#define TEST_DMK "flexcmd.dmk"
#define OUTPUT_FILE "flexcmd_output.txt"
#define NATIVE_FILE "flexcmd_native.bin"
#define NATIVE_OTHER "flexcmd_other.bin"
#define NATIVE_TEXT "flexcmd_text.txt"
#define NATIVE_DIR "flexcmd_dir"
#define CAPTURE " > " OUTPUT_FILE " 2>&1"
#define TEST_DSAVE "flexcmd_dsave.dmk"

/* Dates from the source, in UTC, whatever the machine running the tests has
   set.  SDE uses SOURCE_DATE_EPOCH: 1700000000 is 11/14/23 in UTC, and
   11/15/23 at UTC+14, which it must not depend on. */
#define SOURCE_DATES "env -u SOURCE_DATE_EPOCH TZ=UTC " FLEXCMD
#define SDE "env SOURCE_DATE_EPOCH=1700000000 TZ=XXX-14 " FLEXCMD
#define HOST_TIME 423576000	/* 12:00 on 06/04/83, UTC */

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
	test_text_output(output);
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

/* A file larger than data can hold */
static void write_zeros(const char *pathname, u_int size)
{
	FILE *fp = fopen(pathname, "wb");
	u_int i;

	for (i = 0; i < size; i++)
	{
		fputc(0, fp);
	}
	fclose(fp);
}

static void write_text(const char *pathname, const char *text)
{
	FILE *fp = fopen(pathname, "wb");

	fputs(text, fp);
	fclose(fp);
}

static void clean_up(void)
{
	remove(TEST_IMAGE);
	remove(TEST_DMK);
	remove(OUTPUT_FILE);
	remove(NATIVE_FILE);
	remove(NATIVE_OTHER);
	remove(NATIVE_TEXT);
	remove(NATIVE_DIR "/" NATIVE_FILE);
	rmdir(NATIVE_DIR);
	remove(TEST_DSAVE);
}

/* Make a random file, as ERRORS.SYS is, of a pattern, with a date and
   protection. */
static void make_random_file(const char *pathlist, u_int size)
{
	flex_path_id path;
	flex_file_stat stat;
	u_int i, count = size;

	for (i = 0; i < size; i++)
	{
		data[i] = (u_char)(i * 11 + 5);
	}
	ASSERT_EC(0, _flex_create(&path, pathlist, FAM_WRITE));
	ASSERT_EC(0, _flex_ss_random(path));
	run_status = _flex_write(path, data, &count);
	ASSERT_EQUALS(0, run_status);
	ASSERT_EC(0, _flex_gs_fd(path, &stat));
	stat.attributes = FLEX_ATTR_DELETE;
	stat.month = 10;
	stat.day = 3;
	stat.year = 80;
	ASSERT_EC(0, _flex_ss_fd(path, &stat));
	ASSERT_EC(0, _flex_close(path));
}

static void get_stat(const char *pathlist, flex_file_stat *stat)
{
	flex_path_id path;

	memset(stat, 0, sizeof(*stat));
	ASSERT_EC(0, _flex_open(&path, pathlist, FAM_READ));
	ASSERT_EC(0, _flex_gs_fd(path, stat));
	_flex_close(path);
}

static void set_host_time(const char *pathname)
{
	struct utimbuf times;

	times.actime = HOST_TIME;
	times.modtime = HOST_TIME;
	run_status = utime(pathname, &times);
	ASSERT_EQUALS(0, run_status);
}

#define ASSERT_DATE(month_, day_, year_, stat) \
	{ \
		ASSERT_EQUALS(month_, (stat).month); \
		ASSERT_EQUALS(day_, (stat).day); \
		ASSERT_EQUALS(year_, (stat).year); \
	}

/* A copy from one FLEX disk to another keeps the file as it was: random,
   protected and dated.  Other copies take the date of their source.  With
   SOURCE_DATE_EPOCH set, every copy has that date instead. */
static void test_copy_keeps_random_and_dates(void)
{
	flex_file_stat stat;
	long size;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	ASSERT_RUN(0, FLEXCMD "dskinit -d " TEST_DMK);
	make_random_file(TEST_IMAGE ",ERRORS.SYS", 1764);

	ASSERT_RUN(0, SOURCE_DATES "copy " TEST_IMAGE ",ERRORS.SYS " TEST_DMK ",ERRORS.SYS");
	get_stat(TEST_DMK ",ERRORS.SYS", &stat);
	ASSERT_EQUALS(1, stat.random);
	ASSERT_EQUALS(9, stat.sectors);
	ASSERT_EQUALS(1764, stat.file_size);
	ASSERT_EQUALS(FLEX_ATTR_DELETE, stat.attributes);
	ASSERT_DATE(10, 3, 80, stat);
	ASSERT_RUN(0, FLEXCMD "copy " TEST_DMK ",ERRORS.SYS " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(1764, size);
	ASSERT_MEM_EQUALS(data, back, 1764);
	ASSERT_RUN(0, FLEXCMD "dir " TEST_DMK CAPTURE);
	read_output();
	ASSERT("the copy is listed as random", strstr(output, "\nERRORS   SYS      9  .D.. R  10/03/80") != NULL);

	/* A copy over a file there is still made random, and never left half made. */
	ASSERT_RUN(EOS_FAE, SOURCE_DATES "copy " TEST_IMAGE ",ERRORS.SYS " TEST_DMK ",ERRORS.SYS" QUIET);
	ASSERT_RUN(EOS_FNA, SOURCE_DATES "copy -r " TEST_IMAGE ",ERRORS.SYS " TEST_DMK ",ERRORS.SYS" QUIET);
	ASSERT_RUN(0, FLEXCMD "attr -x " TEST_DMK ",ERRORS.SYS");
	ASSERT_RUN(0, SDE "copy -r " TEST_IMAGE ",ERRORS.SYS " TEST_DMK ",ERRORS.SYS");
	get_stat(TEST_DMK ",ERRORS.SYS", &stat);
	ASSERT_EQUALS(1, stat.random);
	ASSERT_DATE(11, 14, 23, stat);

	/* dsave copies a disk with copy, so keeps them too. */
	ASSERT_RUN(0, FLEXCMD "dskinit -d " TEST_DSAVE);
	ASSERT_RUN(0, SOURCE_DATES "dsave -e -x../cflex/cflex " TEST_IMAGE ", " TEST_DSAVE "," QUIET);
	get_stat(TEST_DSAVE ",ERRORS.SYS", &stat);
	ASSERT_EQUALS(1, stat.random);
	ASSERT_DATE(10, 3, 80, stat);

	/* A host file's date is its modification time, -l or not. */
	write_native(NATIVE_FILE, 300);
	set_host_time(NATIVE_FILE);
	ASSERT_RUN(0, SOURCE_DATES "copy " NATIVE_FILE " " TEST_DMK ",HOST.BIN");
	get_stat(TEST_DMK ",HOST.BIN", &stat);
	ASSERT_EQUALS(0, stat.random);
	ASSERT_DATE(6, 4, 83, stat);
	ASSERT_RUN(0, SOURCE_DATES "copy -l " NATIVE_FILE " " TEST_DMK ",HOST.TXT");
	get_stat(TEST_DMK ",HOST.TXT", &stat);
	ASSERT_DATE(6, 4, 83, stat);
	ASSERT_RUN(0, SDE "copy -r " NATIVE_FILE " " TEST_DMK ",HOST.BIN");
	get_stat(TEST_DMK ",HOST.BIN", &stat);
	ASSERT_DATE(11, 14, 23, stat);
	ASSERT_RUN(0, SDE "copy -l -r " NATIVE_FILE " " TEST_DMK ",HOST.TXT");
	get_stat(TEST_DMK ",HOST.TXT", &stat);
	ASSERT_DATE(11, 14, 23, stat);

	/* A disk made with SOURCE_DATE_EPOCH is dated in UTC. */
	remove(TEST_IMAGE);
	ASSERT_RUN(0, SDE "dskinit " TEST_IMAGE);
	ASSERT_RUN(0, FLEXCMD "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the volume is dated in UTC", strstr(output, "Created: 11/14/23") != NULL);
	clean_up();
}

static void test_help(void)
{
	ASSERT_RUN(0, FLEXCMD CAPTURE);
	read_output();
	ASSERT("the commands are listed", strstr(output, "dskinit") != NULL && strstr(output, "rename") != NULL);

	ASSERT_RUN(0, FLEXCMD "copy" CAPTURE);
	read_output();
	ASSERT("a command with no arguments gives help", strstr(output, "Syntax: copy") != NULL);

	ASSERT_RUN(1, FLEXCMD "nosuchcommand" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("cflex: unknown command 'nosuchcommand'\n", output);
	ASSERT_RUN(1, FLEXCMD "dir -z x" QUIET);
	remove(OUTPUT_FILE);
}

static void test_dskinit(void)
{
	static u_char image[1024 * 1024];

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	ASSERT_EQUALS(35 * 10 * 256, file_size(TEST_IMAGE, image, sizeof(image)));
	ASSERT_RUN(EOS_FAE, FLEXCMD "dskinit " TEST_IMAGE QUIET);
	ASSERT_RUN(0, FLEXCMD "free " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("Tracks: 35, sectors per track: 10\nFree sectors: 340 (85680 bytes)\n", output);
	remove(TEST_IMAGE);

	ASSERT_RUN(0, FLEXCMD "dskinit -t40 -s2 -d -p -nDATA.VOL -v300 " TEST_IMAGE);
	ASSERT_EQUALS(40 * 36 * 256, file_size(TEST_IMAGE, image, sizeof(image)));
	ASSERT_RUN(0, FLEXCMD "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the volume is named", strstr(output, "Volume: DATA.VOL  Number: 300  Created: ") != NULL);
	ASSERT("the disk is empty", strstr(output, "\n0 files, 0 sectors used, 1404 sectors free (353808 bytes)\n") != NULL);
	remove(TEST_IMAGE);

	/* The format comes from the extension, unless given. */
	ASSERT_RUN(0, FLEXCMD "dskinit -d " TEST_DMK);
	ASSERT_RUN(0, FLEXCMD "info " TEST_DMK CAPTURE);
	read_output();
	ASSERT("the image is DMK", strstr(output, "is a DMK disk image") != NULL);
	ASSERT("track 0 is short", strstr(output, "Track 0 is 10 sectors (1-10)") != NULL);
	ASSERT("FLEX is found", strstr(output, "FLEX") != NULL);
	remove(TEST_DMK);
	ASSERT_RUN(0, FLEXCMD "dskinit -fraw " TEST_DMK);
	ASSERT_EQUALS(35 * 10 * 256, file_size(TEST_DMK, image, sizeof(image)));
	remove(TEST_DMK);

	ASSERT_RUN(1, FLEXCMD "dskinit -t1 " TEST_IMAGE QUIET);
	ASSERT_RUN(1, FLEXCMD "dskinit -s3 " TEST_IMAGE QUIET);
	ASSERT_RUN(1, FLEXCMD "dskinit -v70000 " TEST_IMAGE QUIET);
	ASSERT_RUN(1, FLEXCMD "dskinit -fnope " TEST_IMAGE QUIET);
	ASSERT_RUN(1, FLEXCMD "dskinit -n9BAD " TEST_IMAGE QUIET);
	ASSERT_RUN(1, FLEXCMD "dskinit -fvdk " TEST_IMAGE QUIET);
	ASSERT_EQUALS(-1, file_size(TEST_IMAGE, image, sizeof(image)));
	clean_up();
}

static void test_copy_round_trip(void)
{
	long size;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit -d -t40 " TEST_DMK);
	write_native(NATIVE_FILE, 5000);

	ASSERT_RUN(EOS_PNNF, FLEXCMD "copy flexcmd_missing " TEST_DMK ",X" QUIET);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_DMK ",FILE.BIN");
	ASSERT_RUN(EOS_FAE, FLEXCMD "copy " NATIVE_FILE " " TEST_DMK ",FILE.BIN" QUIET);
	ASSERT_RUN(0, FLEXCMD "copy -r " NATIVE_FILE " " TEST_DMK ",FILE.BIN");
	ASSERT_RUN(EOS_BPNAM, FLEXCMD "copy " NATIVE_FILE " " TEST_DMK ",B@D" QUIET);

	/* A file is whole sectors, so it comes back padded with nuls. */
	ASSERT_RUN(0, FLEXCMD "copy " TEST_DMK ",file.bin " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(20 * 252, size);
	ASSERT_MEM_EQUALS(data, back, 5000);

	/* Within a disk too. */
	remove(NATIVE_OTHER);
	ASSERT_RUN(0, FLEXCMD "copy " TEST_DMK ",FILE.BIN " TEST_DMK ",SECOND.BIN");
	ASSERT_RUN(0, FLEXCMD "copy " TEST_DMK ",SECOND.BIN " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(20 * 252, size);
	ASSERT_MEM_EQUALS(data, back, 5000);

	/* A host file that looks like a disk image is never replaced. */
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	ASSERT_RUN(1, FLEXCMD "copy -r " TEST_DMK ",FILE.BIN " TEST_IMAGE QUIET);
	{
		static u_char image[1024 * 1024];

		ASSERT_EQUALS(35 * 10 * 256, file_size(TEST_IMAGE, image, sizeof(image)));
	}
	clean_up();
}

static void test_copy_to_directories(void)
{
	long size;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 300);
	write_native(NATIVE_OTHER, 300);

	/* These names are too long for FLEX, so they're cut short. */
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",");
	ASSERT_RUN(0, FLEXCMD "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the name was cut short", strstr(output, "\nFLEXCMD_ BIN      2  ....  ") != NULL);

	ASSERT_RUN(1, FLEXCMD "copy " NATIVE_FILE " " NATIVE_OTHER " " TEST_IMAGE ",SINGLE" QUIET);

	run_status = mkdir(NATIVE_DIR, 0755);
	ASSERT_EQUALS(0, run_status);
	ASSERT_RUN(0, FLEXCMD "copy " TEST_IMAGE ",FLEXCMD_.BIN " NATIVE_DIR);
	size = file_size(NATIVE_DIR "/FLEXCMD_.BIN", back, sizeof(back));
	ASSERT_EQUALS(2 * 252, size);
	remove(NATIVE_DIR "/FLEXCMD_.BIN");
	clean_up();
}

static void test_text(void)
{
	long size;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	write_text(NATIVE_TEXT, "LINE ONE\n\tINDENTED\nLAST\n");

	/* Text goes in with CR line ends and a tab as compressed spaces. */
	ASSERT_RUN(0, FLEXCMD "copy -l " NATIVE_TEXT " " TEST_IMAGE ",TEXT.TXT");
	ASSERT_RUN(0, FLEXCMD "copy " TEST_IMAGE ",TEXT.TXT " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(252, size);
	ASSERT_MEM_EQUALS("LINE ONE\r\t\010INDENTED\rLAST\r\0", back, 25);

	/* It comes out as it went in. */
	ASSERT_RUN(0, FLEXCMD "copy -l -r " TEST_IMAGE ",TEXT.TXT " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(31, size);
	ASSERT_MEM_EQUALS("LINE ONE\n        INDENTED\nLAST\n", back, 31);

	ASSERT_RUN(0, FLEXCMD "list " TEST_IMAGE ",TEXT.TXT" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS("LINE ONE\n        INDENTED\nLAST\n", output);
	ASSERT_RUN(EOS_PNNF, FLEXCMD "list " TEST_IMAGE ",NONE.TXT" QUIET);
	clean_up();
}

static void test_dir_fstat_attr(void)
{
	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit -nTESTS " TEST_IMAGE);
	write_native(NATIVE_FILE, 600);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",DATA.BIN");

	ASSERT_RUN(0, FLEXCMD "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the heading names the disk", strstr(output, "Directory of: " TEST_IMAGE ",\n\nVolume: TESTS  Number: 1") != NULL);
	ASSERT("the file is listed", strstr(output, "\nDATA     BIN      3  ....    ") != NULL);
	ASSERT("the total is given", strstr(output, "\n1 file, 3 sectors used, 337 sectors free (84924 bytes)\n") != NULL);

	ASSERT_RUN(0, FLEXCMD "attr " TEST_IMAGE ",DATA.BIN" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS(TEST_IMAGE ",DATA.BIN: ....\n", output);
	ASSERT_RUN(0, FLEXCMD "attr -w -d " TEST_IMAGE ",DATA.BIN");
	ASSERT_RUN(0, FLEXCMD "attr " TEST_IMAGE ",DATA.BIN" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS(TEST_IMAGE ",DATA.BIN: WD..\n", output);
	ASSERT_RUN(EOS_FNA, FLEXCMD "kill " TEST_IMAGE ",DATA.BIN" QUIET);
	ASSERT_RUN(EOS_FNA, FLEXCMD "copy -r " NATIVE_FILE " " TEST_IMAGE ",DATA.BIN" QUIET);
	ASSERT_RUN(0, FLEXCMD "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the protection is listed", strstr(output, "\nDATA     BIN      3  WD..    ") != NULL);
	ASSERT_RUN(0, FLEXCMD "attr -x -c " TEST_IMAGE ",DATA.BIN");
	ASSERT_RUN(0, FLEXCMD "attr " TEST_IMAGE ",DATA.BIN" CAPTURE);
	read_output();
	ASSERT_STRING_EQUALS(TEST_IMAGE ",DATA.BIN: ...C\n", output);

	ASSERT_RUN(0, FLEXCMD "fstat " TEST_IMAGE ",DATA.BIN" CAPTURE);
	read_output();
	ASSERT("the name is given", strstr(output, "  Filename           : DATA.BIN\n") != NULL);
	ASSERT("the type is given", strstr(output, "  File type          : Sequential\n") != NULL);
	ASSERT("the sectors are given", strstr(output, "  Sectors            : 3\n") != NULL);
	ASSERT("the size is given", strstr(output, "  Data size          : 756 bytes\n") != NULL);
	ASSERT("the first sector is given", strstr(output, "  First sector       : track 1, sector 1\n") != NULL);
	ASSERT("the last sector is given", strstr(output, "  Last sector        : track 1, sector 3\n") != NULL);
	ASSERT("it's not binary", strstr(output, "Binary") == NULL);
	clean_up();
}

static void test_fstat_binary(void)
{
	static const u_char binary[] = {
		0x02, 0xC1, 0x00, 0x02, 0x7E, 0xCD,
		0x16, 0xC1, 0x00
	};
	FILE *fp;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	fp = fopen(NATIVE_FILE, "wb");
	fwrite(binary, 1, sizeof(binary), fp);
	fclose(fp);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",PROG.CMD");
	ASSERT_RUN(0, FLEXCMD "fstat " TEST_IMAGE ",PROG.CMD" CAPTURE);
	read_output();
	ASSERT("the records are counted", strstr(output, "  Binary records     : 2\n") != NULL);
	ASSERT("the load addresses are given", strstr(output, "  Load addresses     : $C100 to $C101\n") != NULL);
	ASSERT("the transfer address is given", strstr(output, "  Transfer address   : $C100\n") != NULL);
	clean_up();
}

static void test_kill_and_rename(void)
{
	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 300);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",ONE.TXT");
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",TWO.TXT");
	ASSERT_RUN(0, FLEXCMD "rename " TEST_IMAGE ",ONE.TXT FIRST");
	ASSERT_RUN(EOS_FAE, FLEXCMD "rename " TEST_IMAGE ",FIRST.TXT TWO" QUIET);
	ASSERT_RUN(0, FLEXCMD "kill " TEST_IMAGE ",TWO.TXT");
	ASSERT_RUN(EOS_PNNF, FLEXCMD "kill " TEST_IMAGE ",TWO.TXT" QUIET);
	ASSERT_RUN(0, FLEXCMD "dir " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the renamed file is listed", strstr(output, "\nFIRST    TXT      2  ") != NULL);
	ASSERT("the killed file is not", strstr(output, "TWO") == NULL);
	ASSERT("its sectors are free", strstr(output, "\n1 file, 2 sectors used, 338 sectors free") != NULL);
	clean_up();
}

static void test_free_reports_damage(void)
{
	FILE *fp;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	/* Say there are more free sectors than the chain has. */
	fp = fopen(TEST_IMAGE, "r+b");
	fseek(fp, 2 * 256 + 0x22, SEEK_SET);
	fputc(0x60, fp);
	fclose(fp);
	ASSERT_RUN(0, FLEXCMD "free " TEST_IMAGE CAPTURE);
	read_output();
	ASSERT("the difference is reported", strstr(output, "The free chain has 340 sectors, not 352.\n") != NULL);
	clean_up();
}

static void test_dsave(void)
{
	long size;

	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	ASSERT_RUN(0, FLEXCMD "dskinit -fdmk " TEST_DMK);
	write_native(NATIVE_FILE, 1000);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",A.BIN");
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",B.BIN");
	ASSERT_RUN(0, FLEXCMD "dsave -e -x../cflex/cflex " TEST_IMAGE ", " TEST_DMK "," QUIET);
	ASSERT_RUN(0, FLEXCMD "copy " TEST_DMK ",B.BIN " NATIVE_OTHER);
	size = file_size(NATIVE_OTHER, back, sizeof(back));
	ASSERT_EQUALS(4 * 252, size);
	ASSERT_MEM_EQUALS(data, back, 1000);
	clean_up();
}

/* Other tools reach FLEX disks through libcoco. */
static void test_other_tools(void)
{
	clean_up();
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_IMAGE);
	write_native(NATIVE_FILE, 100);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_IMAGE ",DUMPME.BIN");
	ASSERT_RUN(0, FLEXCMD "dump " TEST_IMAGE ",DUMPME.BIN" CAPTURE);
	read_output();
	ASSERT("the dump shows the data", strstr(output, "00000000  000d 1a27 3441") != NULL);
	ASSERT_RUN(0, "../os9/os9 copy " TEST_IMAGE ",DUMPME.BIN " NATIVE_OTHER);
	ASSERT_EQUALS(252, file_size(NATIVE_OTHER, back, sizeof(back)));
	ASSERT_MEM_EQUALS(data, back, 100);
	clean_up();
}


static void test_gen(void)
{
	clean_up();
	write_native(NATIVE_FILE, 3000);

	/* Only a double density disk can boot. */
	ASSERT_RUN(0, FLEXCMD "dskinit " TEST_DMK);
	ASSERT_RUN(1, FLEXCMD "gen " TEST_DMK " " NATIVE_FILE CAPTURE);
	read_output();
	ASSERT("single density is refused", strstr(output, "must be double density") != NULL);
	remove(TEST_DMK);

	ASSERT_RUN(0, FLEXCMD "dskinit -d " TEST_DMK);
	ASSERT_RUN(0, FLEXCMD "gen " TEST_DMK " " NATIVE_FILE);
	ASSERT_RUN(0, FLEXCMD "dir " TEST_DMK CAPTURE);
	read_output();
	ASSERT("FLEX.SYS is copied", strstr(output, "\nFLEX     SYS     12  ") != NULL);
	ASSERT("the boot sectors are taken", strstr(output, "12 sectors used, 589 sectors free") != NULL);
	ASSERT("Disk BASIC is seen", strstr(output, "Other possible filesystems: Disk BASIC") != NULL);

	ASSERT_RUN(0, "../decb/decb dir " TEST_DMK CAPTURE);
	read_output();
	ASSERT("Disk BASIC lists FLEX.BAS", strstr(output, "FLEX     BAS  0  A") != NULL);

	/* Again, linking another file already there */
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_FILE " " TEST_DMK ",OTHER.SYS");
	ASSERT_RUN(0, FLEXCMD "gen -nOTHER.SYS " TEST_DMK);
	ASSERT_RUN(EOS_PNNF, FLEXCMD "gen -nNONE.SYS " TEST_DMK QUIET);
	ASSERT_RUN(1, FLEXCMD "gen " TEST_DMK " " NATIVE_FILE " extra" QUIET);
	ASSERT_RUN(0, FLEXCMD "free " TEST_DMK CAPTURE);
	read_output();
	ASSERT("the free chain agrees", strstr(output, "The free chain") == NULL);

	/* A disk whose boot sectors hold a file */
	remove(TEST_DMK);
	ASSERT_RUN(0, FLEXCMD "dskinit -d -t19 " TEST_DMK);
	write_zeros(NATIVE_OTHER, 252 * 18 * 18);
	ASSERT_RUN(0, FLEXCMD "copy " NATIVE_OTHER " " TEST_DMK ",BIG.BIN");
	ASSERT_RUN(1, FLEXCMD "gen " TEST_DMK " " NATIVE_FILE CAPTURE);
	read_output();
	ASSERT("the sectors in use are named", strstr(output, "track 16 sectors 1 to 9") != NULL);
	clean_up();
}

int main(void)
{
	RUN(test_help);
	RUN(test_dskinit);
	RUN(test_copy_round_trip);
	RUN(test_copy_to_directories);
	RUN(test_text);
	RUN(test_dir_fstat_attr);
	RUN(test_fstat_binary);
	RUN(test_kill_and_rename);
	RUN(test_free_reports_damage);
	RUN(test_dsave);
	RUN(test_copy_keeps_random_and_dates);
	RUN(test_gen);
	RUN(test_other_tools);
	return TEST_REPORT();
}

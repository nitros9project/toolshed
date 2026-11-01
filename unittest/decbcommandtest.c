/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>
#include <sys/wait.h>

void test_decb_command_dskini()
{
	error_code ec;
	native_path_id nativepath;
	u_int size;

	ec = system("../decb/decb dskini test.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	
	ec = _native_open(&nativepath, "test.dsk", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &size);	
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(35*18*256, size);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);
}

void test_decb_command_copy()
{
	error_code ec;
	native_path_id nativepath;
	coco_path_id cocopath;
	u_int source_size, dest_size;

 	ec = system("../decb/decb copy does_not_exist test.dsk,DNE.TXT > /dev/null 2>&1");
 	ASSERT_EQUALS(1, WIFEXITED(ec));
 	ec = WEXITSTATUS(ec);
	ASSERT_EQUALS(EOS_PNNF, ec);

	/* Test a simple copy */
	
	ec = system("../decb/decb copy Makefile test.dsk,MAKEF.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	ec = _native_open(&nativepath, "Makefile", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &source_size);	
	ASSERT_EQUALS(0, ec);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);

	ec = _coco_open(&cocopath, "test.dsk,MAKEF.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _coco_gs_size(cocopath, &dest_size);	
	ASSERT_EQUALS(0, ec);

	ec = _coco_close(cocopath);
	ASSERT_EQUALS(0, ec);

	ASSERT_EQUALS(dest_size, source_size);

	/* Test a copy of an empty file */
	
	remove("empty_file");
	ec = system("touch empty_file > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	ec = system("../decb/decb copy empty_file test.dsk,EMPTY.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	ec = _native_open(&nativepath, "empty_file", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &source_size);	
	ASSERT_EQUALS(0, ec);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);

	ec = _coco_open(&cocopath, "test.dsk,EMPTY.TXT", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _coco_gs_size(cocopath, &dest_size);	
	ASSERT_EQUALS(0, ec);

	ec = _coco_close(cocopath);
	ASSERT_EQUALS(0, ec);

	ASSERT_EQUALS(dest_size, source_size);
}

void test_decb_command_copy_to_native_directory()
{
	error_code ec;
	FILE *file;
	const unsigned char contents[] = {0x00, 0x01, 0x0a, 0x0d, 0x80, 0xff};
	unsigned char result[sizeof(contents) + 1];
	const char *targets[] = {"decb_copy_dir", "decb_copy_dir/"};
	char command[256];
	size_t size;
	int i;

	ec = system("../decb/decb dskini copy_dir.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	file = fopen("copy_source", "wb");
	ASSERT("create source file", file != NULL);
	size = fwrite(contents, 1, sizeof(contents), file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents), size);
	ASSERT_EQUALS(0, ec);
	ec = system("../decb/decb copy copy_source copy_dir.dsk,SOURCE.BIN > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = system("mkdir -p decb_copy_dir");
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		remove("decb_copy_dir/SOURCE.BIN");
		snprintf(command, sizeof(command),
			 "../decb/decb copy copy_dir.dsk,SOURCE.BIN %s > /dev/null 2>&1",
			 targets[i]);
		ec = system(command);
		ASSERT_EQUALS(0, ec);
		file = fopen("decb_copy_dir/SOURCE.BIN", "rb");
		ASSERT("open extracted file", file != NULL);
		size = fread(result, 1, sizeof(result), file);
		ec = fclose(file);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(sizeof(contents), size);
		ASSERT_MEM_EQUALS(contents, result, sizeof(contents));
	}

	remove("decb_copy_dir/SOURCE.BIN");
	remove("decb_copy_dir");
	remove("copy_source");
	remove("copy_dir.dsk");
}

void test_decb_command_copy_eol_same_type()
{
	error_code ec;
	FILE *file;
	coco_path_id path;
	const char contents[] = "FIRST\rSECOND\nTHIRD\r\n";
	char result[sizeof(contents)];
	const char *commands[] = {
		"../decb/decb copy -l copy_eol.dsk,SOURCE.TXT copy_eol.dsk,DEST.TXT > /dev/null 2>&1",
		"../decb/decb copy -l copy_eol_source copy_eol_dest > /dev/null 2>&1"
	};
	char *destinations[] = {"copy_eol.dsk,DEST.TXT", "copy_eol_dest"};
	u_int size;
	size_t written;
	int i;

	ec = system("../decb/decb dskini copy_eol.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	remove("copy_eol_dest");
	file = fopen("copy_eol_source", "wb");
	ASSERT("create EOL source file", file != NULL);
	written = fwrite(contents, 1, sizeof(contents) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents) - 1, written);
	ASSERT_EQUALS(0, ec);
	ec = system("../decb/decb copy copy_eol_source copy_eol.dsk,SOURCE.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(0, ec);
		ec = _coco_open(&path, destinations[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_size(path, &size);
		ASSERT_EQUALS(0, ec);
		if (size != sizeof(contents) - 1)
		{
			_coco_close(path);
			ASSERT_EQUALS(sizeof(contents) - 1, size);
		}
		size = sizeof(result);
		ec = _coco_read(path, result, &size);
		_coco_close(path);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(sizeof(contents) - 1, size);
		ASSERT_MEM_EQUALS(contents, result, size);
	}

	remove("copy_eol_source");
	remove("copy_eol_dest");
	remove("copy_eol.dsk");
}

void test_decb_command_copy_empty_metadata()
{
	error_code ec;
	FILE *file;
	coco_path_id path;
	coco_file_stat statbuf;
	u_int size;
	const char *commands[] = {
		"../decb/decb copy -3a copy_empty_source copy_empty.dsk,EMPTY.TXT > /dev/null 2>&1",
		"../decb/decb copy copy_empty.dsk,EMPTY.TXT copy_empty.dsk,COPY.TXT > /dev/null 2>&1"
	};
	char *destinations[] = {"copy_empty.dsk,EMPTY.TXT", "copy_empty.dsk,COPY.TXT"};
	int i;

	ec = system("../decb/decb dskini copy_empty.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	file = fopen("copy_empty_source", "wb");
	ASSERT("create empty source file", file != NULL);
	ec = fclose(file);
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(0, ec);
		ec = _coco_open(&path, destinations[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_fd(path, &statbuf);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_size(path, &size);
		_coco_close(path);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(0, size);
		ASSERT_EQUALS(3, statbuf.file_type);
		ASSERT_EQUALS(255, statbuf.data_type);
	}

	remove("copy_empty_source");
	remove("copy_empty.dsk");
}

void test_decb_command_copy_token_metadata()
{
	error_code ec, close_ec;
	FILE *file;
	coco_path_id path;
	coco_file_stat statbuf;
	const char program[] = "10 PRINT \"TEST\"\n20 GOTO 10\n";
	char result[sizeof(program)];
	const char *commands[] = {
		"../decb/decb copy -t copy_token_source copy_token.dsk,TOKENS.BAS > /dev/null 2>&1",
		"../decb/decb copy -t copy_token.dsk,TOKENS.BAS copy_token.dsk,TEXT.BAS > /dev/null 2>&1"
	};
	char *destinations[] = {"copy_token.dsk,TOKENS.BAS", "copy_token.dsk,TEXT.BAS"};
	u_int size;
	size_t written;
	int i;

	ec = system("../decb/decb dskini copy_token.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	file = fopen("copy_token_source", "wb");
	ASSERT("create BASIC source", file != NULL);
	written = fwrite(program, 1, sizeof(program) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(program) - 1, written);
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 2; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(0, ec);
		ec = _coco_open(&path, destinations[i], FAM_READ);
		ASSERT_EQUALS(0, ec);
		ec = _coco_gs_fd(path, &statbuf);
		close_ec = _coco_close(path);
		ASSERT_EQUALS(0, ec);
		ASSERT_EQUALS(0, close_ec);
		ASSERT_EQUALS(0, statbuf.file_type);
		ASSERT_EQUALS(i == 0 ? 0 : 255, statbuf.data_type);
	}

	ec = _coco_open(&path, destinations[1], FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(result);
	ec = _coco_read(path, result, &size);
	close_ec = _coco_close(path);
	remove("copy_token_source");
	remove("copy_token.dsk");
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(0, close_ec);
	ASSERT_EQUALS(sizeof(program) - 1, size);
	ASSERT_MEM_EQUALS(program, result, size);
}

void test_decb_command_attr_no_comma_spec()
{
	error_code ec;
	int i;
	const char *const commands[] = {
		"../decb/decb attr no_comma.dsk > /dev/null 2>&1",
		"../decb/decb attr -a no_comma.dsk > /dev/null 2>&1",
		"../decb/decb attr no_comma.dsk no_comma.dsk,KEEP.TXT > /dev/null 2>&1",
		"../decb/decb attr no_comma.dsk,KEEP.TXT no_comma.dsk > /dev/null 2>&1",
		"../decb/decb attr no_comma.dsk,NOPE.TXT no_comma.dsk > /dev/null 2>&1",
		"../decb/decb attr no_comma.dsk,KEEP.TXT > /dev/null 2>&1",
	};
	const int expected[] = {EOS_BPNAM, EOS_BPNAM, EOS_BPNAM, EOS_BPNAM,
		EOS_PNNF, 0};

	remove("no_comma.dsk");
	remove("no_comma.before");

	ec = system("../decb/decb dskini no_comma.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = system("../decb/decb copy Makefile no_comma.dsk,KEEP.TXT > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	ec = system("cp no_comma.dsk no_comma.before");
	ASSERT_EQUALS(0, ec);

	/* A failed spec must be reported even when another spec succeeds. */
	for (i = 0; i < 6; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(1, WIFEXITED(ec));
		ec = WEXITSTATUS(ec);
		ASSERT_EQUALS(expected[i], ec);
		ec = system("cmp -s no_comma.dsk no_comma.before");
		ASSERT_EQUALS(0, ec);
	}

	remove("no_comma.before");
	remove("no_comma.dsk");
}

/* A raw path is the whole disk, by either copy routine. */
void test_decb_command_copy_raw_path()
{
	static u_char image[161280], copy[161280];
	char buffer[4096];
	FILE *fp;
	error_code ec;
	size_t size;

	remove("test_raw.dsk");
	remove("test_raw.bin");

	ec = system("../decb/decb dskini test_raw.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);

	fp = fopen("test_raw.dsk", "rb");
	size = fread(image, 1, sizeof(image), fp);
	fclose(fp);
	ASSERT_EQUALS(sizeof(image), size);

	ec = system("../decb/decb copy test_raw.dsk, test_raw.bin > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	fp = fopen("test_raw.bin", "rb");
	size = fread(copy, 1, sizeof(copy), fp);
	fclose(fp);
	ASSERT_EQUALS(sizeof(copy), size);
	ASSERT_EQUALS(0, memcmp(image, copy, sizeof(image)));
	remove("test_raw.bin");

	ec = TSCopyFile("test_raw.dsk,", "test_raw.bin", 0, 1, 0, 0, buffer, sizeof(buffer));
	ASSERT_EQUALS(0, ec);
	fp = fopen("test_raw.bin", "rb");
	size = fread(copy, 1, sizeof(copy), fp);
	fclose(fp);
	ASSERT_EQUALS(sizeof(copy), size);
	ASSERT_EQUALS(0, memcmp(image, copy, sizeof(image)));

	remove("test_raw.dsk");
	remove("test_raw.bin");
}

/* A copy never replaces a host file that looks like a disk image, even with -r. */
void test_decb_command_copy_refuses_disk_images()
{
	static const char *const commands[] = {
		"../decb/decb copy test_guard_src.txt %s > /dev/null 2> test_guard_err.txt",
		"../decb/decb copy -r test_guard_src.txt %s > /dev/null 2> test_guard_err.txt",
	};
	static const char *const targets[] = { "test_guard.dsk", "test_guard.bin" };
	static char before[4608], after[4608], message[512];
	FILE *fp;
	size_t i, got;
	int t, status;

	fp = fopen("test_guard_src.txt", "wb");
	fputs("replacement\n", fp);
	fclose(fp);

	for (t = 0; t < 2; t++)
	{
		char command[256];

		/* One looks like an image by name, the other by its size. */
		memset(before, 0x5A, sizeof(before));
		fp = fopen(targets[t], "wb");
		fwrite(before, 1, sizeof(before), fp);
		fclose(fp);

		for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
		{
			snprintf(command, sizeof(command), commands[i], targets[t]);
			status = system(command);
			ASSERT_NEQUALS(0, status);

			fp = fopen("test_guard_err.txt", "rb");
			got = fread(message, 1, sizeof(message) - 1, fp);
			fclose(fp);
			message[got] = 0;
			ASSERT("it says why", strstr(message, "looks like a disk image") != NULL);

			fp = fopen(targets[t], "rb");
			got = fread(after, 1, sizeof(after), fp);
			fclose(fp);
			ASSERT_EQUALS(sizeof(before), got);
			ASSERT_EQUALS(0, memcmp(before, after, sizeof(before)));
		}
		remove(targets[t]);
	}

	/* Other files are still written. */
	fp = fopen("test_guard.txt", "wb");
	fputs("old\n", fp);
	fclose(fp);
	status = system("../decb/decb copy -r test_guard_src.txt test_guard.txt > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_guard.txt", "rb");
	got = fread(after, 1, sizeof(after), fp);
	fclose(fp);
	ASSERT_EQUALS(12, got);

	remove("test_guard.txt");
	remove("test_guard_src.txt");
	remove("test_guard_err.txt");
}

/* -x names the executable, in the commands shown and those run. */
void test_decb_command_dsave_executable()
{
	static char output[4096];
	FILE *fp;
	size_t got;
	int status;

	status = system("rm -rf test_x_src test_x_dst && mkdir test_x_src test_x_dst"
			" && echo one > test_x_src/one.txt");
	ASSERT_EQUALS(0, status);

	/* Shown only, under its usual name by default */
	status = system("../decb/decb dsave test_x_src test_x_dst > test_x_out.txt 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_x_out.txt", "rb");
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	output[got] = 0;
	ASSERT("it shows the usual name", strncmp(output, "decb copy 'test_x_src/one.txt' 'test_x_dst/one.txt'", 44) == 0);
	fp = fopen("test_x_dst/one.txt", "rb");
	ASSERT("nothing was copied", fp == NULL);

	/* Shown only, under the given name */
	status = system("../decb/decb dsave -x=no/such/decb test_x_src test_x_dst > test_x_out.txt 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_x_out.txt", "rb");
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	output[got] = 0;
	ASSERT("it shows the given name", strncmp(output, "'no/such/decb' copy ", 18) == 0);

	/* An empty search path shows it is not the shell finding it. */
	status = system("PATH=/nonexistent:/bin:/usr/bin ../decb/decb dsave -e -x=../decb/decb test_x_src test_x_dst > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	fp = fopen("test_x_dst/one.txt", "rb");
	ASSERT("the file was copied", fp != NULL);
	got = fread(output, 1, sizeof(output) - 1, fp);
	fclose(fp);
	ASSERT_EQUALS(4, got);

	/* One that isn't there fails, and says so in the exit status */
	status = system("../decb/decb dsave -e -r -x=no/such/decb test_x_src test_x_dst > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);
	ASSERT_NEQUALS(0, WEXITSTATUS(status));

	/* It needs a path */
	status = system("../decb/decb dsave -x test_x_src test_x_dst > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);

	status = system("rm -rf test_x_src test_x_dst test_x_out.txt");
	ASSERT_EQUALS(0, status);
}

/* hdbconv only takes a raw image, and leaves what looks like an image alone. */
void test_decb_command_hdbconv()
{
	static u_char before[4608], after[4608];
	FILE *fp;
	size_t got;
	int status;

	remove("test_hdb.dmk");
	remove("test_hdb.dsk");
	remove("test_hdb.bin");
	remove("test_hdb_keep.dsk");

	status = system("../decb/decb dskini test_hdb.dmk > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	status = system("../decb/decb hdbconv test_hdb.dmk test_hdb.bin > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);
	fp = fopen("test_hdb.bin", "rb");
	ASSERT("nothing was made", fp == NULL);

	/* A raw image converts, each sector taking twice the room. */
	status = system("../decb/decb dskini test_hdb.dsk > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	status = system("../decb/decb hdbconv test_hdb.dsk test_hdb.bin > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	{
		struct stat st;

		ASSERT_EQUALS(0, stat("test_hdb.bin", &st));
		ASSERT_EQUALS(161280 * 2, (long) st.st_size);
	}

	/* The target is not replaced if it looks like an image. */
	memset(before, 0x5A, sizeof(before));
	fp = fopen("test_hdb_keep.dsk", "wb");
	fwrite(before, 1, sizeof(before), fp);
	fclose(fp);
	status = system("../decb/decb hdbconv test_hdb.dsk test_hdb_keep.dsk > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);
	fp = fopen("test_hdb_keep.dsk", "rb");
	got = fread(after, 1, sizeof(after), fp);
	fclose(fp);
	ASSERT_EQUALS(sizeof(before), got);
	ASSERT_EQUALS(0, memcmp(before, after, sizeof(before)));

	remove("test_hdb.dmk");
	remove("test_hdb.dsk");
	remove("test_hdb.bin");
	remove("test_hdb_keep.dsk");
}

/* Check a dskini makes the format asked for, and a disk that works. */
static void check_dskini_format(const char *arguments, const char *pathname, diskimage_format expected)
{
	char command[256];
	char text[] = "hello";
	char back[16];
	char pathlist[128];
	diskimage_format format = DISKIMAGE_UNKNOWN;
	decb_path_id p;
	u_int size = sizeof(text);
	error_code ec;
	int status;

	remove(pathname);
	snprintf(command, sizeof(command), "../decb/decb dskini %s %s > /dev/null 2>&1", arguments, pathname);
	status = system(command);
	ASSERT_EQUALS(0, status);

	ec = _diskimage_identify(pathname, &format);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(expected, format);

	snprintf(pathlist, sizeof(pathlist), "%s,FILE.TXT", pathname);
	ec = _decb_create(&p, pathlist, FAM_WRITE, 0, 0);
	ASSERT_EQUALS(0, ec);
	ec = _decb_write(p, text, &size);
	ASSERT_EQUALS(0, ec);
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);

	ec = _decb_open(&p, pathlist, FAM_READ);
	ASSERT_EQUALS(0, ec);
	size = sizeof(back);
	ec = _decb_read(p, back, &size);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(sizeof(text), size);
	ASSERT_EQUALS(0, memcmp(text, back, size));
	ec = _decb_close(p);
	ASSERT_EQUALS(0, ec);
	remove(pathname);
}

void test_decb_command_dskini_formats()
{
	u_int filesystems;
	error_code ec;
	int status;

	/* By extension */
	check_dskini_format("", "test_fmt.dmk", DISKIMAGE_DMK);
	check_dskini_format("", "test_fmt.SDF", DISKIMAGE_SDF);
	check_dskini_format("", "test_fmt.vdk", DISKIMAGE_VDK);
	check_dskini_format("", "test_fmt.dsk", DISKIMAGE_RAW);
	/* A JVC image of this layout needs no header, and so is raw. */
	check_dskini_format("", "test_fmt.jvc", DISKIMAGE_RAW);

	/* The option wins */
	check_dskini_format("-fdmk", "test_fmt.dsk", DISKIMAGE_DMK);
	check_dskini_format("-fDMK -4", "test_fmt.dat", DISKIMAGE_DMK);
	check_dskini_format("-fraw", "test_fmt.dmk", DISKIMAGE_RAW);

	/* It is found to be Disk BASIC */
	remove("test_fmt.dmk");
	status = system("../decb/decb dskini test_fmt.dmk > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	ec = _coco_detect_filesystems("test_fmt.dmk", &filesystems);
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(COCO_FS_DECB, filesystems);

	/* An existing disk of that format is initialized again, and any other left alone */
	status = system("../decb/decb dskini test_fmt.dmk > /dev/null 2>&1");
	ASSERT_EQUALS(0, status);
	status = system("../decb/decb dskini -fsdf test_fmt.dmk > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);
	status = system("../decb/decb dskini -fraw test_fmt.dmk > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);

	/* Many drives need a raw image */
	remove("test_fmt.dmk");
	status = system("../decb/decb dskini -h2 test_fmt.dmk > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);
	{
		FILE *fp = fopen("test_fmt.dmk", "rb");

		ASSERT("nothing was made", fp == NULL);
	}

	/* And an unknown format is refused */
	status = system("../decb/decb dskini -fxyz test_fmt.dmk > /dev/null 2>&1");
	ASSERT_NEQUALS(0, status);
}

int main()
{
	remove("test.dsk");
	RUN(test_decb_command_dskini);
	RUN(test_decb_command_copy);
	RUN(test_decb_command_copy_to_native_directory);
	RUN(test_decb_command_copy_eol_same_type);
	RUN(test_decb_command_copy_empty_metadata);
	RUN(test_decb_command_copy_token_metadata);
	RUN(test_decb_command_attr_no_comma_spec);
	RUN(test_decb_command_copy_raw_path);
	
	RUN(test_decb_command_copy_refuses_disk_images);
	RUN(test_decb_command_dsave_executable);
	RUN(test_decb_command_hdbconv);
	RUN(test_decb_command_dskini_formats);
	return TEST_REPORT();
}

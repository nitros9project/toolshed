/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>
#include <utime.h>

void test_os9_command_format()
{
	error_code ec;
	native_path_id nativepath;
	u_int size;

	ec = system("../os9/os9 format -e test.dsk -l65000 > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	
	ec = _native_open(&nativepath, "test.dsk", FAM_READ);
	ASSERT_EQUALS(0, ec);

	ec = _native_gs_size(nativepath, &size);	
	ASSERT_EQUALS(0, ec);
	ASSERT_EQUALS(65000*256, size);

	ec = _native_close(nativepath);
	ASSERT_EQUALS(0, ec);
	
	ec = _os9_makdir("test.dsk,/CMDS");
	ASSERT_EQUALS(0, ec);
}

void test_os9_command_copy_metadata()
{
	error_code ec;
	struct stat native_stat;
	os9_path_id os9path;
	fd_stats os9_fstat;
	int os9_fstat_size = sizeof(os9_fstat);
	fd_stats converted_native_fstat = {0};

	/* Detect reproducible build */
	char *sde_env = getenv("SOURCE_DATE_EPOCH");
	int use_sde = 0;
	time_t sde_val = 0;
	if (sde_env != NULL) {
		char *endptr;
		sde_val = strtoll(sde_env, &endptr, 10);
		if (*endptr == '\0' && sde_val >= 0) {
			use_sde = 1;
		}
	}
 
	/* Create a file on the disk image with known content */
	ec = _os9_create(&os9path, "test.dsk,meta.txt",
	                 FAM_READ | FAM_WRITE, FAP_READ | FAP_WRITE);
	ASSERT_EQUALS(0, ec);
	char *buf = "hello";
	u_int size = strlen(buf);
	ec = _os9_write(os9path, buf, &size);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(os9path);
	ASSERT_EQUALS(0, ec);
 
	/* Set the OS9 file's modification date to a fixed date in the 1980s:
	 * January 15, 1987 at 10:30 (OS9 fd_dat: year, month, day, hour, min).
	 * January is chosen to avoid DST ambiguity in the mktime round-trip. */
	ec = _os9_open(&os9path, "test.dsk,meta.txt", FAM_READ | FAM_WRITE);
	ASSERT_EQUALS(0, ec);
	ec = _os9_gs_fd(os9path, os9_fstat_size, &os9_fstat);
	ASSERT_EQUALS(0, ec);
	os9_fstat.fd_dat[0] = 87;  /* year: 1987 (years since 1900) */
	os9_fstat.fd_dat[1] = 1;   /* month: January */
	os9_fstat.fd_dat[2] = 15;  /* day */
	os9_fstat.fd_dat[3] = 10;  /* hour */
	os9_fstat.fd_dat[4] = 30;  /* minute */
	ec = _os9_ss_fd(os9path, os9_fstat_size, &os9_fstat);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(os9path);
	ASSERT_EQUALS(0, ec);
 
	/* Copy from OS9 image to native filesystem */
	ec = system("../os9/os9 copy test.dsk,meta.txt meta_out.txt > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
 
	/* Read the OS9 file's modification time */
	ec = _os9_open(&os9path, "test.dsk,meta.txt", FAM_READ);
	ASSERT_EQUALS(0, ec);
	ec = _os9_gs_fd(os9path, os9_fstat_size, &os9_fstat);
	ASSERT_EQUALS(0, ec);
	ec = _os9_close(os9path);
	ASSERT_EQUALS(0, ec);
 
	/* Check that the native file's mtime matches the OS9 last_modified_time */
	ec = stat("meta_out.txt", &native_stat);
	ASSERT_EQUALS(0, ec);
	UnixToOS9Time(native_stat.st_mtime, (char *) converted_native_fstat.fd_dat);
	
// 	fprintf(stderr, "os9_fstat.fd_dat: %d %d %d %d %d\n", os9_fstat.fd_dat[0], os9_fstat.fd_dat[1],os9_fstat.fd_dat[2], os9_fstat.fd_dat[3], os9_fstat.fd_dat[4]);
// 	fprintf(stderr, "converted_native_fstat.fd_dat: %d %d %d %d %d\n", converted_native_fstat.fd_dat[0], converted_native_fstat.fd_dat[1],converted_native_fstat.fd_dat[2], converted_native_fstat.fd_dat[3], converted_native_fstat.fd_dat[4]);
	
	if (use_sde) {
		/* Reproducible mode: Destination is pinned to SOURCE_DATE_EPOCH */
		ASSERT_EQUALS(sde_val, native_stat.st_mtime);

		struct tm *expected_tm = gmtime(&sde_val);
		fd_stats expected_from_sde = {0};
		expected_from_sde.fd_dat[0] = expected_tm->tm_year;
		expected_from_sde.fd_dat[1] = expected_tm->tm_mon + 1;
		expected_from_sde.fd_dat[2] = expected_tm->tm_mday;
		expected_from_sde.fd_dat[3] = expected_tm->tm_hour;
		expected_from_sde.fd_dat[4] = expected_tm->tm_min;

		ASSERT_MEM_EQUALS(expected_from_sde.fd_dat, converted_native_fstat.fd_dat, 5);
	} else {
		ASSERT_MEM_EQUALS(os9_fstat.fd_dat, converted_native_fstat.fd_dat, 5);
	}

	/* Change the native file's mtime to a different fixed date:
     * March 3, 1984 at 14:15 */
    struct utimbuf native_utime;
    struct tm native_tm;
    memset(&native_tm, 0, sizeof(native_tm));
    native_tm.tm_year = 84;  /* 1984 */
    native_tm.tm_mon  = 2;   /* March */
    native_tm.tm_mday = 3;
    native_tm.tm_hour = 14;
    native_tm.tm_min  = 15;
    native_tm.tm_isdst = -1;
    native_utime.modtime = mktime(&native_tm);
    native_utime.actime  = native_utime.modtime;
    ec = utime("meta_out.txt", &native_utime);
    ASSERT_EQUALS(0, ec);

    /* Copy native file back into the disk image */
    ec = system("../os9/os9 copy meta_out.txt test.dsk,meta_back.txt > /dev/null 2>&1");
    ASSERT_EQUALS(0, ec);

    /* Read the OS9 file's modification time */
    fd_stats os9_back_fstat;
    ec = _os9_open(&os9path, "test.dsk,meta_back.txt", FAM_READ);
    ASSERT_EQUALS(0, ec);
    ec = _os9_gs_fd(os9path, os9_fstat_size, &os9_back_fstat);
    ASSERT_EQUALS(0, ec);
    ec = _os9_close(os9path);
    ASSERT_EQUALS(0, ec);

    /* Check that the OS9 file's mtime matches the native file's mtime */
    fd_stats expected_back_fstat = {0};
	if (use_sde) {
		UnixToOS9Time(sde_val, (char *) expected_back_fstat.fd_dat);
	} else {
		UnixToOS9Time(native_utime.modtime, (char *) expected_back_fstat.fd_dat);
	}

// 	fprintf(stderr, "os9_back_fstat.fd_dat: %d %d %d %d %d\n", os9_back_fstat.fd_dat[0], os9_back_fstat.fd_dat[1],os9_back_fstat.fd_dat[2], os9_back_fstat.fd_dat[3], os9_back_fstat.fd_dat[4]);
// 	fprintf(stderr, "expected_back_fstat.fd_dat: %d %d %d %d %d\n", expected_back_fstat.fd_dat[0], expected_back_fstat.fd_dat[1],expected_back_fstat.fd_dat[2], expected_back_fstat.fd_dat[3], expected_back_fstat.fd_dat[4]);

    ASSERT_MEM_EQUALS(expected_back_fstat.fd_dat, os9_back_fstat.fd_dat, 5);

	remove("meta_out.txt");
}

void test_os9_command_copy_exit_status()
{
	error_code ec;
	FILE *file;
	coco_path_id path;
	const char contents[] = "copy status regression\n";
	char result[sizeof(contents)];
	size_t written;
	u_int size;
	int i;
	const char *commands[] = {
		"../os9/os9 copy copy_status_source copy_status.dsk,ONE.TXT > /dev/null 2>&1",
		"../os9/os9 copy copy_status_missing copy_status.dsk,MISSING.TXT > /dev/null 2>&1",
		"../os9/os9 copy copy_status_source copy_status.dsk,ONE.TXT > /dev/null 2>&1",
		"../os9/os9 copy copy_status_missing copy_status_source copy_status.dsk, > /dev/null 2>&1",
		"../os9/os9 copy -r copy_status_source copy_status_missing copy_status.dsk, > /dev/null 2>&1"
	};
	const int expected[] = {0, EOS_PNNF, EOS_FAE, EOS_PNNF, EOS_PNNF};

	ec = system("../os9/os9 format -e copy_status.dsk -l630 > /dev/null 2>&1");
	ASSERT_EQUALS(0, ec);
	remove("copy_status_missing");
	file = fopen("copy_status_source", "wb");
	ASSERT("create copy source", file != NULL);
	written = fwrite(contents, 1, sizeof(contents) - 1, file);
	ec = fclose(file);
	ASSERT_EQUALS(sizeof(contents) - 1, written);
	ASSERT_EQUALS(0, ec);

	for (i = 0; i < 5; i++)
	{
		ec = system(commands[i]);
		ASSERT_EQUALS(1, WIFEXITED(ec));
		ec = WEXITSTATUS(ec);
		ASSERT_EQUALS(expected[i], ec);

		/* A failed source must not prevent a later valid source from copying. */
		if (i == 3)
		{
			ec = _coco_open(&path, "copy_status.dsk,copy_status_source", FAM_READ);
			ASSERT_EQUALS(0, ec);
			size = sizeof(result);
			ec = _coco_read(path, result, &size);
			_coco_close(path);
			ASSERT_EQUALS(0, ec);
			ASSERT_EQUALS(sizeof(contents) - 1, size);
			ASSERT_MEM_EQUALS(contents, result, size);
		}
	}

	remove("copy_status_source");
	remove("copy_status.dsk");
}

int main()
{
	remove("test.dsk");
	RUN(test_os9_command_format);
	RUN(test_os9_command_copy_metadata);
	RUN(test_os9_command_copy_exit_status);

	return TEST_REPORT();
}

/*
 * ALWAYS BE TESTING!!!
 *
 * ALWAYS BE WRITING MORE TESTS!!!
 *
 * THIS WORK IS **NEVER** DONE!!!
 */

#include "tinytest.h"
#include <toolshed.h>

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

int main()
{
	remove("test.dsk");
	RUN(test_decb_command_dskini);
	RUN(test_decb_command_copy);
	
	return TEST_REPORT();
}

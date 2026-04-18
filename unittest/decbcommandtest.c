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

int main()
{
	remove("test.dsk");
	RUN(test_decb_command_dskini);
	
	return TEST_REPORT();
}

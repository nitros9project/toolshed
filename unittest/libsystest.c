/* Regression tests for system utilities. */

#include "tinytest.h"
#include "cocopath.h"
#include "cocosys.h"
#include <string.h>

static void test_os9_empty_name(void)
{
	ASSERT_EQUALS(EOS_BPNAM, _os9_prsnam(""));
}

static void test_os9_valid_names(void)
{
	ASSERT_EQUALS(0, _os9_prsnam("A"));
	ASSERT_EQUALS(0, _os9_prsnam("file-1.$"));
	ASSERT_EQUALS(0, _os9_prsnam("A.B.C"));
}

static void test_os9_name_length(void)
{
	char name[D_NAMELEN + 2];

	memset(name, 'A', D_NAMELEN);
	name[D_NAMELEN] = '\0';
	ASSERT_EQUALS(0, _os9_prsnam(name));
	name[D_NAMELEN] = 'A';
	name[D_NAMELEN + 1] = '\0';
	ASSERT_EQUALS(EOS_BPNAM, _os9_prsnam(name));
}

static void test_decb_empty_basename(void)
{
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam(""));
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam(".BAS"));
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("."));
}

static void test_decb_multiple_dots(void)
{
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("A.B.C"));
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("A.."));
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("A.B."));
}

static void test_decb_valid_names(void)
{
	ASSERT_EQUALS(0, _decb_prsnam("A"));
	ASSERT_EQUALS(0, _decb_prsnam("12345678"));
	ASSERT_EQUALS(0, _decb_prsnam("FILE.BAS"));
	ASSERT_EQUALS(0, _decb_prsnam("12345678.123"));
	ASSERT_EQUALS(0, _decb_prsnam("FILE."));
}

static void test_decb_name_too_long(void)
{
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("123456789"));
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("123456789.BAS"));
	ASSERT_EQUALS(EOS_BPNAM, _decb_prsnam("FILE.1234"));
}

static void test_cecb_graphical_names(void)
{
	ASSERT_EQUALS(0, _cecb_prsnam("HELLO"));
	ASSERT_EQUALS(0, _cecb_prsnam("file-1.$"));
}

static void test_cecb_control_characters(void)
{
	ASSERT_EQUALS(EOS_BPNAM, _cecb_prsnam("\n"));
	ASSERT_EQUALS(EOS_BPNAM, _cecb_prsnam("\t"));
	ASSERT_EQUALS(EOS_BPNAM, _cecb_prsnam("\177"));
	ASSERT_EQUALS(EOS_BPNAM, _cecb_prsnam("AB\001CD"));
}

static void test_cecb_maximum_length(void)
{
	ASSERT_EQUALS(0, _cecb_prsnam("12345678"));
}

static void test_cecb_name_too_long(void)
{
	ASSERT_EQUALS(EOS_BPNAM, _cecb_prsnam("123456789"));
}

int main(void)
{
	RUN(test_os9_empty_name);
	RUN(test_os9_valid_names);
	RUN(test_os9_name_length);
	RUN(test_decb_empty_basename);
	RUN(test_decb_multiple_dots);
	RUN(test_decb_valid_names);
	RUN(test_decb_name_too_long);
	RUN(test_cecb_graphical_names);
	RUN(test_cecb_control_characters);
	RUN(test_cecb_maximum_length);
	RUN(test_cecb_name_too_long);

	return TEST_REPORT();
}

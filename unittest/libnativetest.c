/* Regression tests for native file I/O. */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "tinytest.h"
#include "nativepath.h"

static void check_readln(const char *input, const char *expected, u_int capacity)
{
	struct _native_path_id path = {0};
	char buffer[16];
	u_int size = capacity;
	u_int eof_size = sizeof(buffer);
	size_t written;
	error_code ec, eof_ec;
	long position;
	char eof_buffer[16];

	path.mode = FAM_READ;
	path.fd = tmpfile();
	ASSERT("tmpfile", path.fd != NULL);
	written = fwrite(input, 1, strlen(input), path.fd);
	if (written != strlen(input))
	{
		fclose(path.fd);
		ASSERT_EQUALS(strlen(input), written);
	}
	rewind(path.fd);
	memset(buffer, 'X', sizeof(buffer));
	ec = _native_readln(&path, buffer, &size);
	position = ftell(path.fd);
	eof_ec = _native_readln(&path, eof_buffer, &eof_size);
	fclose(path.fd);

	ASSERT_EQUALS(input[0] == '\0' ? EOS_EOF : 0, ec);
	ASSERT_EQUALS(strlen(expected), size);
	ASSERT_MEM_EQUALS(expected, buffer, size);
	ASSERT_EQUALS('X', buffer[size]);
	ASSERT_EQUALS(strlen(input), position);
	ASSERT_EQUALS(EOS_EOF, eof_ec);
	ASSERT_EQUALS(0, eof_size);
}

static void test_readln_counts_newline(void)
{
	check_readln("hello\n", "hello\r", 16);
}

static void test_readln_empty_line(void)
{
	check_readln("\n", "\r", 16);
}

static void test_readln_newline_at_capacity(void)
{
	check_readln("hello\n", "hello\r", 6);
}

static void test_readln_unterminated_line(void)
{
	check_readln("hello", "hello", 16);
}

static void test_readln_empty_file(void)
{
	check_readln("", "", 16);
}

static void test_open_write_only_preserves_contents(void)
{
	char filename[] = "libnative-write-XXXXXX";
	const char original[] = "abcdef\r\n";
	char replacement[] = "Z\n";
	char buffer[16];
	native_path_id path = NULL;
	u_int size = sizeof(replacement) - 1;
	error_code ec, write_ec;
	ssize_t written;
	size_t count;
	int fd, perms_ec;
	FILE *file;

	fd = mkstemp(filename);
	ASSERT("mkstemp", fd >= 0);
	written = write(fd, original, sizeof(original) - 1);
	/* Opening for write must not require read permission. */
	perms_ec = fchmod(fd, S_IWUSR);
	close(fd);
	if (written != sizeof(original) - 1 || perms_ec != 0)
	{
		remove(filename);
		ASSERT_EQUALS(sizeof(original) - 1, written);
		ASSERT_EQUALS(0, perms_ec);
	}

	ec = _native_open(&path, filename, FAM_WRITE);
	if (ec != 0)
	{
		remove(filename);
		ASSERT_EQUALS(0, ec);
	}
	write_ec = _native_write(path, replacement, &size);
	_native_close(path);

	perms_ec = chmod(filename, S_IRUSR | S_IWUSR);
	file = fopen(filename, "rb");
	if (file == NULL)
	{
		remove(filename);
		ASSERT("fopen", file != NULL);
	}
	count = fread(buffer, 1, sizeof(buffer), file);
	fclose(file);
	remove(filename);

	ASSERT_EQUALS(0, write_ec);
	ASSERT_EQUALS(sizeof(replacement) - 1, size);
	ASSERT_EQUALS(0, perms_ec);
	ASSERT_EQUALS(sizeof(original) - 1, count);
	ASSERT_MEM_EQUALS("Z\ncdef\r\n", buffer, count);
}

static void test_open_write_only_does_not_create(void)
{
	char filename[] = "libnative-missing-XXXXXX";
	native_path_id path = NULL;
	error_code ec;
	int fd, remove_ec;
	FILE *file;

	fd = mkstemp(filename);
	ASSERT("mkstemp", fd >= 0);
	close(fd);
	remove_ec = remove(filename);
	ASSERT_EQUALS(0, remove_ec);

	ec = _native_open(&path, filename, FAM_WRITE);
	if (ec == 0)
	{
		_native_close(path);
	}
	file = fopen(filename, "rb");
	if (file != NULL)
	{
		fclose(file);
	}
	remove(filename);

	ASSERT_EQUALS(EOS_PNNF, ec);
	ASSERT("failed open clears path", path == NULL);
	ASSERT("open must not create a file", file == NULL);
}

static void test_seek_reports_failure(void)
{
	struct _native_path_id path = {0};
	error_code invalid_ec, valid_ec;
	long invalid_position, valid_position;

	path.mode = FAM_READ | FAM_WRITE;
	path.fd = tmpfile();
	ASSERT("tmpfile", path.fd != NULL);

	/* A negative absolute offset cannot be represented in a file. */
	invalid_ec = _native_seek(&path, -1, SEEK_SET);
	invalid_position = ftell(path.fd);
	/* A failed seek must not prevent a subsequent valid seek. */
	valid_ec = _native_seek(&path, 3, SEEK_SET);
	valid_position = ftell(path.fd);
	fclose(path.fd);

	ASSERT("seek before start must fail", invalid_ec != 0);
	ASSERT_EQUALS(0, invalid_position);
	ASSERT_EQUALS(0, valid_ec);
	ASSERT_EQUALS(3, valid_position);
}

static void test_rename_missing_source_reports_failure(void)
{
	char source[] = "libnative-rename-missing-XXXXXX";
	char destination[64];
	error_code ec;
	int fd, remove_ec, destination_exists;

	fd = mkstemp(source);
	ASSERT("mkstemp", fd >= 0);
	close(fd);
	remove_ec = remove(source);
	ASSERT_EQUALS(0, remove_ec);
	snprintf(destination, sizeof(destination), "%s-new", source);

	ec = _native_rename(source, destination);
	destination_exists = access(destination, F_OK) == 0;
	remove(destination);

	ASSERT_EQUALS(ENOENT, ec);
	ASSERT("failed rename must not create destination", !destination_exists);
}

static void test_rename_within_directory(void)
{
	char directory[] = "libnative-rename-XXXXXX";
	char source[64], destination[64];
	char new_name[] = "new.txt";
	const char contents[] = "rename payload\n";
	char buffer[32];
	error_code ec;
	size_t written, count = 0;
	int source_exists;
	FILE *file;

	ASSERT("mkdtemp", mkdtemp(directory) != NULL);
	snprintf(source, sizeof(source), "%s/original.txt", directory);
	snprintf(destination, sizeof(destination), "%s/%s", directory, new_name);
	file = fopen(source, "wb");
	if (file == NULL)
	{
		rmdir(directory);
		ASSERT("fopen source", file != NULL);
	}
	written = fwrite(contents, 1, sizeof(contents) - 1, file);
	fclose(file);

	ec = _native_rename(source, new_name);
	source_exists = access(source, F_OK) == 0;
	file = fopen(destination, "rb");
	if (file != NULL)
	{
		count = fread(buffer, 1, sizeof(buffer), file);
		fclose(file);
	}
	remove(source);
	remove(destination);
	rmdir(directory);

	ASSERT_EQUALS(sizeof(contents) - 1, written);
	ASSERT_EQUALS(0, ec);
	ASSERT("rename must remove original name", !source_exists);
	ASSERT_EQUALS(sizeof(contents) - 1, count);
	ASSERT_MEM_EQUALS(contents, buffer, count);
}

int main(void)
{
	RUN(test_readln_counts_newline);
	RUN(test_readln_empty_line);
	RUN(test_readln_newline_at_capacity);
	RUN(test_readln_unterminated_line);
	RUN(test_readln_empty_file);
	RUN(test_open_write_only_preserves_contents);
	RUN(test_open_write_only_does_not_create);
	RUN(test_seek_reports_failure);
	RUN(test_rename_missing_source_reports_failure);
	RUN(test_rename_within_directory);

	return TEST_REPORT();
}

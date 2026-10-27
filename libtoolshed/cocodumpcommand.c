/********************************************************************
 * Shared dump command for OS-9, Disk BASIC and Cassette BASIC paths.
 * Formatting is provided by cocodump.c in this library.
 ********************************************************************/
#include <util.h>
#include <cocopath.h>
#include <cocodump.h>

static char const *const helpMessage[] = {
	"Syntax: dump {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display the contents of a file in hexadecimal.\n",
	"Options:\n",
	"     -a    dump output in assembler format (hex)\n",
	"     -b    dump output in assembler format (binary)\n",
	"     -c    don't display ASCII character data\n",
	"     -e    dump output in C format\n",
	"     -t    don't display header\n",
	"     -h    don't display header (DEPRECATED)\n",
	"     -l    don't display line label/count\n",
	"     -z    decode DECB binary\n",
	NULL
};

/* Read up to length bytes, allowing backends to return short reads. */
static error_code read_bytes(coco_path_id path, unsigned char *data,
			     u_int length, u_int *actual)
{
	error_code ec = 0;
	*actual = 0;
	while (*actual < length)
	{
		u_int count = length - *actual;
		ec = _coco_read(path, data + *actual, &count);
		if (ec && ec != EOS_EOF) return ec;
		*actual += count;
		if (ec || count == 0) break;
	}
	return ec;
}

static error_code dump_data(coco_path_id path, coco_dump *dump,
			    size_t length, int limited)
{
	unsigned char buffer[256];
	while (!limited || length)
	{
		u_int count, request = sizeof(buffer);
		error_code ec;
		if (limited && length < request) request = length;
		ec = read_bytes(path, buffer, request, &count);
		if (ec && ec != EOS_EOF) return ec;
		coco_dump_write(dump, buffer, count);
		if (limited) length -= count;
		if (ec == EOS_EOF || count == 0)
			return limited && length ? EOS_EOF : 0;
	}
	return 0;
}

static error_code dump_decb(coco_path_id path, const coco_dump_options *options)
{
	unsigned char header[5];
	for (;;)
	{
		u_int count, length, address;
		error_code ec = read_bytes(path, header, sizeof(header), &count);
		coco_dump dump;
		if (ec && ec != EOS_EOF) return ec;
		if (count == 0) return 0;
		if (count != sizeof(header)) return EOS_EOF;
		length = (header[1] << 8) | header[2];
		address = (header[3] << 8) | header[4];
		if (header[0] == 0xff)
		{
			size_t extra = 0;
			printf("\nExecution address: $%04X\n", address);
			do
			{
				ec = read_bytes(path, header, sizeof(header), &count);
				if (ec && ec != EOS_EOF) return ec;
				extra += count;
			} while (!ec && count);
			if (extra) printf("%zu bytes extra at end of file.\n", extra);
			return 0;
		}
		if (header[0] != 0)
		{
			fprintf(stderr, "Expected 0 or 255 for DECB block header\n");
			return 1;
		}
		coco_dump_begin(&dump, stdout, options, address);
		ec = dump_data(path, &dump, length, 1);
		coco_dump_end(&dump);
		if (ec) return ec;
	}
}

static error_code dump_file(char *file, const coco_dump_options *options,
			    int decb_binary)
{
	coco_path_id path;
	coco_dump dump;
	error_code ec, close_ec;
	ec = _coco_open(&path, file, FAM_READ);
	if (ec) ec = _coco_open(&path, file, FAM_DIR | FAM_READ);
	if (ec) return ec;
	if (decb_binary) ec = dump_decb(path, options);
	else
	{
		coco_dump_begin(&dump, stdout, options, 0);
		ec = dump_data(path, &dump, 0, 0);
		coco_dump_end(&dump);
	}
	if (!ec && ferror(stdout)) ec = 1;
	close_ec = _coco_close(path);
	return ec ? ec : close_ec;
}

int cocodump(int argc, char **argv)
{
	coco_dump_options options;
	int i, decb_binary = 0;
	coco_dump_defaults(&options);
	if (argc < 2)
	{
		show_help(helpMessage);
		return 0;
	}
	for (i = 1; i < argc; i++)
	{
		char *p;
		if (argv[i][0] != '-') continue;
		for (p = argv[i] + 1; *p; p++)
		{
			if (coco_dump_option(&options, *p)) continue;
			switch (*p)
			{
			case 'z': decb_binary = 1; break;
			case 'h':
				fprintf(stderr, "%s: warning: option -h is deprecated, use -t\n", argv[0]);
				options.header = 0;
				break;
			case '?': show_help(helpMessage); return 0;
			default:
				fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
				return 1;
			}
		}
	}
	for (i = 1; i < argc; i++)
	{
		error_code ec;
		if (argv[i][0] == '-') continue;
		ec = dump_file(argv[i], &options, decb_binary);
		if (ec)
		{
			fprintf(stderr, "%s: error %d dumping '%s'\n", argv[0], ec, argv[i]);
			return ec;
		}
	}
	return 0;
}

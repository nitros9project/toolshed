#include <cocodump.h>

#ifdef _WIN32
#include <io.h>
#define is_terminal(output) _isatty(_fileno(output))
#else
#include <unistd.h>
#define is_terminal(output) isatty(fileno(output))
#endif

static void ascii_color(coco_dump *dump, unsigned char c, int begin)
{
	if (dump->color && c >= 32 && c < 127)
		fputs(begin ? "\033[32m" : "\033[0m", dump->output);
}

void coco_dump_defaults(coco_dump_options *options)
{
	options->format = COCO_DUMP_HEX;
	options->ascii = options->header = options->label = 1;
}

int coco_dump_option(coco_dump_options *options, int option)
{
	switch (option)
	{
	case 'a': options->format = COCO_DUMP_ASMHEX; break;
	case 'b': options->format = COCO_DUMP_ASMBIN; break;
	case 'e': options->format = COCO_DUMP_C; break;
	case 'c': options->ascii = 0; break;
	case 't': options->header = 0; break;
	case 'l': options->label = 0; break;
	default: return 0;
	}
	return 1;
}

static size_t line_width(const coco_dump *dump)
{
	switch (dump->options.format)
	{
	case COCO_DUMP_ASMBIN: return 1;
	case COCO_DUMP_ASMHEX:
	case COCO_DUMP_C: return 8;
	default: return 16;
	}
}

static void dump_header(coco_dump *dump)
{
	FILE *out = dump->output;
	if (dump->options.format != COCO_DUMP_HEX || !dump->options.header)
		return;
	fputs("\n\n", out);
	if (dump->options.label) fputs("  Addr    ", out);
	fputs(" 0 1  2 3  4 5  6 7  8 9  A B  C D  E F", out);
	if (dump->options.ascii) fputs(" 0 2 4 6 8 A C E", out);
	fputc('\n', out);
	if (dump->options.label) fputs("--------  ", out);
	fputs("---- ---- ---- ---- ---- ---- ---- ----", out);
	if (dump->options.ascii) fputs(" ----------------", out);
}

static void dump_line(coco_dump *dump)
{
	FILE *out = dump->output;
	size_t i, columns = 0, target;
	enum coco_dump_format format = dump->options.format;

	if (dump->lines % 16 == 0) dump_header(dump);
	fputc('\n', out);
	if (format == COCO_DUMP_HEX)
	{
		if (dump->options.label) fprintf(out, "%08zx  ", dump->offset);
	}
	else if (format == COCO_DUMP_C)
	{
		if (dump->options.label)
			fprintf(out, "   /* offset = %08zX */ ", dump->offset);
		else fputs("   ", out);
	}
	else
	{
		if (dump->options.label) fprintf(out, "L%04zX    fcb   ", dump->offset);
		else fputs("         fcb   ", out);
	}

	for (i = 0; i < dump->count; i++)
	{
		unsigned char c = dump->line[i];
		if (format == COCO_DUMP_HEX)
		{
			ascii_color(dump, c, 1);
			columns += fprintf(out, "%02x", c);
			ascii_color(dump, c, 0);
			if (i % 2) { fputc(' ', out); columns++; }
		}
		else if (format == COCO_DUMP_C)
		{
			ascii_color(dump, c, 1);
			columns += fprintf(out, "0x%02X", c);
			ascii_color(dump, c, 0);
			fputc(',', out);
			columns++;
		}
		else
		{
			if (i) { fputc(',', out); columns++; }
			ascii_color(dump, c, 1);
			if (format == COCO_DUMP_ASMHEX)
				columns += fprintf(out, "$%02X", c);
			else
			{
				int bit;
				fputc('%', out);
				for (bit = 7; bit >= 0; bit--)
					fputc((c & (1 << bit)) ? '1' : '0', out);
				columns += 9;
			}
			ascii_color(dump, c, 0);
		}
	}
	if (dump->options.ascii)
	{
		target = format == COCO_DUMP_HEX ? 40 :
			format == COCO_DUMP_C ? 40 :
			format == COCO_DUMP_ASMHEX ? 34 : 11;
		while (columns++ < target) fputc(' ', out);
		if (format == COCO_DUMP_C) fputs("  // ", out);
		for (i = 0; i < dump->count; i++)
		{
			unsigned char c = dump->line[i];
			ascii_color(dump, dump->line[i], 1);
			if (c >= 160 && c <= 128 + 'z') c -= 128;
			fputc(c >= 32 && c < 127 ? c : '.', out);
			ascii_color(dump, dump->line[i], 0);
		}
	}
	dump->offset += dump->count;
	dump->count = 0;
	dump->lines++;
}

void coco_dump_begin(coco_dump *dump, FILE *output,
		     const coco_dump_options *options, size_t offset)
{
	dump->output = output;
	dump->options = *options;
	dump->offset = offset;
	dump->count = dump->lines = 0;
	dump->color = is_terminal(output);
	if (options->format == COCO_DUMP_C) fputc('{', output);
}

void coco_dump_write(coco_dump *dump, const void *data, size_t length)
{
	const unsigned char *bytes = data;
	size_t width = line_width(dump);
	while (length--)
	{
		dump->line[dump->count++] = *bytes++;
		if (dump->count == width) dump_line(dump);
	}
}

void coco_dump_end(coco_dump *dump)
{
	if (dump->count) dump_line(dump);
	if (dump->options.format == COCO_DUMP_C) fputs("\n}", dump->output);
	fputc('\n', dump->output);
}

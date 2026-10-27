/********************************************************************
 * cecbdumpblock.c - Dump CoCo Cassette blocks
 *
 * $Id$
 ********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cecbpath.h>
#include <cocopath.h>
#include <util.h>
#include <cocodump.h>

#ifdef _WIN32
#include <io.h>
#define is_stdout_terminal() _isatty(_fileno(stdout))
#else
#include <unistd.h>
#define is_stdout_terminal() isatty(fileno(stdout))
#endif

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: dumpblock [<opts>] <imagefile> [<opts>]\n",
	"Usage:  Scan a CAS/WAV image and print a hex dump of each block.\n",
	"Options:\n",
	"     -h, ?       Show this help message\n",
	"     -a          dump output in assembler format (hex)\n",
	"     -b          dump output in assembler format (binary)\n",
	"     -e          dump output in C format\n",
	"     -c          don't display ASCII character data\n",
	"     -t          don't display header\n",
	"     -l          don't display line label/count\n",
	NULL
};

int cecbdumpblock(int argc, char *argv[])
{
	error_code ec = 0;
	char *srcfile = NULL;
	cecb_path_id path = NULL;
	int i;
	unsigned char block_type, block_length;
	unsigned char data[256];
	int use_color = is_stdout_terminal();
	coco_dump_options options;
	coco_dump dump;
	coco_dump_defaults(&options);

	/* 1. Walk command line for options and source file. */
	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "?") == 0)
		{
			show_help(helpMessage);
			return 0;
		}
		else if (argv[i][0] == '-')
		{
			char *p;
			for (p = argv[i] + 1; *p; p++)
			{
				if (*p == '?') { show_help(helpMessage); return 0; }
				if (!coco_dump_option(&options, *p))
				{
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return 1;
				}
			}
		}
		else if (srcfile == NULL)
		{
			srcfile = argv[i];
		}
		else
		{
			fprintf(stderr, "%s: unexpected argument '%s'\n", argv[0], argv[i]);
			show_help(helpMessage);
			return 1;
		}
	}

	if (srcfile == NULL)
	{
		show_help(helpMessage);
		return 0;
	}

	/* 2. Open the cassette path. */
	ec = _cecb_open(&path, srcfile, FAM_READ);
	if (ec != 0)
	{
		fprintf(stderr, "%s: `%s`: open error %d\n", argv[0], srcfile, ec);
		return 1;
	}

	printf("Scanning blocks in: %s\n", srcfile);
	switch (path->tape_type)
	{
		case WAV:
			printf("Format: WAV\n");
			break;
		case CAS:
		case C10:
			printf("Format: CAS/C10\n");
			break;
		default:
			printf("Format: Unknown\n");
			break;
	}
	printf("------------------------------------------------------------\n");

	/* 3. Read and dump each block. */
	
	do
	{
		ec = _cecb_read_next_block(path, &block_type, &block_length, data);
		
		if (ec == EOS_EOF)
		{
			ec = 0;
			break;
		}
		/* A checksum mismatch still provides a complete, useful block. */
		if (ec != 0 && ec != EOS_CRC)
		{
			fprintf(stderr, "%s: block read error %d\n", argv[0], ec);
			break;
		}
		printf("Block: Type=0x%02x, Len=%d", block_type, block_length);

		/* Handle CRC warnings gracefully (non-fatal for dump) */
		if (path->block_cksum != path->block_cksum_calc)
		{
			if (use_color)
			{
				printf(", \033[31m CKSUM Error=0x%02x ≠ 0x%02x\033[0m", path->block_cksum, path->block_cksum_calc);
			}
			else
			{
				printf(", CKSUM Error=0x%02x ≠ 0x%02x", path->block_cksum, path->block_cksum_calc);
			}
		}
		else
			printf(", CKSUM=0x%02x", path->block_cksum);

		if (path->tape_type == WAV)
		{
			printf(", End Sample=%ld\n", path->wav_current_sample);
		}
		else
		{
			printf(", End Byte=%ld, End Bit=%d\n", path->cas_current_byte, path->cas_current_bit);
		}
		
		/* Print hex dump of block data */
		coco_dump_begin(&dump, stdout, &options, 0);
		coco_dump_write(&dump, data, block_length);
		coco_dump_end(&dump);
		printf("\n");
	} while (1);

	/* 4. Close the path. */
	{
		error_code close_ec = _cecb_close(path);
		if (!ec) ec = close_ec;
	}
	if (!ec && ferror(stdout)) ec = 1;

	return ec;
}

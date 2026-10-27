/* Shared, incremental dump formatting. No file-system dependencies. */
#ifndef COCODUMP_H
#define COCODUMP_H

#include <stdio.h>

enum coco_dump_format
{
	COCO_DUMP_HEX,
	COCO_DUMP_ASMHEX,
	COCO_DUMP_ASMBIN,
	COCO_DUMP_C
};

typedef struct
{
	enum coco_dump_format format;
	int ascii, header, label;
} coco_dump_options;

typedef struct
{
	FILE *output;
	coco_dump_options options;
	size_t offset, count, lines;
	int color;
	unsigned char line[16];
} coco_dump;

void coco_dump_defaults(coco_dump_options *options);
/* Returns nonzero if the formatting option was recognized. */
int coco_dump_option(coco_dump_options *options, int option);
void coco_dump_begin(coco_dump *dump, FILE *output,
		     const coco_dump_options *options, size_t offset);
/* Writes can be split at any byte boundary. */
void coco_dump_write(coco_dump *dump, const void *data, size_t length);
void coco_dump_end(coco_dump *dump);

#endif

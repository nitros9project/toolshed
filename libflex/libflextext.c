/********************************************************************
 * libflextext.c - Text and binary file formats
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "libflexint.h"

#define FX_TEXT_COMPRESS	0x09	/* followed by a count of spaces */
#define FX_TEXT_IGNORED		0x18	/* dropped when read, as $00 is */
#define FX_COMPRESS_MAX		127	/* the most spaces FLEX puts in one count */
#define FX_TAB_STOP		8
#define FX_BINARY_DATA		0x02
#define FX_BINARY_TRANSFER	0x16


error_code _flex_text_to_native(const u_char *in_buffer, u_int in_size, u_char **out_buffer, u_int *out_size)
{
	u_int capacity = in_size + 1;
	u_int length = 0;
	u_int i;
	u_char *out;

	/* A count can turn 2 bytes into 255. */
	for (i = 0; i + 1 < in_size; i++)
	{
		if (in_buffer[i] == FX_TEXT_COMPRESS)
		{
			capacity += in_buffer[i + 1];
			i++;
		}
	}

	out = malloc(capacity);
	if (out == NULL)
	{
		return EOS_OM;
	}

	for (i = 0; i < in_size; i++)
	{
		u_char c = in_buffer[i];

		/* As FLEX reads text, nul and $18 are dropped. */
		if (c == '\0' || c == FX_TEXT_IGNORED)
		{
			continue;
		}
		if (c == '\r')
		{
			out[length++] = '\n';
		}
		else if (c == FX_TEXT_COMPRESS && i + 1 < in_size)
		{
			memset(&out[length], ' ', in_buffer[i + 1]);
			length += in_buffer[i + 1];
			i++;
		}
		else
		{
			out[length++] = c;
		}
	}

	*out_buffer = out;
	*out_size = length;
	return 0;
}

/* Record a run of spaces as FLEX does: one alone stays a space, and more
   become $09 and a count, of at most 127. */
static u_int put_spaces(u_char *out, u_int length, u_int spaces)
{
	while (spaces > 0)
	{
		u_int count = spaces > FX_COMPRESS_MAX ? FX_COMPRESS_MAX : spaces;

		if (count == 1)
		{
			out[length++] = ' ';
		}
		else
		{
			out[length++] = FX_TEXT_COMPRESS;
			out[length++] = (u_char)count;
		}
		spaces -= count;
	}
	return length;
}

error_code _flex_text_from_native(const u_char *in_buffer, u_int in_size, u_char **out_buffer, u_int *out_size)
{
	/* No byte becomes more than 2: a tab, or a run of spaces, at most $09
	   and a count each. */
	u_char *out = malloc(in_size * 2 + 1);
	u_int length = 0;
	u_int column = 0;
	u_int spaces = 0;
	u_int i;

	if (out == NULL)
	{
		return EOS_OM;
	}

	for (i = 0; i < in_size; i++)
	{
		u_char c = in_buffer[i];

		if (c == ' ')
		{
			spaces++;
			column++;
			continue;
		}
		if (c == '\t')
		{
			u_int to_stop = FX_TAB_STOP - column % FX_TAB_STOP;

			spaces += to_stop;
			column += to_stop;
			continue;
		}

		length = put_spaces(out, length, spaces);
		spaces = 0;
		if (c == '\r' || c == '\n')
		{
			if (c == '\r' && i + 1 < in_size && in_buffer[i + 1] == '\n')
			{
				i++;
			}
			out[length++] = '\r';
			column = 0;
		}
		else
		{
			out[length++] = c;
			column++;
		}
	}
	length = put_spaces(out, length, spaces);

	*out_buffer = out;
	*out_size = length;
	return 0;
}

error_code _flex_binary_info(const u_char *buffer, u_int size, u_int *load_low, u_int *load_high,
			     u_int *transfer, int *has_transfer, u_int *records)
{
	u_int i = 0;
	u_int data_records = 0;

	*load_low = 0xFFFF;
	*load_high = 0;
	*transfer = 0;
	*has_transfer = 0;
	*records = 0;

	while (i < size)
	{
		switch (buffer[i])
		{
		case '\0':
			i++;
			break;

		case FX_BINARY_DATA:
		{
			u_int address, count;

			if (i + 4 > size)
			{
				return EOS_WT;
			}
			address = _fx_get2(&buffer[i + 1]);
			count = buffer[i + 3];
			if (i + 4 + count > size)
			{
				return EOS_WT;
			}
			if (count != 0)
			{
				u_int last = address + count - 1;

				if (address < *load_low)
				{
					*load_low = address;
				}
				if (last > *load_high)
				{
					*load_high = last;
				}
			}
			data_records++;
			(*records)++;
			i += 4 + count;
			break;
		}

		case FX_BINARY_TRANSFER:
			if (i + 3 > size)
			{
				return EOS_WT;
			}
			*transfer = _fx_get2(&buffer[i + 1]);
			*has_transfer = 1;
			(*records)++;
			i += 3;
			break;

		default:
			return EOS_WT;
		}
	}

	if (*records == 0)
	{
		return EOS_WT;
	}
	if (data_records == 0)
	{
		*load_low = 0;
	}
	return 0;
}

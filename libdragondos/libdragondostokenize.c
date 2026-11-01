/********************************************************************
 * libdragondostokenize.c - Dragon BASIC tokenization routines
 *
 * The tables are those of the Dragon 64 BASIC ROMs and Dragon DOS 1.0, in
 * token order.  Disk commands and functions carry on from BASIC's, as they
 * do once Dragon DOS has added them.  Both Dragon 64 ROMs have the same
 * tables and routines.
 *
 * _dragondos_entoken follows the routine at $8F67 of the BASIC ROM, and
 * _dragondos_detoken the one at $8F08.
 ********************************************************************/
#include <stdlib.h>
#include <string.h>

#include "libdragondosint.h"

#define TOKEN_BASE		0x80
#define TOKEN_REM		0x82
#define TOKEN_APOSTROPHE	0x83
#define TOKEN_ELSE		0x84
#define TOKEN_DATA		0x86
#define TOKEN_PRINT		0x87
#define FUNCTION_PREFIX		0xFF

#define HEADER_SIZE		9
#define HEADER_START		0x55
#define HEADER_END		0xAA
#define HEADER_TYPE_BASIC	1
#define BASIC_EXEC_ADDRESS	0x8B8D	/* what SAVE records for any program */
#define MAX_LINE_NUMBER		63999

static const char *const commands[] =
{
	"FOR", "GO", "REM", "'", "ELSE", "IF", "DATA", "PRINT",
	"ON", "INPUT", "END", "NEXT", "DIM", "READ", "LET", "RUN",
	"RESTORE", "RETURN", "STOP", "POKE", "CONT", "LIST", "CLEAR",
	"NEW", "DEF", "CLOAD", "CSAVE", "OPEN", "CLOSE", "LLIST",
	"SET", "RESET", "CLS", "MOTOR", "SOUND", "AUDIO", "EXEC",
	"SKIPF", "DEL", "EDIT", "TRON", "TROFF", "LINE", "PCLS", "PSET",
	"PRESET", "SCREEN", "PCLEAR", "COLOR", "CIRCLE", "PAINT",
	"GET", "PUT", "DRAW", "PCOPY", "PMODE", "PLAY", "DLOAD", "RENUM",
	"TAB(", "TO", "SUB", "FN", "THEN", "NOT", "STEP", "OFF", "+",
	"-", "*", "/", "^", "AND", "OR", ">", "=", "<", "USING",
	/* Dragon DOS */
	"AUTO", "BACKUP", "BEEP", "BOOT", "CHAIN", "COPY", "CREATE", "DIR",
	"DRIVE", "DSKINIT", "FREAD", "FWRITE", "ERROR", "KILL", "LOAD",
	"MERGE", "PROTECT", "WAIT", "RENAME", "SAVE", "SREAD", "SWRITE",
	"VERIFY", "FROM", "FLREAD", "SWAP"
};

static const char *const functions[] =
{
	"SGN", "INT", "ABS", "POS", "RND", "SQR", "LOG",
	"EXP", "SIN", "COS", "TAN", "ATN", "PEEK", "LEN",
	"STR$", "VAL", "ASC", "CHR$", "EOF", "JOYSTK",
	"FIX", "HEX$", "LEFT$", "RIGHT$", "MID$", "POINT", "INKEY$", "MEM",
	"VARPTR", "INSTR", "TIMER", "PPOINT", "STRING$", "USR",
	/* Dragon DOS */
	"LOF", "FREE", "ERL", "ERR", "HIMEM", "LOC", "FRE$"
};

#define COMMAND_COUNT	(sizeof(commands) / sizeof(commands[0]))
#define FUNCTION_COUNT	(sizeof(functions) / sizeof(functions[0]))


/* Find the first of a table's keywords at the start of some text. */
static int find_keyword(const char *const *table, size_t count, const u_char *text, size_t length, size_t *keyword_length)
{
	size_t i;

	for (i = 0; i < count; i++)
	{
		size_t size = strlen(table[i]);

		if (size <= length && memcmp(table[i], text, size) == 0)
		{
			*keyword_length = size;
			return (int)i;
		}
	}
	return -1;
}

static int is_letter(u_char c)
{
	return c >= 'A' && c <= 'Z';
}

static int is_digit(u_char c)
{
	return c >= '0' && c <= '9';
}

/* Tokenize the text of one line, returning where its tokens end.  Only upper
   case is a keyword, and a keyword inside a variable name isn't one. */
static u_char *entoken_line(const u_char *text, size_t length, u_char *out)
{
	size_t i = 0;
	int in_variable = 0, in_data = 0;

	while (i < length)
	{
		u_char c = text[i++];
		size_t size;
		int token;

		if (in_variable)
		{
			if (is_letter(c) || is_digit(c))
			{
				*out++ = c;
				continue;
			}
			in_variable = 0;
		}

		if (c == ' ')
		{
			*out++ = c;
			continue;
		}

		if (c == '"')
		{
			*out++ = c;
			while (i < length)
			{
				c = text[i++];
				*out++ = c;
				if (c == '"')
				{
					break;
				}
			}
			continue;
		}

		if (in_data)
		{
			*out++ = c;
			if (c == ':')
			{
				in_data = 0;
			}
			continue;
		}

		if (c == '?')
		{
			*out++ = TOKEN_PRINT;
			continue;
		}

		/* Digits, colon and semicolon.  Not a keyword's start. */
		if (c >= '0' && c < '<')
		{
			*out++ = c;
			continue;
		}

		token = find_keyword(commands, COMMAND_COUNT, text + i - 1, length - i + 1, &size);
		if (token >= 0)
		{
			token += TOKEN_BASE;
			i += size - 1;

			/* These two carry a colon, so they can follow a statement. */
			if (token == TOKEN_ELSE || token == TOKEN_APOSTROPHE)
			{
				*out++ = ':';
			}
			*out++ = (u_char)token;

			if (token == TOKEN_DATA)
			{
				in_data = 1;
			}
			if (token == TOKEN_REM || token == TOKEN_APOSTROPHE)
			{
				memcpy(out, text + i, length - i);
				out += length - i;
				i = length;
			}
			continue;
		}

		token = find_keyword(functions, FUNCTION_COUNT, text + i - 1, length - i + 1, &size);
		if (token >= 0)
		{
			*out++ = FUNCTION_PREFIX;
			*out++ = (u_char)(token + TOKEN_BASE);
			i += size - 1;
			continue;
		}

		*out++ = c;
		in_variable = is_letter(c);
	}

	return out;
}

error_code _dragondos_entoken(const u_char *in_buffer, u_int in_size, u_char **out_buffer, u_int *out_size, u_int load_address)
{
	u_char *text, *out, *program;
	u_int pos = 0;
	long last_line = -1;
	u_int program_size;

	*out_buffer = NULL;
	*out_size = 0;

	/* Keep the caller's buffer, which holds only what can't be typed as
	   spaces.  An apostrophe doubles in size, the most anything can. */
	text = malloc((size_t)in_size + 1);
	out = malloc((size_t)in_size * 2 + HEADER_SIZE + 2 + 1);
	if (text == NULL || out == NULL)
	{
		free(text);
		free(out);
		return EOS_OM;
	}

	{
		u_int i;

		for (i = 0; i < in_size; i++)
		{
			u_char c = in_buffer[i] & 0x7F;

			text[i] = (c == '\r' || c == '\n' || (c >= ' ' && c < 0x7F)) ? c : ' ';
		}
	}

	program = out + HEADER_SIZE;
	program_size = 0;

	while (pos < in_size)
	{
		u_int end = pos, start;
		u_int line_start = program_size;
		long line_number = 0;
		u_char *line_end;

		while (end < in_size && text[end] != '\r' && text[end] != '\n')
		{
			end++;
		}

		start = pos;
		pos = end + 1;

		while (start < end && text[start] == ' ')
		{
			start++;
		}
		if (start == end)
		{
			continue;
		}

		if (!is_digit(text[start]))
		{
			free(text);
			free(out);
			return EOS_SN;
		}
		while (start < end && is_digit(text[start]))
		{
			line_number = line_number * 10 + (text[start++] - '0');
			if (line_number > MAX_LINE_NUMBER)
			{
				free(text);
				free(out);
				return EOS_SN;
			}
		}
		while (start < end && text[start] == ' ')
		{
			start++;
		}

		/* A number alone would delete a line, so makes none. */
		if (start == end)
		{
			continue;
		}

		/* BASIC would put lines in order.  This can't, so won't hide it. */
		if (line_number <= last_line)
		{
			free(text);
			free(out);
			return EOS_SN;
		}
		last_line = line_number;

		program[line_start + 2] = (u_char)(line_number >> 8);
		program[line_start + 3] = (u_char)line_number;
		line_end = entoken_line(text + start, end - start, program + line_start + 4);
		*line_end++ = 0;
		program_size = (u_int)(line_end - program);

		program[line_start] = (u_char)((load_address + program_size) >> 8);
		program[line_start + 1] = (u_char)(load_address + program_size);
	}

	free(text);

	if (program_size == 0 || program_size + 2 > 0xFFFF)
	{
		free(out);
		return EOS_SN;
	}

	program[program_size++] = 0;
	program[program_size++] = 0;

	out[0] = HEADER_START;
	out[1] = HEADER_TYPE_BASIC;
	out[2] = (u_char)(load_address >> 8);
	out[3] = (u_char)load_address;
	out[4] = (u_char)(program_size >> 8);
	out[5] = (u_char)program_size;
	out[6] = (u_char)(BASIC_EXEC_ADDRESS >> 8);
	out[7] = (u_char)BASIC_EXEC_ADDRESS;
	out[8] = HEADER_END;

	*out_buffer = out;
	*out_size = HEADER_SIZE + program_size;
	return 0;
}


typedef struct
{
	char *text;
	size_t size;
	size_t capacity;
} text_buffer;

static error_code append(text_buffer *buffer, const char *text, size_t size)
{
	if (buffer->size + size > buffer->capacity)
	{
		size_t capacity = (buffer->capacity + size) * 2;
		char *grown = realloc(buffer->text, capacity);

		if (grown == NULL)
		{
			return EOS_OM;
		}
		buffer->text = grown;
		buffer->capacity = capacity;
	}
	memcpy(buffer->text + buffer->size, text, size);
	buffer->size += size;
	return 0;
}

static error_code append_token(text_buffer *buffer, const char *const *table, size_t count, u_char token)
{
	size_t index = token & 0x7F;

	if (index >= count)
	{
		return append(buffer, "!", 1);
	}
	return append(buffer, table[index], strlen(table[index]));
}

/* Is there a header that says this is a BASIC program? */
error_code _dragondos_detect_tokenized(const u_char *in_buffer, u_int in_size)
{
	if (in_size < HEADER_SIZE || in_buffer[0] != HEADER_START || in_buffer[8] != HEADER_END ||
	    in_buffer[1] != HEADER_TYPE_BASIC ||
	    (u_int)((in_buffer[4] << 8) | in_buffer[5]) > in_size - HEADER_SIZE)
	{
		return EOS_SN;
	}
	return 0;
}

static error_code detoken_program(const u_char *in, u_int in_size, text_buffer *buffer)
{
	u_int pos = 0;

	for (;;)
	{
		char number[16];
		error_code ec;

		if (pos + 2 > in_size)
		{
			return EOS_SN;
		}
		if (in[pos] == 0 && in[pos + 1] == 0)
		{
			return 0;
		}
		if (pos + 4 > in_size)
		{
			return EOS_SN;
		}

		sprintf(number, "%u ", (in[pos + 2] << 8) | in[pos + 3]);
		pos += 4;
		ec = append(buffer, number, strlen(number));

		while (ec == 0)
		{
			u_char c;

			if (pos >= in_size)
			{
				return EOS_SN;
			}
			c = in[pos++];
			if (c == 0)
			{
				break;
			}

			if (c == FUNCTION_PREFIX)
			{
				if (pos >= in_size)
				{
					return EOS_SN;
				}
				ec = append_token(buffer, functions, FUNCTION_COUNT, in[pos++]);
			}
			else if (c >= TOKEN_BASE)
			{
				ec = append_token(buffer, commands, COMMAND_COUNT, c);
			}
			else if (c != ':' || pos >= in_size || (in[pos] != TOKEN_ELSE && in[pos] != TOKEN_APOSTROPHE))
			{
				char character = (char)c;

				ec = append(buffer, &character, 1);
			}
		}

		if (ec == 0)
		{
			ec = append(buffer, "\n", 1);
		}
		if (ec != 0)
		{
			return ec;
		}
	}
}

/* A header is checked if there is one.  With none, the buffer is taken to be
   a program as BASIC holds it. */
error_code _dragondos_detoken(const u_char *in_buffer, u_int in_size, char **out_buffer, u_int *out_size)
{
	text_buffer buffer = { NULL, 0, 0 };
	error_code ec;

	*out_buffer = NULL;
	*out_size = 0;

	if (in_size >= HEADER_SIZE && in_buffer[0] == HEADER_START && in_buffer[8] == HEADER_END)
	{
		ec = _dragondos_detect_tokenized(in_buffer, in_size);
		if (ec != 0)
		{
			return in_buffer[1] != HEADER_TYPE_BASIC ? EOS_WT : ec;
		}
		in_size = (u_int)((in_buffer[4] << 8) | in_buffer[5]);
		in_buffer += HEADER_SIZE;
	}

	ec = detoken_program(in_buffer, in_size, &buffer);
	if (ec != 0)
	{
		free(buffer.text);
		return ec;
	}

	/* An empty program has no text, but the caller still frees something. */
	if (buffer.text == NULL)
	{
		buffer.text = malloc(1);
		if (buffer.text == NULL)
		{
			return EOS_OM;
		}
	}

	*out_buffer = buffer.text;
	*out_size = (u_int)buffer.size;
	return 0;
}

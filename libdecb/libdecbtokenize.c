/********************************************************************
 * tokenize.c - Color BASIC tokeniziation routines.
 *
 * $Id$
 ********************************************************************/

#define BLOCK_QUANTUM  256

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "decbpath.h"

/* CoCo function tokens */
const char *functions[128] =
	{ "SGN", "INT", "ABS", "USR", "RND", "SIN", "PEEK",
	"LEN", "STR$", "VAL", "ASC", "CHR$", "EOF", "JOYSTK",
	"LEFT$", "RIGHT$", "MID$", "POINT", "INKEY$", "MEM",
	"ATN", "COS", "TAN", "EXP", "FIX", "LOG", "POS", "SQR",
	"HEX$", "VARPTR", "INSTR", "TIMER", "PPOINT", "STRING$",
	"CVN", "FREE", "LOC", "LOF", "MKN$", "AS", "", "LPEEK",
	"BUTTON", "HPOINT", "ERNO", "ERLIN", NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/* Dragon Function tokens */
const char *d_functions[128] =
	{ "SGN", "INT", "ABS", "POS", "RND", "SQR", "LOG",
	"EXP", "SIN", "COS", "TAN", "ATN", "PEEK", "LEN",
	"STR$", "VAL", "ASC", "CHR$", "EOF", "JOYSTK",
	"FIX", "HEX$", "LEFT$", "RIGHT$", "MID$", "POINT", "INKEY$", "MEM",
	"VARPTR", "INSTR", "TIMER", "PPOINT", "STRING$", "USR", "LOF",
	"FREE", "ERL", "ERR", "HIMEM", "LOC", "FRE$", NULL
};

/* CoCo command tokens */
const char *commands[128] = { "FOR", "GO", "REM", "'", "ELSE", "IF",
	"DATA", "PRINT", "ON", "INPUT", "END", "NEXT",
	"DIM", "READ", "RUN", "RESTORE", "RETURN", "STOP",
	"POKE", "CONT", "LIST", "CLEAR", "NEW", "CLOAD",
	"CSAVE", "OPEN", "CLOSE", "LLIST", "SET", "RESET",
	"CLS", "MOTOR", "SOUND", "AUDIO", "EXEC", "SKIPF",
	"TAB(", "TO", "SUB", "THEN", "NOT", "STEP",
	"OFF", "+", "-", "*", "/", "^",
	"AND", "OR", ">", "=", "<", "DEL",
	"EDIT", "TRON", "TROFF", "DEF", "LET", "LINE", "PCLS",
	"PSET", "PRESET", "SCREEN", "PCLEAR", "COLOR", "CIRCLE",
	"PAINT", "GET", "PUT", "DRAW", "PCOPY", "PMODE",
	"PLAY", "DLOAD", "RENUM", "FN", "USING", "DIR",
	"DRIVE", "FIELD", "FILES", "KILL", "LOAD", "LSET",
	"MERGE", "RENAME", "RSET", "SAVE", "WRITE", "VERIFY",
	"UNLOAD", "DSKINI", "BACKUP", "COPY", "DSKI$", "DSKO$",
	"DOS", "WIDTH", "PALETTE", "HSCREEN", "LPOKE", "HCLS",
	"HCOLOR", "HPAINT", "HCIRCLE", "HLINE", "HGET", "HPUT",
	"HBUFF", "HPRINT", "ERR", "BRK", "LOCATE", "HSTAT",
	"HSET", "HRESET", "HDRAW", "CMP", "RGB", "ATTR",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/* Dragon command tokens */
const char *d_commands[128] =
	{ "FOR", "GO", "REM", "'", "ELSE", "IF", "DATA", "PRINT",
	"ON", "INPUT", "END", "NEXT", "DIM", "READ", "LET", "RUN",
	"RESTORE", "RETURN", "STOP", "POKE", "CONT", "LIST", "CLEAR",
	"NEW", "DEF", "CLOAD", "CSAVE", "OPEN", "CLOSE", "LLIST",
	"SET", "RESET", "CLS", "MOTOR", "SOUND", "AUDIO", "EXEC",
	"SKIPF", "DEL", "EDIT", "TRON", "TROFF", "LINE", "PCLS", "PSET",
	"PRESET", "SCREEN", "PCLEAR", "COLOR", "CIRCLE", "PAINT",
	"GET", "PUT", "DRAW", "PCOPY", "PMODE", "PLAY", "DLOAD", "RENUM",
	"TAB(", "TO", "SUB", "FN", "THEN", "NOT", "STEP", "OFF", "+",
	"-", "*", "/", "^", "AND", "OR", ">", "=", "<", "USING", "AUTO",
	"BACKUP", "BEEP", "BOOT", "CHAIN", "COPY", "CREATE", "DIR",
	"DRIVE", "DSKINIT", "FREAD", "FWRITE", "ERROR", "KILL", "LOAD",
	"MERGE", "PROTECT", "WAIT", "RENAME", "SAVE", "SREAD", "SWRITE",
	"VERIFY", "FROM", "FLREAD", "SWAP", NULL
};

//size_t malloc_size(void *ptr);
int tok_strncmp(const char *str1, const char *str2, size_t n);

/* _decb_detoken()

   This subroutine will de-token a binary BASIC program in in_buffer of size in_size.
   The resulting textual data will be in out_buffer and have a size of out_size

   The caller is responsible for free()ing out_buffer
*/

error_code _decb_detoken(unsigned char *in_buffer, int in_size,
			 char **out_buffer, u_int * out_size)
{
	u_int in_pos = 0, out_pos = 0;
	int file_size, value, line_number;
	unsigned char character;
	error_code ec;
	size_t buffer_size;

	*out_size = 0;

	if (*in_buffer == 0xff)
	{
		in_pos = 1;

		file_size = in_buffer[in_pos++] << 8;
		file_size += in_buffer[in_pos++];

		if (file_size > (in_size - 3))
		{
			/* Error adjusted internal BASIC file size does not match buffer size */
			return EOS_SN;
		}

		file_size = (in_size - 3);
	}

	*out_buffer = malloc(BLOCK_QUANTUM);
	buffer_size = BLOCK_QUANTUM;

	if (*out_buffer == NULL)
	{
		/* Memory Error */
		return EOS_OM;
	}

	/* Value will be where the next line starts in the CoCo's memory map */
	value = in_buffer[in_pos++] << 8;
	value += in_buffer[in_pos++];

	while (value != 0)
	{
		/* Evaluate line number */
		line_number = in_buffer[in_pos++] << 8;
		line_number += in_buffer[in_pos++];

		if ((ec =
		     _decb_buffer_sprintf(&out_pos, out_buffer, &buffer_size,
					  "%d ", line_number)) != 0)
			return ec;

		while ((character = in_buffer[in_pos++]) != 0)
		{
			if (character == 0xff)
			{
				/* A Function call */
				character = in_buffer[in_pos++];

				if (functions[character - 0x80] != NULL)
				{
					if ((ec =
					     _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size,
								  "%s",
								  functions
								  [character -
								   0x80])) !=
					    0)
						return ec;
				}
				else
				{
					if ((ec =
					     _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size,
								  "!")) != 0)
						return ec;
				}
			}
			else if (character >= 0x80)
			{
				/* A Command call */
				if (commands[character - 0x80] != NULL)
				{
					if ((ec =
					     _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size,
								  "%s",
								  commands
								  [character -
								   0x80])) !=
					    0)
						return ec;
				}
				else
				{
					if ((ec =
					     _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size,
								  "!")) != 0)
						return ec;
				}
			}
			else if (character == ':'
				 && (in_buffer[in_pos] == 0x83
				     || in_buffer[in_pos] == 0x84))
			{
				/* When colon-apostrophe is encountered, the colon is dropped. */
				/* When colon-ELSE is encountered, the colon is dropped. */
			}
			else
			{
				if ((ec =
				     _decb_buffer_sprintf(&out_pos,
							  out_buffer,
							  &buffer_size, "%c",
							  character)) != 0)
					return ec;
			}
		}

		value = in_buffer[in_pos++] << 8;
		value += in_buffer[in_pos++];

		if ((ec =
		     _decb_buffer_sprintf(&out_pos, out_buffer, &buffer_size,
					  "\n")) != 0)
			return ec;
	}

	*out_size = out_pos;

	return 0;
}

/* _decb_entoken()

   This subroutine will en-token a textual BASIC program in in_buffer of size in_size.
   The resulting binary data will be in out_buffer and have a size of out_size

   The caller is responsible for free()ing out_buffer
*/

error_code _decb_entoken(unsigned char *in_buffer, int in_size,
			 unsigned char **out_buffer, u_int * out_size,
			 int path_type)
{
	int in_pos = 0, out_pos = 0;

	*out_size = 0;

	/* Preprocessing input to remove illegal characters */
	for (int i=0; i<in_size; i++)
	{
		in_buffer[i] &= 0x7f;

		if (in_buffer[i] == 0x0d)
			continue;
		if (in_buffer[i] == 0x0a)
			continue;
		if (isprint(in_buffer[i]))
			continue;
		
		in_buffer[i] = ' ';
	}

	/* The tokenized form of the BASIC program should be smaller than the untokenized form,
	   but you never know. */
	*out_buffer = malloc(in_size + 64);

	if (*out_buffer == NULL)
	{
		/* Memory Error */
		return EOS_OM;
	}

	/* Remove trailing new lines from in_buffer */
	while (in_buffer[in_size - 1] == 0x0d
	       || in_buffer[in_size - 1] == 0x0a)
	{
		in_size--;
		if (in_size == 0)
		{
			/* Empty File */
			return EOS_SN;
		}
	}

	if (path_type)
	{
		/* Add DECB Header */
		(*out_buffer)[out_pos++] = 0xff;	/* flag */
		(*out_buffer)[out_pos++] = 0;	/* File size */
		(*out_buffer)[out_pos++] = 0;
	}

	while (in_pos < in_size)
	{
		int line_number, next_line_pointer;
		int data_literal, quote_literal, rem_literal, var_literal;

		next_line_pointer = out_pos;
		(*out_buffer)[out_pos++] = 0x00;	/* Reserve two bytes for BASIC's next-line-pointer */
		(*out_buffer)[out_pos++] = 0x00;

		while (in_pos < in_size && isspace(in_buffer[in_pos]))
			in_pos++;	/* Spin past pre-line-number spaces */

		/* Encode line number */

		line_number = 0;
		while (in_pos < in_size && isdigit(in_buffer[in_pos]))
			line_number =
				line_number * 10 + (in_buffer[in_pos++] -
						    '0');

		if (line_number > 63999)
		{
			/* Error - line number to big */
			return EOS_SN;
		}

		(*out_buffer)[out_pos++] = line_number >> 8;
		(*out_buffer)[out_pos++] = line_number & 0x00ff;

		while (in_pos < in_size && isspace(in_buffer[in_pos]))
			in_pos++;	/* Spin past any post-line-number spaces */

		/* All literal flags get reset on a new line */
		data_literal = quote_literal = rem_literal = var_literal = 0;

		/* entoken line */
		while (in_pos < in_size
		       && !(in_buffer[in_pos] == 0x0d
			    || in_buffer[in_pos] == 0x0a))
		{
			int i;

			i = 0x80;

			/* Skip tokenization if we are in a literal state. */
			if (quote_literal + data_literal + rem_literal +
			    var_literal == 0)
			{
				/* Check for PRINT abbreviation */
				if (in_buffer[in_pos] == '?')
				{
					(*out_buffer)[out_pos++] = 0x87;	/* PRINT token */
					in_pos++;
					i = 0x87;
				}
				else
				{
					/* Tokenize a command */
					for (i = 0; i < 0x80; i++)
					{
						if (tok_strncmp
						    (commands[i],
						     (char *)
						     &(in_buffer[in_pos]), in_size - in_pos) ==
						    0)
						{
							if (i == 3)	/* Preface ' with a colon */
								(*out_buffer)
									[out_pos++]
									= ':';

							if (i == 4)	/* Preface ELSE with a colon */
								(*out_buffer)
									[out_pos++]
									= ':';

							(*out_buffer)
								[out_pos++] =
								i + 0x80;
							in_pos +=
								strlen
								(commands[i]);

							if (i == 6)
								data_literal =
									1;

							if (i == 2 || i == 3)
								rem_literal =
									1;

							break;
						}
					}

					if (i == 0x80)
					{
						/* Tokenize a function */
						for (i = 0; i < 0x80; i++)
						{
							if (tok_strncmp
							    (functions[i],
							     (char *)
							     &(in_buffer
							       [in_pos]), in_size - in_pos) ==
							    0)
							{
								(*out_buffer)[out_pos++] = 0xff;	/* Function marker */
								(*out_buffer)
									[out_pos++]
									=
									i +
									0x80;
								in_pos +=
									strlen
									(functions
									 [i]);
								break;
							}
						}
					}
				}
			}

			if (i == 0x80 && in_pos < in_size)
			{
				/* Detect any 'end of literal' tranisitions */
				if (in_buffer[in_pos] == '"')
				{
					if (quote_literal == 0)
						quote_literal = 1;
					else
						quote_literal = 0;
				}
				else if (in_buffer[in_pos] == ':'
					 && quote_literal == 0
					 && data_literal == 1)
					data_literal = 0;

				if (quote_literal + data_literal +
				    rem_literal == 0)
				{
					if (isalpha(in_buffer[in_pos]))
						/* If no tokens were found, and character is an letter, then we are in a variable literal */
						var_literal = 1;
				}

				(*out_buffer)[out_pos++] =
					in_buffer[in_pos++];

				if (in_pos >= in_size || !isalnum(in_buffer[in_pos]))
					var_literal = 0;
			}
		}

		if (in_pos < in_size && in_buffer[in_pos] == 0x0a)
			in_pos++;	/* skip past DOS line feeds (0d 0a) */

		/* Go back and fix up BASIC's 'next line' pointer */
		(*out_buffer)[next_line_pointer] = (0x25FF + out_pos) >> 8;
		(*out_buffer)[next_line_pointer + 1] =
			(0x25FF + out_pos) & 0x00ff;

		(*out_buffer)[out_pos++] = 0x00;	/* Every line ends with a zero */
	}

	(*out_buffer)[out_pos++] = 0x00;	/* BASIC file ends with two bytes of zeros */
	(*out_buffer)[out_pos++] = 0x00;

	if (path_type)
	{
		/* Update file size in Disk BASIC's header */

		(*out_buffer)[1] = (out_pos - 3) >> 8;
		(*out_buffer)[2] = (out_pos - 3) & 0x00ff;
	}

	*out_size = out_pos;

	return 0;
}

/* This function will determine is the file in in_buffer is a tokenized
   BASIC file or not.

   returns -1 if not.
   returns 0 if it is.
*/

error_code _decb_detect_tokenized(unsigned char *in_buffer, u_int in_size)
{
	int file_size;

	if (in_size < 3)
	{
		/* File to small to be a tokenized BASIC file */
		return EOS_SN;
	}

	if (in_buffer[0] != 0xff)
	{
		/* Error tokennized BASIC file needs to start wit 0xFF */
		return EOS_SN;
	}

	file_size = in_buffer[1] << 8;
	file_size += in_buffer[2];

	if (file_size > (in_size - 3))
	{
		/* Error adjusted internal BASIC file size does not match buffer size */
		return EOS_SN;
	}

	return 0;
}

/* This sprintf will use realloc to make the buffer larger if needed */
error_code _decb_buffer_sprintf(u_int * position, char **str,
				size_t *buffer_size, const char *format, ...)
{
	va_list ap;

	if (*position > ((*buffer_size) - 20))
	{
		char *buffer;

		buffer = realloc(*str, (*buffer_size) + BLOCK_QUANTUM);

		if (buffer == NULL)
		{
			/* error */
			return EOS_OM;
		}

		*buffer_size = *buffer_size + BLOCK_QUANTUM;

		if (*str != buffer)
		{
			*str = buffer;
		}
	}

	va_start(ap, format);
	*position += vsprintf((*str) + *position, format, ap);
	va_end(ap);

	return 0;
}

/* Returns 0 if the buffer in str2 of size n begins with the NULL-terminated
   string in str1, else returns -1. */
int tok_strncmp(const char *str1, const char *str2, size_t n) {
	int i = 0;

	if (str1 == NULL || strlen(str1) > n)
		return -1;

	if (str1[0] == 0x00)
		return -1;

	while (str1[i] != '\0')
	{
		if (str1[i] != str2[i])
			return -1;

		i++;
	}

	return 0;
}

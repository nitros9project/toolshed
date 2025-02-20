/********************************************************************
 * tokenize.c - Micro BASIC tokeniziation routines.
 *
 * $Id$
 ********************************************************************/

#define BLOCK_QUANTUM  256

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "cecbpath.h"

const char *tokens[128] = {
	"FOR", "GOTO", "GOSUB", "REM", "IF", "DATA", "PRINT", "ON", "INPUT", "END", "NEXT",
	"DIM", "READ", "LET", "RUN", "RESTORE", "RETURN", "STOP", "POKE", "CONT", "LIST",
	"CLEAR", "NEW", "CLOAD", "CSAVE", "LLIST", "LPRINT", "SET", "RESET", "CLS", "SOUND",
	"EXEC", "SKIPF",  "TAB(", "TO", "THEN", NULL, "STEP", "OFF", "+", "-", "*", "/",
	"^", "AND", "OR", ">", "=", "<", "SGN", "INT", "ABS", NULL, "RND", "SQR", "LOG",
	"EXP", "SIN", "COS", "TAN", "PEEK", "LEN", "STR$", "VAL", "ASC", "CHR$", "LEFT$",
	"RIGHT$", "MID$", "POINT", "VARPTR", "INKEY$", "MEM", "ELSE", "PCLS", "PSET", "PRESET",
	"LINE", "CIRCLE", "PAINT", "DRAW", "PMODE", "SCREEN", "COLOR", "PCOPY", "PLAY", "OPEN",
	"CLOSE", "FILES", "CHAIN", "SWAP", "WAIT", "ERROR", "'", "DEF", "LOAD", "SAVE", "MERGE",
	"DIR", "EDIT", "RENUM", "AUTO", "DEL", "TRON", "TROFF", "BREAK", "HEX$", "EOF", "FIX",
	"POS", "STRMEM", "ATN", "PPOINT", "STRING$", "INSTR", "MINVAL", "MAXVAL", "TIMER",
	"ERRL", "ERRN", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

int tok_strncmp(const char *str1, const char *str2, size_t n);
int ahex2int(char a, char b);

error_code _cecb_detoken(unsigned char *in_buffer, int in_size,
			 char **out_buffer, u_int * out_size)
{
	u_int in_pos = 0, out_pos = 0;
	int value, mode_character_string, mode_data_statement;
	size_t buffer_size;
	error_code ec;

	*out_size = 0;
	mode_character_string = 0;
	mode_data_statement = 0;

	*out_buffer = malloc(BLOCK_QUANTUM);
	buffer_size = BLOCK_QUANTUM;

	value = in_buffer[in_pos++] << 8;
	value += in_buffer[in_pos++];

	while (value != 0 && in_pos < in_size )
	{
		unsigned int line_number;
		unsigned char character;

		line_number = in_buffer[in_pos++] << 8;
		line_number += in_buffer[in_pos++];

		if ((ec = _decb_buffer_sprintf(&out_pos, out_buffer, &buffer_size, "%u ", line_number)) != 0) return ec;

		while ((character = in_buffer[in_pos++]) != 0)
		{
			if (mode_character_string != 0 || mode_data_statement != 0)
			{
				if (isprint(character))
				{
					if ((ec =
						 _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size, "%c",
								  character)) != 0)
						return ec;
				}
				else
				{
					/* escape non printable characters */
					if ((ec =
						 _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size, "\\x%02x", character )) != 0)
						return ec;
				}

				if (character == '"')
				{
					mode_character_string = 0;
				}
			}
			else
			{
				if( character < 0x80 )
				{
					if (character == ':')
					{
						if( in_buffer[in_pos] == 0x80 + 73) continue; /* ELSE */
						if( in_buffer[in_pos] == 0x80 + 93) continue; /* ' */
					}

					if (isprint(character))
					{
						if ((ec =
							 _decb_buffer_sprintf(&out_pos,
									  out_buffer,
									  &buffer_size, "%c",
									  character)) != 0)
							return ec;
					}
					else
					{
						/* escape non printable characters */
						if ((ec =
							 _decb_buffer_sprintf(&out_pos,
									  out_buffer,
									  &buffer_size, "\\x%02x", character )) != 0)
							return ec;
					}
				}
				else
				{
					if (tokens[character - 0x80] == NULL)
					{
						/* escape non printable token */
						if ((ec =
							 _decb_buffer_sprintf(&out_pos,
									  out_buffer,
									  &buffer_size, "\\x%02x", character )) != 0)
							return ec;
					}
					else
					{
						if ((ec =
							 _decb_buffer_sprintf(&out_pos,
									  out_buffer,
									  &buffer_size,
									  "%s",
									  tokens
									  [character -
									   0x80])) !=
							0)
							return ec;
					}
				}

				if (character == '\\')
				{
					/* escape backslash */
					if ((ec =
						 _decb_buffer_sprintf(&out_pos,
								  out_buffer,
								  &buffer_size, "\\" )) != 0)
						return ec;
				}

				if (character == '"')
				{
					mode_character_string = !mode_character_string;
				}

				if( character == 0x80 + 5)
				{
					mode_data_statement = !mode_data_statement;
				}
			}
		}

		if ((ec =
		     _decb_buffer_sprintf(&out_pos, out_buffer, &buffer_size,
					  "\n")) != 0)
			return ec;

		mode_character_string = 0;
		mode_data_statement = 0;

		value = in_buffer[in_pos++] << 8;
		value += in_buffer[in_pos++];
	}

	*out_size = out_pos;
	return 0;
}

error_code _cecb_entoken(unsigned char *in_buffer, int in_size,
			 unsigned char **out_buffer, u_int * out_size,
			 int path_type)
{
	int in_pos = 0, out_pos = 0;

	*out_size = 0;

	/* The tokenized form of the BASIC program should be smaller than the untokenized form,
	   but you never know. */
	*out_buffer = malloc(in_size + 64);

	if (*out_buffer == NULL)
	{
		/* Memory Error */
		return EOS_OM;
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

		/* Enocde line number */

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

			/* look for a backslash */
			if (in_buffer[in_pos] == '\\' && (in_pos + 1) < in_size)
			{
				/* look for literal */
				if (in_buffer[in_pos+1] == '\\')
				{
					/* output a single backslash */
					(*out_buffer)[out_pos++] = '\\';
					in_pos += 1;
					continue;
				}
				/* look for hex escape */
				else if (in_buffer[in_pos+1] == 'x' && (in_pos + 3) < in_size)
				{
					if (isxdigit(in_buffer[in_pos+2]))
					{
						if (isxdigit(in_buffer[in_pos+3]))
						{
							/* output single character */
							(*out_buffer)[out_pos++] = ahex2int(in_buffer[in_pos+2],in_buffer[in_pos+3]);
							in_pos += 4;
							continue;
						}
					}
				}
			}

			i = 0x80;

			/* Skip tokenization if we are in a literal state. */
			if (quote_literal + data_literal + rem_literal +
			    var_literal == 0)
			{
				/* Check for PRINT abbreviation */
				if (in_buffer[in_pos] == '?')
				{
					(*out_buffer)[out_pos++] = 0x86;	/* PRINT token */
					in_pos++;
					i = 0x86;
				}
				else
				{
					/* Tokenize a command */
					for (i = 0; i < 0x80; i++)
					{
						if (tok_strncmp
						    (tokens[i],
						     (char *)
						     &(in_buffer[in_pos]), in_size - in_pos) ==
						    0)
						{
							if (i == 93)	/* Preface ' with a colon */
								(*out_buffer)
									[out_pos++]
									= ':';

							if (i == 73)	/* Preface ELSE with a colon */
								(*out_buffer)
									[out_pos++]
									= ':';

							(*out_buffer)
								[out_pos++] =
								i + 0x80;
							in_pos +=
								strlen
								(tokens[i]);

							if (i == 5)
								data_literal =
									1;

							if (i == 3 || i == 73)
								rem_literal =
									1;

							break;
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

	*out_size = out_pos;

	return 0;
}

int ahex2int(char a, char b)
{
    a = (a <= '9') ? a - '0' : (a & 0x7) + 9;
    b = (b <= '9') ? b - '0' : (b & 0x7) + 9;

    return (a << 4) + b;
}

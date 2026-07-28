/********************************************************************
 * os9util.h - os9 utility header file
 *
 * $Id$
 ********************************************************************/

#ifndef _OS9UTIL_H
#define _OS9UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <cocotypes.h>


/* Function prototypes for supported os9 commands are here */

int os9attr(int, char **);
int os9cmp(int, char **);
int os9copy(int, char **);
int os9dcheck(int, char **);
int os9del(int, char **);
int os9deldir(int, char **);
int os9dir(int, char **);
int os9dsave(int, char **);
int os9dump(int, char **);
int os9format(int, char **);
int os9free(int, char **);
int os9fstat(int, char **);
int os9gen(int, char **);
int os9id(int, char **);
int os9ident(int, char **);
int os9list(int, char **);
int os9makdir(int, char **);
int os9modbust(int, char **);
int os9padrom(int, char **);
int os9rename(int, char **);

int StrToInt(char *s);
void show_help(char const * const *helpMessage);

struct personality
{
	int startlsn;
};

error_code get_boottrack_lsn(lsn0_sect LSN0, struct personality *hwtype, int *startlsn, int verbose);

#ifdef __cplusplus
}
#endif

#endif  /* _OS9UTIL_H */

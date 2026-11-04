/********************************************************************
 * decbutil.h - DECB utility header file
 *
 * $Id$
 ********************************************************************/

#ifndef _DECBUTIL_H
#define _DECBUTIL_H

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


/* Function prototypes for supported DECB commands are here */

int decbattr(int, char **);
int decbcmp(int, char **);
int decbcopy(int, char **);
int decbdcheck(int, char **);
int decbdel(int, char **);
int decbdeldir(int, char **);
int decbdir(int, char **);
int decbdsave(int, char **);
int decbdump(int, char **);
int decbformat(int, char **);
int decbfree(int, char **);
int decbfstat(int, char **);
int decbgen(int, char **);
int decbid(int, char **);
int decbident(int, char **);
int decblist(int, char **);
int decbmakdir(int, char **);
int decbmodbust(int, char **);
int decbpadrom(int, char **);
int decbrename(int, char **);
int decbreveal(int, char **);
int decbdump(int, char**);
int decbdsave(int, char**);
int decbbinbust(int, char **);

#ifdef __cplusplus
}
#endif

#endif	/* _DECBUTIL_H */

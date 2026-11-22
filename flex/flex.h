/********************************************************************
 * flex.h - FLEX Tools Executive internals
 ********************************************************************/
#ifndef	_FLEX_H
#define	_FLEX_H

#include <stddef.h>

/* The exit status when a command's message has already said what is wrong. */
#define FLEX_USAGE_ERROR 1

int flexattr(int, char **);
int flexcopy(int, char **);
int flexdir(int, char **);
int flexdsave(int, char **);
int flexdskinit(int, char **);
int flexfree(int, char **);
int flexfstat(int, char **);
int flexgen(int, char **);
int flexkill(int, char **);
int flexlist(int, char **);
int flexrename(int, char **);

/* Make the pathlist of a whole disk, which is <disk> with a comma if it
   doesn't have one.  The result is EOS_BPNAM if it doesn't fit. */
int flex_disk_pathlist(const char *disk, char *pathlist, size_t size);

/* Print a nul filled field, escaping what can't be printed.  Nuls are
   printed as spaces when pad is set, or not at all. */
void flex_print_field(const unsigned char *field, int length, int pad);

#endif	/* _FLEX_H */

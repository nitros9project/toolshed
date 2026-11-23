/********************************************************************
 * cflex.h - FLEX Tools Executive internals
 ********************************************************************/
#ifndef	_CFLEX_H
#define	_CFLEX_H

#include <stddef.h>

/* The exit status when a command's message has already said what is wrong. */
#define CFLEX_USAGE_ERROR 1

int cflexattr(int, char **);
int cflexcopy(int, char **);
int cflexdir(int, char **);
int cflexdsave(int, char **);
int cflexdskinit(int, char **);
int cflexfree(int, char **);
int cflexfstat(int, char **);
int cflexgen(int, char **);
int cflexkill(int, char **);
int cflexlist(int, char **);
int cflexrename(int, char **);

/* Make the pathlist of a whole disk, which is <disk> with a comma if it
   doesn't have one.  The result is EOS_BPNAM if it doesn't fit. */
int cflex_disk_pathlist(const char *disk, char *pathlist, size_t size);

/* Print a nul filled field, escaping what can't be printed.  Nuls are
   printed as spaces when pad is set, or not at all. */
void cflex_print_field(const unsigned char *field, int length, int pad);

#endif	/* _CFLEX_H */

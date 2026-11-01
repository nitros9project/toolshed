/********************************************************************
 * dgndos.h - Dragon DOS Tools Executive internals
 ********************************************************************/
#ifndef	_DGNDOS_H
#define	_DGNDOS_H

#include <stddef.h>

/* The exit status when a command's message has already said what is wrong. */
#define DGNDOS_USAGE_ERROR 1

int dgndosattr(int, char **);
int dgndoscopy(int, char **);
int dgndosdir(int, char **);
int dgndosdskinit(int, char **);
int dgndosfree(int, char **);
int dgndosfstat(int, char **);
int dgndoskill(int, char **);
int dgndoslist(int, char **);
int dgndosrename(int, char **);

/* Make the pathlist of a whole disk, which is <disk> with a comma if it
   doesn't have one.  The result is EOS_BPNAM if it doesn't fit. */
int dgndos_disk_pathlist(const char *disk, char *pathlist, size_t size);

#endif	/* _DGNDOS_H */

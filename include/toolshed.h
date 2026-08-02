/********************************************************************
 * toolshed.h - ToolShed global header
 *
 * $Id$
 ********************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
#include <cocopath.h>

#define TS_MAXSTR	128


/* ERROR CODES */
#define EOS_PADROM		258


const char *TSReportError(error_code te);

error_code TSPadROM(char *pathlist, int padSize, char padChar, int padAtStart);
int TSDirectory(char *pathlist);
int TSIsDirectory(char *pathlist);
error_code TSRename(char *pathlist, char *new_name);
error_code TSDelete(char *pathlist);
error_code TSRBFAttrGet(char *p, char *attr, char *strattr);
error_code TSRBFAttrSet(char *file, int attrSetMask, int attrResetMask, char *attr, char *strattr);
error_code TSMoveFile(char *srcfile, char *dstfile);
error_code TSCopyFile(char *srcfile, char *dstfile, int eolTranslate, int rewrite, int owner, int owner_set, char *buffer, u_int buffer_size);
void NativeToCoCo(char *buffer, u_int size, char **newBuffer, u_int *newSize);
void CoCoToNative(char *buffer, u_int size, char **newBuffer, u_int *newSize);
EOL_Type DetermineEOLType(char *buffer, int size);
int TSMakeDirectory(char *p);
error_code TSRBFFree(char *file, char *dname, u_int *month, u_int *day, u_int *year, u_int *bps, u_int *total_sectors, u_int *bytes_free, u_int *free_sectors, u_int *largest_free_block, u_int *sectors_per_cluster, u_int *largest_count, u_int *sector_count);
error_code TSDECBFree(char *file, u_int *free_granules);

#ifdef __cplusplus
}
#endif

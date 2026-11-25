/********************************************************************
 * toolshed_ql.h - What the Quick Look extension calls in ToolShed
 *
 * The listings are the ones the command line tools print, so a
 * preview shows what "os9 dir", "decb dir" and the rest would.
 ********************************************************************/
#ifndef TOOLSHED_QL_H
#define TOOLSHED_QL_H

#ifdef __cplusplus
extern "C" {
#endif

/* The filesystems _coco_detect_filesystems() reports. */
#define TSQL_FS_OS9		0x01
#define TSQL_FS_DECB		0x02
#define TSQL_FS_DRAGONDOS	0x04
#define TSQL_FS_FLEX		0x08

typedef enum
{
	TSQL_INFO,		/* os9 info: the image format and track layout */
	TSQL_DIR_OS9,		/* os9 dir -e -r */
	TSQL_DIR_DECB,		/* decb dir */
	TSQL_DIR_DRAGONDOS,	/* dgndos dir */
	TSQL_DIR_FLEX		/* cflex dir */
} tsql_command;

/* Report which filesystems the image at path could hold.  Returns 0 if path
   is a disk image, even one with no filesystem found, and not 0 if it isn't
   one or can't be read. */
int tsql_detect(const char *path, unsigned int *filesystems);

/* Run a command on the image at path and return what it printed, which the
   caller frees.  What it printed as errors is returned in *errors, also to be
   freed, or NULL if there were none.  *status is the command's result.
   Returns NULL only if the output could not be captured. */
char *tsql_run(tsql_command command, const char *path, char **errors, int *status);

#ifdef __cplusplus
}
#endif

#endif

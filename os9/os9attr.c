/********************************************************************
 * os9attr.c - File attribute utility for OS-9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cocotypes.h>
#include <os9path.h>
#include <toolshed.h>


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: attr {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display or modify file attributes.\n",
	"Options:\n",
	"     -q        quiet mode (suppress output)\n",
	"     -e        set execute permission\n",
	"     -w        set write permission\n",
	"     -r        set read permission\n",
	"     -s        set single user permission\n",
	"     -pe, -pw, -pr        set public execute, write or read permission\n",
	"     -ne, -nw, -nr, -ns   unset execute, write, read or single user\n",
	"     -npe, -npw, -npr     unset public execute, write or read\n",
	"   Options combine, as in -ew -pr, or -ewpr.\n",
	NULL
};


/* The permission bit for the letter after -n, or 0 if there is none.
   -nd clears the directory bit, as copy -a=nd does. */
static int owner_permission(char letter)
{
	switch (letter)
	{
	case 'e':
		return FAP_EXEC;
	case 'w':
		return FAP_WRITE;
	case 'r':
		return FAP_READ;
	case 's':
		return FAP_SINGLE;
	case 'd':
		return FAP_DIR;
	}

	return 0;
}


/* The public permission bit for the letter after -p or -np, or 0. */
static int public_permission(char letter)
{
	switch (letter)
	{
	case 'e':
		return FAP_PEXEC;
	case 'w':
		return FAP_PWRITE;
	case 'r':
		return FAP_PREAD;
	}

	return 0;
}


int os9attr(int argc, char *argv[])
{
	char *p = NULL;
	int i;
	int attrSetMask = 0;
	int attrResetMask = 0;
	int quiet = 0;
	error_code first_error = 0;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'q':
					quiet = 1;
					break;
				case 'e':
					attrSetMask |= FAP_EXEC;
					break;
				case 'w':
					attrSetMask |= FAP_WRITE;
					break;
				case 'r':
					attrSetMask |= FAP_READ;
					break;
				case 'n':
				{
					/* -n<x> clears one owner permission, -np<x> one
					   public permission.  The letters after -n are
					   used here, so the loop doesn't read them again. */
					int mask = *(p + 1) == 'p' ?
						public_permission(*(p + 2)) :
						owner_permission(*(p + 1));

					if (mask == 0)
					{
						fprintf(stderr,
							"%s: unknown option '%.*s'\n",
							argv[0], *(p + 1) == 'p' ? 3 : 2, p);
						return (EOS_IA);
					}
					attrResetMask |= mask;
					p += *(p + 1) == 'p' ? 2 : 1;
					break;
				}
				case 'p':
				{
					int mask = public_permission(*(p + 1));

					if (mask == 0)
					{
						fprintf(stderr,
							"%s: unknown option '%.2s'\n",
							argv[0], p);
						return (EOS_IA);
					}
					attrSetMask |= mask;
					p++;
					break;
				}
				case 's':
					attrSetMask |= FAP_SINGLE;
					break;
				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);
				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (EOS_IA);
				}
			}
		}
	}

	/* walk command line for pathnames */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}
		else
		{
			p = argv[i];

			if (attrSetMask == 0 && attrResetMask == 0)
			{
				error_code ec;
				char attrs[9], attr;

				/* No attribute options were specified */
				if ((ec = TSRBFAttrGet(p, &attr, attrs)) != 0)
				{
					fprintf(stderr,
						"%s: error %d opening '%s': %s\n",
						argv[0], ec, p, TSReportError(ec));
					/* Keep failures visible even if a later file succeeds. */
					if (first_error == 0)
						first_error = ec;
				}
				else
				{
					printf("%s\n", attrs);
				}
			}
			else
			{
				error_code ec;
				char attr, attrs[9];

				/* Attributes were specified */
				if ((ec =
				     TSRBFAttrSet(p, attrSetMask,
						  attrResetMask, &attr,
						  attrs)) != 0)
				{
					fprintf(stderr,
						"%s: error %d opening '%s': %s\n",
						argv[0], ec, p, TSReportError(ec));
					if (first_error == 0)
						first_error = ec;
				}
				else if (quiet == 0)
				{
					printf("%s\n", attrs);
				}
			}
			if (quiet == 0)
			{
				printf("\n");
			}
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (first_error);
}

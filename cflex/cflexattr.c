/********************************************************************
 * cflexattr.c - File attribute utility for FLEX
 ********************************************************************/
#include <util.h>
#include <flexpath.h>

#include "cflex.h"


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: attr {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display or change the protection of files, as PROT does.\n",
	"Options:\n",
	"     -w        write protect\n",
	"     -d        delete protect\n",
	"     -r        read protect\n",
	"     -c        catalog protect\n",
	"     -x        remove all protection, before adding any of the above\n",
	NULL
};


static error_code do_attr(char *file, int clear, u_int set)
{
	int change = clear || set != 0;
	flex_path_id path;
	flex_file_stat stat;
	error_code ec = _flex_open(&path, file, change ? FAM_READ | FAM_WRITE : FAM_READ);
	error_code close_ec;

	if (ec != 0)
	{
		return ec;
	}

	ec = _flex_gs_fd(path, &stat);
	if (ec == 0 && change)
	{
		if (clear)
		{
			stat.attributes &= ~(FLEX_ATTR_WRITE | FLEX_ATTR_DELETE | FLEX_ATTR_READ | FLEX_ATTR_CATALOG);
		}
		stat.attributes |= set;
		ec = _flex_ss_fd(path, &stat);
	}
	else if (ec == 0)
	{
		printf("%s: %c%c%c%c\n", file,
		       (stat.attributes & FLEX_ATTR_WRITE) ? 'W' : '.',
		       (stat.attributes & FLEX_ATTR_DELETE) ? 'D' : '.',
		       (stat.attributes & FLEX_ATTR_READ) ? 'R' : '.',
		       (stat.attributes & FLEX_ATTR_CATALOG) ? 'C' : '.');
	}

	close_ec = _flex_close(path);
	return ec != 0 ? ec : close_ec;
}


int cflexattr(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;
	int clear = 0;
	u_int set = 0;

	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'w':
					set |= FLEX_ATTR_WRITE;
					break;

				case 'd':
					set |= FLEX_ATTR_DELETE;
					break;

				case 'r':
					set |= FLEX_ATTR_READ;
					break;

				case 'c':
					set |= FLEX_ATTR_CATALOG;
					break;

				case 'x':
					clear = 1;
					break;

				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr, "%s: unknown option '%c'\n", argv[0], *p);
					return (CFLEX_USAGE_ERROR);
				}
			}
		}
	}

	/* walk command line for pathnames */
	p = NULL;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			continue;
		}

		p = argv[i];
		ec = do_attr(p, clear, set);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d with '%s'\n", argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
	}

	return (0);
}

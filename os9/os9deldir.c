/********************************************************************
 * os9deldir.c - Delete file entry for OS-9 filesystems
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cocotypes.h>
#include <cocopath.h>


static int do_deldir(char **argv, char *path, int interaction);


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: deldir {[<opts>]} {<directory>} {[<opts>]}\n",
	"Usage:  Delete a directory and its contents.\n",
	"Options:\n",
	"     -q    quiet mode (suppress interaction)\n",
	NULL
};



int os9deldir(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i, interaction = 0;

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
					interaction = 1;
					break;

				case '?':
				case 'h':
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
		}

		ec = do_deldir(argv, p, interaction);

		if (ec == 1)	/* User quit */
			ec = 0;

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d deleting '%s'\n",
				argv[0], ec, p);
			return (ec);
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}

	return (0);
}


static int do_deldir(char **argv, char *path, int interaction)
{
	char c[10];

	if (interaction == 0)
	{
		do
		{
			printf("\nDeleting directory: %s\n", path);
			printf("List directory, delete directory, or quit? (l/d/q) ");

			if (scanf("%s", c) > 0)
			{
				switch (c[0])
				{
				case 'l':
					{
						char *argv[3];
						argv[0] = "dir";
						argv[1] = path;
						argv[2] = NULL;

						os9dir(2, argv);
					}
					break;

				case 'd':
					break;

				case 'q':
					return 1;

				default:
					c[0] = 'l';	/* Force the user to type l, d, or q */
					break;
				}
			}
		}
		while (c[0] == 'l');
	}

	return _os9_delete_directory(path);
}

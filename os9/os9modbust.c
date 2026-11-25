/********************************************************************
 * os9modbust.c - OS-9 module buster utility
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <cocotypes.h>
#include <cocopath.h>
#include <os9module.h>
#include <cococonv.h>


static int do_modbust(char **argv, char *filename);

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: modbust {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Bust a single merged file of OS-9 modules into separate files.\n",
	"Options:\n",
	NULL
};



int os9modbust(int argc, char **argv)
{
	error_code ec = 0;
	int i;
	char *p = NULL;


	if (argv[1] == NULL)
	{
		show_help(helpMessage);

		return 0;
	}


	/* 1. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '?':
				case 'h':
					show_help(helpMessage);

					return 0;

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);

					return (EOS_IA);
				}
			}
		}
	}


	/* 2. Walk command line for pathnames. */

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

		ec = do_modbust(argv, p);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening file %s\n",
				argv[0], ec, p);

			return ec;
		}
	}


	return 0;
}



int modbust_osk(coco_path_id path)
{
	error_code ec;
	u_int size;
	coco_path_id path2;
	char name[256];
	int nameoffset;
	char buffer[256];
	u_char *module;
	coco_file_stat fstat;

	/* We have an OS-9 module - get 2 bytes then 4 byte size */
	size = 6;

	ec = _coco_read(path, buffer, &size);
	size = int4((u_char *) buffer + 2);
	module = (u_char *) malloc(size);

	if (module == NULL)
	{
		printf("Memory allocation error\n");
		return (1);
	}

	module[0] = OSK_ID0;
	module[1] = OSK_ID1;
	module[2] = buffer[0];
	module[3] = buffer[1];
	module[4] = buffer[2];
	module[5] = buffer[3];
	module[6] = buffer[4];
	module[7] = buffer[5];
	size -= 8;
	ec = _coco_read(path, &module[8], &size);
	nameoffset = int4(&module[12]);
	memcpy(name, &module[nameoffset], OS9Strlen(&module[nameoffset]));
	OS9StringToCString((u_char *) name);
	printf("Busting module %s...\n", name);

	fstat.perms = FAP_READ | FAP_WRITE;
	ec = _coco_create(&path2, name, FAM_WRITE, &fstat);

	if (ec != 0)
	{
		printf("Error creating file %s\n", name);
		return (1);
	}

	size += 8;
	_coco_write(path2, module, &size);
	_coco_close(path2);

	free(module);

	return 0;
}

int modbust_os9(coco_path_id path)
{
	error_code ec;
	u_int size;
	coco_path_id path2;
	char name[256];
	int nameoffset;
	char buffer[256];
	u_char *module;
	coco_file_stat fstat;

	/* We have an OS-9 module - get size */
	size = 2;

	ec = _coco_read(path, buffer, &size);
	size = int2((u_char *) buffer);
	module = (u_char *) malloc(size);

	if (module == NULL)
	{
		printf("Memory allocation error\n");
		return (1);
	}

	module[0] = 0x87;
	module[1] = 0xCD;
	module[2] = buffer[0];
	module[3] = buffer[1];
	size -= 4;
	ec = _coco_read(path, &module[4], &size);
	nameoffset = int2(&module[4]);
	memcpy(name, &module[nameoffset], OS9Strlen(&module[nameoffset]));
	OS9StringToCString((u_char *) name);
	printf("Busting module %s...\n", name);

	fstat.perms = FAP_READ | FAP_WRITE;
	ec = _coco_create(&path2, name, FAM_WRITE, &fstat);

	if (ec != 0)
	{
		printf("Error creating file %s\n", name);
		return (1);
	}

	size += 4;
	_coco_write(path2, module, &size);
	_coco_close(path2);

	free(module);

	return 0;
}

static int do_modbust(char **argv, char *filename)
{
	error_code ec = 0;
	char buffer[256];
	coco_path_id path;

	ec = _coco_open(&path, filename, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}

	while (_coco_gs_eof(path) == 0)
	{
		u_int size = 2;

		ec = _coco_read(path, buffer, &size);
		if (ec != 0)
		{
			fprintf(stderr, "%s: error reading file %s\n",
				argv[0], filename);

			return ec;
		}

		if (buffer[0] == (char) OS9_ID0
		    && buffer[1] == (char) OS9_ID1)
		{
			modbust_os9(path);
		}
		else if (buffer[0] == (char) OSK_ID0
			 && buffer[1] == (char) OSK_ID1)
		{
			modbust_osk(path);
		}
	}

	_coco_close(path);


	return 0;
}

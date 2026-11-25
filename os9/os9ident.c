/********************************************************************
 * os9ident.c - OS-9 ident utility
 *
 * $Id$
 ********************************************************************/
#include <stdio.h>
#include <string.h>

#include "cocotypes.h"
#include "cocopath.h"
#include "os9module.h"
#include "util.h"


static char const *const types[16] = {
	"???", "Prog", "Subr", "Multi", "Data", "USR 5", "USR 6", "USR 7",
	"USR 8", "USR 9", "USR A", "USR B", "System", "File Manager",
	"Device Driver", "Device Descriptor"
};

static char const *const langs[16] = {
	"Data", "6809 Obj", "Basic09 I-Code", "Pascal P-Code", "C I-Code",
	"Cobol I-Code", "Fortran I-Code", "6309 Obj", "???", "???", "???",
	"???", "???", "???", "???", "???"
};

static int shortFlag = 0;
static u_char *buffer;




/* Help message */
static char const *const helpMessage[] = {
	"Syntax: ident {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Display OS-9 module information.\n",
	"Options:\n",
	"     -s    short output\n",
	NULL
};



static u_char *os9_string(u_char * string)
{
	static u_char cleaned[80];	/* strings shouldn't be longer than this */
	u_char *ptr = cleaned;
	int i = 0;

	while (((*(ptr++) = *(string++)) < 0x7f)
	       && (i++ < sizeof(cleaned) - 1));

	*(ptr - 1) &= 0x7f;
	*ptr = '\0';
	return cleaned;
}

static char *modPermission(int perm)
{
	static char tmpBuf[17];
	int i;

	for (i = 0; i < 4; i++)
	{
		sprintf(&tmpBuf[i * 4], "-%c%c%c",
			(perm & 0x4000) ? 'e' : '-',
			(perm & 0x2000) ? 'w' : '-',
			(perm & 0x1000) ? 'r' : '-');
		perm <<= 4;
	}

	return tmpBuf;
}


static void ident_osk(OSK_MODULE_t * mod)
{
	int i;
	char *name;
	u_char *CRC, *buffer = (u_char *) mod;
	int module_size;

	/* gather all information here */
	i = int4(mod->name);
	name = (char *) os9_string(&buffer[i]);
	module_size = int4(mod->size);
	CRC = &buffer[module_size - 3];
	u_char calccrc[3] = {0xff, 0xff, 0xff};
	error_code crcerror = _os9_crc_compute((u_char *) mod, module_size, calccrc);

	printf("Header for :      %s\n", name);
	printf("Module size:      $%-8X     #%d\n", module_size, module_size);
	printf("Owner:            %d.%d\n", mod->owner[0], mod->owner[1]);
	printf("Module CRC :      $%02X%02X%02X       ", CRC[0],
	       CRC[1], CRC[2]);

	if (crcerror != 0 )
	{
		printf( "Good CRC\n" );
	}
	else
	{
		calccrc[0] = 0xff;
		calccrc[1] = 0xff;
		calccrc[2] = 0xff;
		_os9_crc_compute((u_char *) mod, module_size-3, calccrc);
		calccrc[0] ^= 0xff;
		calccrc[1] ^= 0xff;
		calccrc[2] ^= 0xff;

		printf( "Mismatch calculated: $%02X%02X%02X\n",
			calccrc[0], calccrc[1], calccrc[2]);
	}

	printf("Header Parity:    $%04X         Good parity\n",
	       int2(mod->parity));
	printf("Edition:          $%-8X     #%d\n", int2(mod->edit),
	       int2(mod->edit));
	printf("Ty/La At/Rev      $%02X%02X         $%02X%02X\n", mod->type,
	       mod->lang, mod->attr, mod->revs);
	printf("Permission:       $%-4X         %s\n", int2(mod->accs),
	       modPermission(int2(mod->accs)));

	switch (mod->type)
	{
	case Prgrm:
		i = int4(mod->data.program.exec);
		printf("Exec. off:        $%-8X     #%d\n", i, i);
		i = int4(mod->data.program.mem);
		printf("Data size:        $%-8X     #%d\n", i, i);
		i = int4(mod->data.program.stack);
		printf("Stack size:       $%-8X     #%d\n", i, i);
		i = int4(mod->data.program.idata);
		printf("Init. data off:   $%-8X     #%d\n", i, i);
		i = int4(mod->data.program.irefs);
		printf("Data ref. off:    $%-8X     #%d\n", i, i);
		printf("Prog Mod");
		break;
	case Devic:
		printf("Dev Descr");
		break;
	case Drivr:
		printf("Dev Drv");
		break;
	case FlMgr:
		printf("File Mngr");
		break;
	case Systm:
		printf("System Mod");
		break;
	case Traplib:
		printf("Trap Hnlr");
		break;
	default:
		printf("%s", types[mod->type]);
		break;
	}
	printf(", %s", (mod->lang == Objct) ? "68000 obj" : langs[mod->lang]);

	if (mod->attr & 0x80)
		printf(", Sharable");
	if (mod->attr & 0x40)
		printf(", Sticky Module");
	if (mod->attr & 0x20)
		printf(", System State Process");
	printf("\n");
}


static void ident_os9(OS9_MODULE_t * mod)
{
	int i;
	char *name;
	u_char *CRC, *buffer = (u_char *) mod;
	int module_size, typelang, attrev, hdrparity;
	u_char edition;

	/* gather all information here */
	i = INT(mod->name);
	name = (char *) os9_string(&buffer[i]);
	module_size = INT(mod->size);
	hdrparity = mod->parity;
	CRC = &buffer[module_size - 3];
	edition = buffer[INT(mod->name) + strlen(name)];
	typelang = mod->tyla;
	attrev = mod->atrv;
	u_char calccrc[3] = {0xff, 0xff, 0xff};
	error_code crcerror = _os9_crc_compute((u_char *) mod, module_size, calccrc);

	if (shortFlag == 1)
	{
		char CRCindicator;

		if (crcerror != 0)
		{
			CRCindicator = '.';
		}
		else
		{
			CRCindicator = '?';
		}

		printf("  %3d $%02X $%02X%02X%02X %c %s\n", edition, typelang,
		       CRC[0], CRC[1], CRC[2], CRCindicator, name);
		return;
	}

	printf("Header for : %s\n", name);
	printf("Module size: $%X  #%d\n", module_size, module_size);
	printf("Module CRC : $%02X%02X%02X ", CRC[0], CRC[1], CRC[2]);

	if (crcerror != 0 )
	{
		printf( "(Good)\n" );
	}
	else
	{
		calccrc[0] = 0xff;
		calccrc[1] = 0xff;
		calccrc[2] = 0xff;
		_os9_crc_compute((u_char *) mod, module_size-3, calccrc);
		calccrc[0] ^= 0xff;
		calccrc[1] ^= 0xff;
		calccrc[2] ^= 0xff;

		printf( "(Mismatch calculated: $%02X%02X%02X)\n",
			calccrc[0], calccrc[1], calccrc[2]);
	}

	printf("Hdr parity : $%02X\n", hdrparity);

	switch ((mod->tyla & TYPE_MASK) >> 4)
	{
	case Drivr:
	case Prgrm:
		i = INT(mod->data.program.exec);
		printf("Exec. off  : $%04X  #%d\n", i, i);
		i = INT(mod->data.program.mem);
		printf("Data size  : $%04X  #%d\n", i, i);
		break;

	case Devic:
		printf("File Mgr   : %s\n",
		       os9_string(&buffer[INT(mod->data.descriptor.fmgr)]));
		printf("Driver     : %s\n",
		       os9_string(&buffer[INT(mod->data.descriptor.driver)]));
		break;

	case NULL_TYPE:
	case TYPE_6:
	case TYPE_7:
	case TYPE_8:
	case TYPE_9:
	case TYPE_A:
	case Traplib:
	case Systm:
		break;
	}

	printf("Edition    : $%02X  #%d\n", edition, edition);
	printf("Ty/La At/Rv: $%02X $%02x\n", typelang, attrev);
	printf("%s mod, ", types[(typelang & TYPE_MASK) >> 4]);
	printf("%s, ", langs[typelang & LANG_MASK]);
	printf("%s, %s\n", (attrev & ReEnt) ? "re-ent" : "non-share",
	       (attrev & Modprot) ? "R/W" : "R/O");
	printf("\n");
}


static int do_ident(char **argv, char *filename)
{
	error_code ec = 0;
	int i;
	coco_path_id path;
	OS9_MODULE_t *os9mod = (OS9_MODULE_t *) buffer;
	OSK_MODULE_t *oskmod = (OSK_MODULE_t *) buffer;


	ec = _coco_open(&path, filename, FAM_READ);

	if (ec != 0)
	{
		return (ec);
	}


	while (_coco_gs_eof(path) == 0)
	{
		u_int size = 2;

		ec = _coco_read(path, buffer, &size);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error reading file %s\n",
				argv[0], filename);
			return (ec);
		}

		if ((os9mod->id[0] == OS9_ID0) && (os9mod->id[1] == OS9_ID1))
		{
			size = OS9_HEADER_SIZE - 2;

			if ((ec = _coco_read(path, buffer + 2, &size)) != 0)
			{
				fprintf(stderr, "%s: error reading file %s\n",
					argv[0], filename);
				return (ec);
			}

			if ((i = _os9_header(os9mod)) != 0xFF)
			{
				fprintf(stderr,
					"Bad header parity.  Expected 0xFF, got 0x%02X\n\n",
					i);
				break;
			}

			size = i = int2(os9mod->size) - OS9_HEADER_SIZE;
			ec = _coco_read(path, buffer + OS9_HEADER_SIZE,
					&size);

			if (ec != 0)
			{
				printf("Module short.  Expected 0x%04X, got 0x%04X\n\n", i + OS9_HEADER_SIZE, size + OS9_HEADER_SIZE);
				break;
			}

			ident_os9(os9mod);
		}
		else if ((os9mod->id[0] == OSK_ID0)
			 && (os9mod->id[1] == OSK_ID1))
		{
			size = OSK_HEADER_SIZE - 2;

			if ((ec = _coco_read(path, buffer + 2, &size)) != 0)
			{
				fprintf(stderr, "%s: error reading file %s\n",
					argv[0], filename);
				return (ec);
			}

			if ((i = _osk_header(oskmod)) != 0xFFFF)
			{
				fprintf(stderr,
					"Bad header parity.  Expected 0xFFFF, got 0x%04X\n\n",
					i);
				break;
			}

			size = i = int4(oskmod->size) - OSK_HEADER_SIZE;

			ec = _coco_read(path, buffer + OSK_HEADER_SIZE,
					&size);

			if (ec != 0)
			{
				printf("Module short.  Expected 0x%04X, got 0x%04X\n\n", i + OSK_HEADER_SIZE, size + OSK_HEADER_SIZE);
				break;
			}

			ident_osk(oskmod);
		}
		else
		{
			fprintf(stderr, "Not OS9 module, skipping.\n\n");
			break;
		}
	}

	_coco_close(path);


	return (0);
}


int os9ident(int argc, char **argv)
{
	error_code ec = 0;
	int i;
	char *p = NULL;


	if (argv[1] == NULL)
	{
		show_help(helpMessage);
		return (0);
	}


	/* An OS-9 module can't be larger than this */

	buffer = malloc(65535);

	if (buffer == 0)
	{
		fprintf(stderr, "%s: cannot allocate memory\n", argv[0]);
		return (1);
	}


	/* Walk command line for options */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 's':
					shortFlag = 1;
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

		ec = do_ident(argv, p);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening file %s\n",
				argv[0], ec, p);
			break;
		}
	}

	free(buffer);


	return (ec);
}

#include <util.h>
#include <stdio.h>


typedef unsigned short u16;	/* Little-endian coco int */


/* Static functions */
void ferr(char *s);
unsigned int getwrd(FILE * fp);
void showlcls(void);
void showrefs(void);
void ftext(char c, int ref);
void showglobs(void);
void do_rdump(char *file);
void showhead(void);
unsigned int o9_int(u16 nbr);
void getname(char *s);


/* symbol table types */
/* symbol definition/reference type/location */
/* type flags */
#define CODENT      0x04	/* data/code flag */
/* data type flags */
#define DIRENT      0x02	/* global/direct flag */
#define INIENT      0x01	/* clear/init. data flag */
/* location flags */
#define CODLOC      0x20	/* data/code flag */
#define DIRLOC      0x10	/* global/direct flag */
#define LOC1BYT     0x08	/* two/one byte size flag */
#define LOCMASK     (CODLOC|DIRLOC)
#define NEGMASK     0x40	/* negate on resolution */
#define RELATIVE    0x80	/* relative reference */
/* misc. constants */
#ifdef __APPLE__
#define ROFSYNC     0x62CD2387
#else
#define ROFSYNC	    0x8723CD62
#endif
#define SYMLEN      9		/* Length of symbols */
#define MAXNAME     16		/* length of module name */

/* definition/reference */
typedef struct
{
	char r_flag;		/* type/location */
	u16 r_offset;
} def_ref;


/* rof header structure */
typedef struct
{
	int h_sync;
	u16 h_tylan;
	char h_valid;
	char h_date[5];
	char h_edit;
	char h_spare;
	u16 h_glbl;
	u16 h_dglbl;
	u16 h_data;
	u16 h_ddata;
	u16 h_ocode;
	u16 h_stack;
	u16 h_entry;
} binhead;


#define MAXSOURCE 20
#define puts(s) fputs(s, stdout)
#define mc(c) ((c) & 0xff)
#define DEF 1
#define REF 2

char *snames[MAXSOURCE], *fname;
u16 scount;
u16 gflag, rflag, oflag;
binhead hd;
FILE *in;


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: rdump [opts] <file>[ <file>] [opts]\n",
	"Usage:  prints formatted dump of .r and .l files\n",
	"Options:\n",
	"     -g       add global definition info\n",
	"     -r       add reference info\n",
	"     -o       add reference and local offset info\n",
	"     -a       all of the above\n",
	NULL
};




int os9rdump(int argc, char **argv)
{
	char *p;
	int i;


	/* walk command line for options */
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'g':
					gflag = 1;
					break;
				case 'r':
					rflag = 1;
					break;
				case 'o':
					oflag = 1;
					break;
				case 'a':
					gflag = rflag = oflag = 1;
					break;
				case '?':
				case 'h':
					show_help(helpMessage);
					return (0);
				default:
					fprintf(stderr,
						"rdump: unknown option -%c",
						*p);
					return (EOS_IA);
				}
			}
		}
	}

	/* walk command line for pathnames */
	for (i = 1; i < argc; i++)
	{
		if (argv[0][0] == '-')
		{
			continue;
		}
		else
		{
			p = argv[i];
		}

		do_rdump(p);
	}


	return (0);
}



void do_rdump(char *file)
{
	fname = file;

	if ((in = fopen(fname, "rb")) == NULL)
	{
		printf("can't open '%s'\n", fname);

		return;
	}

	for (;;)
	{
		if (fread(&hd, sizeof(hd), 1, in) == 0)
		{
			break;
		}

		if (hd.h_sync != ROFSYNC)
		{
			printf("'%s' is not a relocatable module\n", fname);
			break;
		}

		showhead();
		showglobs();

		/* skip code and initialized data */
		fseek(in, o9_int(hd.h_ocode +
				 o9_int(hd.h_ddata) + o9_int(hd.h_data)), 1);

		showrefs();
		showlcls();
	}

	fclose(in);
}



/* Switch a Little-Endian number byte order to make it Big-Endian */

unsigned int o9_int(u16 nbr)
{
#ifdef __APPLE__
	return nbr;
#else
	return (((nbr & 0xff00) >> 8) + ((nbr & 0xff) << 8));
#endif
}



void showhead(void)
{
	int c;


	puts("\nModule name: ");

	while ((c = getc(in)))
	{
		putchar(c);
	}

	if (ferror(in))
		ferr(fname);

	printf("\nTyLa/RvAt:   %02x/%02x\n", mc(hd.h_tylan >> 8),
	       mc(hd.h_tylan));
	printf("Asm valid:   %s\n", hd.h_valid ? "No" : "Yes");
	printf("Create date: %.3s %2d, %4d %02d:%02d\n",
	       &("JanFebMarAprMayJunJulAugSepOctNovDec"
		 [(mc(hd.h_date[1]) - 1) * 3]), mc(hd.h_date[2]),
	       1900 + mc(hd.h_date[0]), mc(hd.h_date[3]), mc(hd.h_date[4]));
	printf("Edition:     %2d\n", hd.h_edit);
	puts("  Section    Init Uninit\n");
	printf("   Code:     %04x\n", o9_int(hd.h_ocode));
	printf("     DP:       %02x   %02x\n", o9_int(hd.h_ddata),
	       o9_int(hd.h_dglbl));
	printf("   Data:     %04x %04x\n", o9_int(hd.h_data),
	       o9_int(hd.h_glbl));
	printf("  Stack:     %04x\n", o9_int(hd.h_stack));
	printf("Entry point: %04x\n", o9_int(hd.h_entry));
}



void showglobs(void)
{
	unsigned int count, offset;
	char sym[SYMLEN + 1], flag;


	count = getwrd(in);	/* global def count */
	if (gflag)
		printf("\n%u global symbols defined:\n", count);

	while (count--)
	{
		getname(sym);
		flag = getc(in);
		offset = getwrd(in);
		if (gflag)
		{
			printf(" %9s %04x ", sym, offset);
			ftext(flag, DEF);
		}
	}
}



void getname(char *s)
{
	while ((*s++ = getc(in)));
	*s = '\0';
	if (ferror(in))
		ferr(fname);

	return;
}



void ftext(char c, int ref)
{
	printf("(%02x) ", mc(c));

	if (ref & REF)
	{
		if (c & CODLOC)
			puts("in code");
		else
			puts(c & DIRLOC ? "in dp data" : "in non-dp data");
		puts(c & LOC1BYT ? "/byte" : "/word");
		if (c & NEGMASK)
			puts("/neg");
		if (c & RELATIVE)
			puts("/pcr");
	}

	if (ref & DEF)
	{
		if (ref & REF)
			puts(" - ");
		if (c & CODENT)
			puts("to code");
		else
		{
			puts(c & DIRENT ? "to dp" : "to non-dp");
			puts(c & INIENT ? " data" : " bss");
		}
	}
	putchar('\n');
}


void showrefs(void)
{
	unsigned count, rcount;
	def_ref ref;
	char sym[SYMLEN + 1];
	int fflag;


	count = getwrd(in);

	if (rflag)
	{
		printf("\n%u external references:\n", count);
	}

	while (count--)
	{
		getname(sym);
		rcount = getwrd(in);
		if (rflag)
		{
			printf(" %9s ", sym);
		}
		fflag = 0;

		while (rcount--)
		{
			fread(&ref, sizeof(ref), 1, in);
			if (ferror(in))
				ferr(fname);
			if (rflag && oflag)
			{
				if (fflag)
					puts("           ");
				else
					fflag = 1;
				printf("%04x ", o9_int(ref.r_offset));
				ftext(ref.r_flag, REF);
			}
		}
		if (rflag && !oflag)
			putchar('\n');
	}
}



void showlcls(void)
{
	unsigned count;
	def_ref ref;
	count = getwrd(in);

	if (oflag)
	{
		printf("\n%u local references\n", count);
	}

	while (count--)
	{
		fread(&ref, sizeof(ref), 1, in);

		if (ferror(in))
			ferr(fname);
		if (oflag)
		{
			printf("   %04x ", o9_int(ref.r_offset));
			ftext(ref.r_flag, DEF | REF);
		}
	}
}



unsigned int getwrd(FILE * fp)
{
	unsigned char Msb, Lsb;


	Msb = getc(fp);
	Lsb = getc(fp);

	return ((Msb << 8) + (Lsb));
}



void ferr(char *s)
{
	fprintf(stderr, "rdump: error reading '%s'", s);
}

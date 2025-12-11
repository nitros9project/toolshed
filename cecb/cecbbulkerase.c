/********************************************************************
 * cecbbulkerase.c - Image creation for Cassette BASIC
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
#include <cecbpath.h>

int sample_rate = 22050;
int bits_per_sample = 16;
double silence_length = 0.5;

/* Help message */
static char const *const helpMessage[] = {
	"Syntax: bulkerase {[<opts>]} {<file> [<...>]} {[<opts>]}\n",
	"Usage:  Create cassette image files, WAV for CAS.\n",
	"Options:\n",
	"     -s<num>  = Sample rate of WAV file (11025, 22050, 44100, etc. Default: 22050).\n",
	"     -b<num>  = Bits per sample of WAV file (8, 16, 24, 32, default: 16).\n",
	"     -l<num>  = Length of silence to record in WAV file. (default: 0.5 seconds).\n",
	NULL
};


int cecbbulkerase(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;


	/* 1. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case 'l':
					silence_length = atof(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 's':
					sample_rate = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'b':
					bits_per_sample = atoi(p + 1);
					while (*(p + 1) != '\0')
						p++;
					break;

				case 'h':
				case '?':
					show_help(helpMessage);
					return (0);

				default:
					fprintf(stderr,
						"%s: unknown option '%c'\n",
						argv[0], *p);
					return (0);
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

			ec = _cecb_bulkerase(argv[i], sample_rate,
					     bits_per_sample, silence_length);

			if (ec == 0)
			{
				printf("Creating WAV file: %s\n", argv[i]);
				printf("      Sample Rate: %d\n",
				       sample_rate);
				printf("  Bits Per Sample: %d\n",
				       bits_per_sample);
				printf("   Silence Length: %f\n\n",
				       silence_length);
			}
			else
			{
				fprintf(stderr,
					"%s: cannot open virtual cassette: %s\n\n",
					argv[0], argv[i]);
			}
		}
	}

	if (p == NULL)
	{
		show_help(helpMessage);
		return (0);
	}


	return (0);
}

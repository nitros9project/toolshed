/********************************************************************
 * cecbleader.c - WAV leader anaylizer
 *
 * $Id$
 ********************************************************************/
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "util.h"
#include "cocotypes.h"
#include "cecbpath.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

static int do_leader(char **argv, char *p);
void print_stats(cecb_path_id path, long start_block, long count, long high_sum, long low_sum, long high_max, long high_min, long low_max, long low_min);


/* Help Message */
static char const *const helpMessage[] = {
	"Syntax: leader <wave file>\n",
	"Usage:  Analyze and provide statistics on leaders found in WAV files.\n",
	"Options:\n",
	NULL
};

int cecbleader(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL;
	int i;


	if (argv[1] == NULL)
	{
		show_help(helpMessage);

		return 0;
	}

	/* walk command line for options */
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
					return 0;
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

		ec = do_leader(argv, p);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d opening '%s'\n",
				argv[0], ec, p);

			return ec;
		}
	}

	return 0;
}

static int do_leader(char **argv, char *p)
{
	error_code ec = 0;
	cecb_path_id path;
	long wave1, wave2;
	long low_sum, high_sum, count, start_block;
	long low_min, low_max, high_min, high_max;
	double ratio;

	ec = _cecb_open(&path, p, FAM_READ | FAM_RAW);

	if (ec == 0 && path->israw == 1)
	{
		low_sum = 0;
		high_sum = 0;
		count = 0;
		start_block = 0;
		low_max = 0;
		low_min = LONG_MAX;
		high_max = 0;
		high_min = LONG_MAX;

		printf( "\nLeader analysis of %s.\n", p);
		printf( "Find and report blocks of alternating frequencies.\n");
		printf( "Target ratio: %f (%f - %f)\n", cecb_ratio, path->wav_ratio_low, path->wav_ratio_high);
		printf( "Dead zone: %f\n\n", cecb_zero_adjust);
	
		if(path->tape_type == WAV)
		{
			/* preload 5 zero crossings */
			position_tape(path, path->play_at);
			path->wav_crossings[4] = next_zero_crossing(path);
			path->wav_crossings[3] = next_zero_crossing(path);
			path->wav_crossings[2] = next_zero_crossing(path);
			path->wav_crossings[1] = next_zero_crossing(path);
			path->wav_crossings[0] = next_zero_crossing(path);

			while(path->wav_current_sample < path->wav_total_samples)
			{
				wave1 = path->wav_crossings[2] - path->wav_crossings[4];
				wave2 = path->wav_crossings[0] - path->wav_crossings[2];
			
				ratio = (float)wave1 / wave2;
				
				if (ratio>path->wav_ratio_low && ratio<path->wav_ratio_high)
				{
					low_sum += wave1;
					low_max = MAX(wave1, low_max);
					low_min = MIN(wave1, low_min);
					high_sum += wave2;
					high_max = MAX(wave2, high_max);
					high_min = MIN(wave2, high_min);
					count += 1;

					path->wav_crossings[4] = path->wav_crossings[0];
					path->wav_crossings[3] = next_zero_crossing(path);
					path->wav_crossings[2] = next_zero_crossing(path);
					path->wav_crossings[1] = next_zero_crossing(path);
					path->wav_crossings[0] = next_zero_crossing(path);
				}
				else
				{
					if(count>20)
					{
						print_stats(path, start_block, count, high_sum, low_sum, high_max, high_min, low_max, low_min);
					}
					
					low_sum = 0;
					high_sum = 0;
					count = 0;
					low_max = 0;
					low_min = LONG_MAX;
					high_max = 0;
					high_min = LONG_MAX;
				
					path->wav_crossings[4] = path->wav_crossings[3];
					path->wav_crossings[3] = path->wav_crossings[2];
					path->wav_crossings[2] = path->wav_crossings[1];
					path->wav_crossings[1] = path->wav_crossings[0];
					path->wav_crossings[0] = next_zero_crossing(path);
					
					start_block = path->wav_crossings[4];
				}
			}
		}
		else
		{
			printf("File: %s, not a WAV file\n\n", p);
		}

		if(count>20)
		{
			print_stats(path, start_block, count, high_sum, low_sum, high_max, high_min, low_max, low_min);
		}

		ec = _cecb_close(path);
	}

	return ec;
}

void print_stats(cecb_path_id path, long start_block, long count, long high_sum, long low_sum, long high_max, long high_min, long low_max, long low_min)
{
	printf( "Block length %f seconds (%ld-%ld)\n", (path->wav_crossings[0] - start_block) / (float)path->wav_sample_rate, start_block, path->wav_crossings[0] );
	printf( "Zero crossing count: %ld\n", count);
	printf( "Average ratio: %f\n", ((float)path->wav_sample_rate / ((float)high_sum / count)) / ((float)path->wav_sample_rate / ((float)low_sum / count)));
	printf( "High frequency average: %.1f Hz, ", (float)path->wav_sample_rate / ((float)high_sum / count));
	printf( "max: %.1f Hz, min %.1f Hz\n", (float)path->wav_sample_rate / high_max, (float)path->wav_sample_rate / high_min);
	printf( "Low frequency average: %.1f Hz, ", (float)path->wav_sample_rate / ((float)low_sum / count));
	printf( "max: %.1f Hz, min %.1f Hz\n", (float)path->wav_sample_rate / low_max, (float)path->wav_sample_rate / low_min);
	printf( "\n");
}

/********************************************************************
 * seek.c - Disk BASIC Seek routine
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "cecbpath.h"


error_code _cecb_seek(cecb_path_id path, int pos, int mode)
{
	error_code ec = 0;


	if (path->israw == 1)
	{
		ec = fseek(path->fd, pos, mode);
	}
	else
	{
		switch (mode)
		{
		case SEEK_SET:
			if (pos != 0 )
			{
				fprintf( stderr,
					"_cecb_seek(): SEEK_SET only implemented if pos is zero.\n");
				ec = -1;
			}
			
			if (path->filepos == 0 ) break;
			
			if ((path->tape_type == CAS) || (path->tape_type == C10))
			{
				path->cas_current_byte = path->cas_start_byte;
				path->cas_current_bit = path->cas_start_bit;
				fseek(path->fd, path->cas_current_byte, SEEK_SET);
			}
			else
			{
				fseek(path->fd, path->wav_data_start, SEEK_SET);
				path->wav_current_sample = path->wav_start_sample;
				if (path->wav_bits_per_sample == 8)
				{
					fseek(path->fd, path->wav_start_sample, SEEK_CUR);
				}
				else if (path->wav_bits_per_sample == 16)
				{
					fseek(path->fd, path->wav_start_sample*2, SEEK_CUR);
				}
				else
				{
					fprintf(stderr, "_cecb_seek: unknown bits per sample\n");
					ec = -1;
				}
			}

			path->filepos = pos;
			path->eof_flag = 0;
			break;

		case SEEK_CUR:
			fprintf(stderr,
				"_cecb_seek(): SEEK_CUR not implemented.\n");
			ec = -1;
			break;

		case SEEK_END:
			fprintf(stderr,
				"_cecb_seek(): SEEK_END not implemented.\n");
			ec = -1;
			break;
		}
	}


	return ec;
}


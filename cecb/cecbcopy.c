/********************************************************************
 * cecbcopy.c - Copy utility for Cassette BASIC
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <cococonv.h>
#include <cecbpath.h>
#include <sys/stat.h>


#define YES 1
#define NO 0

/* globals */
//static u_int buffer_size = 32768;
//static char *buffer;

static error_code CopyCECBFile(char *srcfile, char *dstfile, int eolTranslate,
			       int tokTranslate, int s_record,
			       int binary_concat, int file_type,
			       int data_type, int gap, int ml_load_address,
			       int ml_exec_address);
static char *GetFilename(char *path);


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: copy {[<opts>]} <srcfile> {[<...>]} <target> {[<opts>]}\n",
	"Usage:  Copy one or more files to a target directory.\n",
	"Options:\n",
	"     -[0-3]     file type (when copying to a Cassette BASIC image)\n",
	"                  0 = BASIC program\n",
	"                  1 = BASIC data file\n",
	"                  2 = machine-language program\n",
	"                  3 = text editor source file\n",
	"     -[a|b]     data type (a = ASCII, b = binary)\n",
	"     -[g|n]     gap flag (g = gaps, n = no gaps)\n",
	"     -d<n>      load address\n",
	"     -e<n>      execution address\n",
	"     -l         perform end of line translation\n",
	"     -t         perform BASIC entokenization of ASCII text\n",
	"     -k         perform BASIC detokenization of binary data\n",
	"     -s         perform S Record encode of machine language loadables\n",
	"     -f         perform S Record decode of ASCII text file\n",
	"     -c         perform segment concatenation on machine language loadables\n",
	NULL
};



int cecbcopy(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL, *desttarget = NULL;
	int i, j;
	int targetDirectory = NO;
	int count = 0;
	int eolTranslate = 0, tokTranslate = 0, s_record = 0, binary_concat =
		0, ml_exec_address = -1, ml_load_address = -1;
	int file_type = -1, data_type = -1, gap = -1;
	char df[256];


	/* 1. Walk command line for options. */

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
		{
			for (p = &argv[i][1]; *p != '\0'; p++)
			{
				switch (*p)
				{
				case '0':
					file_type = 0;
					break;

				case '1':
					file_type = 2;
					break;

				case '2':
					file_type = 2;
					break;

				case '3':
					file_type = 3;
					break;

				case 'a':
					data_type = 255;
					break;

				case 'g':
					gap = 255;
					break;

				case 'n':
					gap = 0;
					break;

				case 'b':
					data_type = 0;
					break;

				case 'l':
					eolTranslate = 1;
					break;

				case 't':
					tokTranslate = 1;
					break;

				case 'k':
					tokTranslate = -1;
					break;

				case 'c':
					binary_concat = 1;
					break;

				case 's':
					s_record = 1;
					break;

				case 'f':
					s_record = -1;
					break;

				case 'd':
					p = p + 1;
					ml_load_address = strtol(p, &p, 0);
					p = p - 1;
					break;

				case 'e':
					p = p + 1;
					ml_exec_address = strtol(p, &p, 0);
					p = p - 1;
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

	/* 2. Count non option arguments. */

	for (i = 1, count = 0; i < argc; i++)
	{
		if (argv[i] == NULL)
		{
			continue;
		}

		if (argv[i][0] == '-')
		{
			continue;
		}

		count++;
	}

	if (count == 0)
	{
		show_help(helpMessage);

		return 0;
	}


	/* 3. Walk backwards and get the destination first. */

	for (i = argc - 1; i > 0; i--)
	{
		error_code ec;
		coco_path_id tmp_path;


		if (argv[i][0] != '-')
		{
			desttarget = argv[i];


			/* 1. Determine if dest is native */

			ec = _coco_open(&tmp_path, desttarget,
					FAM_DIR | FAM_READ);

//                      _coco_gs_pathtype(desttarget, &type);


			if (ec == 0)
			{
				targetDirectory = YES;

				_coco_close(tmp_path);
			}
			else
			{
				targetDirectory = NO;
			}

			break;
		}
	}

	if (targetDirectory == NO && count > 2)
	{
		printf("Error: two or more sources requires target to be a directory.\n\n");
		show_help(helpMessage);
		return (0);
	}

	/* Now look for the source files  */
	for (j = 1; j < i; j++)
	{
		if (argv[j][0] == '-')
			continue;

		if (argv[j] == NULL)
			continue;

		memset(df, 0, 256);


		if (targetDirectory == YES)
		{
			/* OK, we need to add the filename to the end of the directory path */

			if ((p = strchr(desttarget, ',')) != NULL)
			{
				if (*(p + 1) == ':')
				{
					strncpy(df, desttarget,
						p - desttarget + 1);

					strcat(df, GetFilename(argv[j]));

					strcat(df, p + 1);
				}
				else
				{
					strcpy(df, desttarget);
					strcat(df, GetFilename(argv[j]));
				}
			}
		}
		else
		{
			strcpy(df, desttarget);
		}


		ec = CopyCECBFile(argv[j], df, eolTranslate, tokTranslate,
				  s_record, binary_concat, file_type,
				  data_type, gap, ml_load_address,
				  ml_exec_address);

		if (ec != 0)
		{
			fprintf(stderr, "%s: `%s`: error %d\n", argv[0], df, ec);
		}
	}


	return ec;
}


static char *GetFilename(char *path)
{
	/* This works for both native file paths and os9 file paths */

	char *a, *b;

	a = strchr(path, ',');

	if (a == NULL)
	{
		/* Native file */
		a = strrchr(path, '/');

		if (a == NULL)
		{
			return path;
		}

		return a + 1;
	}

	b = strrchr(a, '/');

	if (b == NULL)
	{
		return a + 1;
	}

	return b + 1;
}



#define BLOCKSIZE 256

static error_code CopyCECBFile(char *srcfile, char *dstfile, int eolTranslate,
			       int tokTranslate, int s_record,
			       int binary_concat, int file_type,
			       int data_type, int gap, int ml_load_address,
			       int ml_exec_address)
{
	error_code ec = 0;
	coco_path_id path, destpath;
	int mode = FAM_NOCREATE | FAM_WRITE;
	u_char *buffer = NULL;
	u_int buffer_size;
	_path_type dst_path_type, src_path_type;
	coco_file_stat fstat;

	/* 1. Open a path to the srcfile. */

	ec = _coco_open_read_whole_file(&path, srcfile, FAM_READ, &buffer,
					&buffer_size);

	if (ec != 0)
	{
		return ec;
	}

	/* 2. Apply meta data */

	_coco_gs_pathtype(path, &src_path_type);
	_coco_gs_fd(path, &fstat);

	if ((file_type == -1) && (tokTranslate == 1))
		file_type = 0;
	else if (tokTranslate == -1)
		file_type = 0;

	if ((data_type == -1) && (tokTranslate == -1))
		data_type = 0;
	else if (tokTranslate == -1)
		data_type = 0xff;

	if (s_record != 0)
	{
		if (s_record == 1)	/* s record encode */
		{
			char *encode_buffer;
			u_int encode_size;

			/* encode file */
			ec = _decb_srec_encode(buffer, buffer_size,
					       &encode_buffer, &encode_size);

			if (ec == 0)
			{
				free(buffer);
				buffer = (u_char *) encode_buffer;
				buffer_size = encode_size;
			}
			else
				return -1;
		}
		else if (s_record == -1)	/* s record decode */
		{
			unsigned char *decode_buffer;
			u_int decode_size;

			/* decode file */
			ec = _decb_srec_decode(buffer, buffer_size,
					       &decode_buffer, &decode_size);

			if (ec == 0)
			{
				free(buffer);
				buffer = decode_buffer;
				buffer_size = decode_size;

				if (file_type == -1)
					file_type = 2;

				if (data_type == -1)
					data_type = 0;

				if (gap == -1)
					gap = 0xff;
			}
			else
				return -1;
		}
	}

	if (binary_concat == 1)
	{
		u_char *binconcat_buffer;
		u_int binconcat_size;

		ec = _decb_binconcat(buffer, buffer_size, &binconcat_buffer,
				     &binconcat_size);

		if (ec == 0)
		{
			free(buffer);
			buffer = binconcat_buffer;
			buffer_size = binconcat_size;
		}
		else
			return -1;
	}

	_coco_identify_image(dstfile, &dst_path_type);

	if ((dst_path_type == CECB) && (gap == 0x00) && (file_type == 2)
	    && (data_type == 0))
	{
		/* only one segment allowed */
		int count;
		u_char *extracted_buffer;
		u_int extracted_buffer_size;
		u_int load_address, exec_address;

		count = _decb_count_segements(buffer, buffer_size);

		if ((count == 0) || (count > 1))
		{
			fprintf(stderr,
				"Error copying multiple segement binary to single segement file.\n");
			return -1;
		}

		ec = _decb_extract_first_segment(buffer, buffer_size,
						 &extracted_buffer,
						 &extracted_buffer_size,
						 &load_address,
						 &exec_address);

		if (ec == 0)
		{
			free(buffer);
			buffer = extracted_buffer;
			buffer_size = extracted_buffer_size;

			if (ml_load_address == -1)
				ml_load_address = load_address;

			if (ml_exec_address == -1)
				ml_exec_address = exec_address;

		}
		else
			return -1;
	}

	if (file_type == -1)
		fstat.file_type = fstat.file_type;
	else
		fstat.file_type = file_type;

	if (data_type == -1)
		fstat.data_type = fstat.data_type;
	else
		fstat.data_type = data_type;

	if (gap == -1)
		fstat.gap_flag = fstat.gap_flag;
	else
		fstat.gap_flag = gap;

	if (ml_load_address == -1)
		fstat.ml_load_address = fstat.ml_load_address;
	else
		fstat.ml_load_address = ml_load_address;

	if (ml_exec_address == -1)
		fstat.ml_exec_address = fstat.ml_exec_address;
	else
		fstat.ml_exec_address = ml_exec_address;


	/* 3. Attempt to create the destfile. */

	ec = _coco_create(&destpath, dstfile, mode, &fstat);

	if (ec != 0)
	{
		_coco_close(path);

		return ec;
	}


	if (tokTranslate != 0)
	{
		u_char *xx_tokenize_buffer = NULL;
		u_int xx_tokenize_size;

		if (tokTranslate == 1) /* entokenize */
		{
			if (cecb_suggest_mc10 || (src_path_type == CECB && path->path.cecb->tape_type == C10))
			{
				/* Tokenized file */
				ec = _cecb_entoken(buffer, buffer_size,
						   &xx_tokenize_buffer,
						   &xx_tokenize_size,
						   destpath->type == DECB);
		   }
		   else
		   {
				ec = _decb_entoken(buffer, buffer_size,
						   &xx_tokenize_buffer,
						   &xx_tokenize_size,
						   destpath->type == DECB);
		   }

			eolTranslate = 0;
		}
		else /* detokenize */
		{
			if (buffer[0] == 0xff)
			{
				/* skip past disk flag and file size */
				buffer += 3;
				buffer_size -= 3;
			}

			if (cecb_suggest_mc10 || (src_path_type == CECB && path->path.cecb->tape_type == C10))
			{
				ec = _cecb_detoken(buffer, buffer_size,
							(char **) &xx_tokenize_buffer,
							&xx_tokenize_size);
			}
			else
			{
				ec = _decb_detoken(buffer, buffer_size,
						   (char **) &xx_tokenize_buffer,
						   &xx_tokenize_size);
			}
		}

		if (ec == 0)
		{
			free(buffer);
			buffer = xx_tokenize_buffer;
			buffer_size = xx_tokenize_size;
		}
		else
		{
			if (xx_tokenize_buffer)
				free(xx_tokenize_buffer);
			return ec;
		}
	}

	if (eolTranslate == 1)
	{
		char *translation_buffer;
		u_int new_translation_size;

		if (path->type == NATIVE)
		{
			/* source is native, destination is coco */

			NativeToDECB((char *) buffer, buffer_size,
				     &translation_buffer,
				     &new_translation_size);

			free(buffer);
			buffer = (u_char *) translation_buffer;
			buffer_size = new_translation_size;
		}
		else if (path->type != NATIVE)
		{
			/* source is coco, destination is native */

			DECBToNative((char *) buffer, buffer_size,
				     &translation_buffer,
				     &new_translation_size);

			free(buffer);
			buffer = (u_char *) translation_buffer;
			buffer_size = new_translation_size;
		}
	}

	ec = _coco_write(destpath, buffer, &buffer_size);

	if (buffer != NULL)
		free(buffer);

	_coco_close(path);
	_coco_close(destpath);

	if (ec != 0)
		return -1;

	return (ec);
}

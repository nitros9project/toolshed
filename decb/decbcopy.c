/********************************************************************
 * decbcopy.c - Copy utility for Disk BASIC
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <cococonv.h>
#include <decbpath.h>
#include <sys/stat.h>


#define YES 1
#define NO 0

/* globals */
//static u_int buffer_size = 32768;
//static char *buffer;

static error_code CopyDECBFile(char *srcfile, char *dstfile, int eolTranslate,
			       int tokTranslate, int binary_concat,
			       int rewrite, int file_type, int data_type);
static char *GetFilename(char *path);


/* Help message */
static char const *const helpMessage[] = {
	"Syntax: copy {[<opts>]} <srcfile> {[<...>]} <target> {[<opts>]}\n",
	"Usage:  Copy one or more files to a target directory.\n",
	"Options:\n",
	"     -[0-3]     file type (when copying to a Disk BASIC image)\n",
	"                  0 = BASIC program\n",
	"                  1 = BASIC data file\n",
	"                  2 = machine-language program\n",
	"                  3 = text editor source file\n",
	"     -[a|b]     data type (a = ASCII, b = binary)\n",
	"     -l         perform end of line translation\n",
	"     -r         rewrite if file exists\n",
	"     -t         perform BASIC token translation\n",
	"     -c         perform segment concatenation on machine language loadables\n",
	NULL
};



int decbcopy(int argc, char *argv[])
{
	error_code ec = 0;
	char *p = NULL, *desttarget = NULL;
	int i, j;
	int targetDirectory = NO;
	int count = 0;
	int eolTranslate = 0, tokTranslate = 0, binary_concat = 0;
	int rewrite = 0;
	int file_type = -1, data_type = -1;
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
					file_type = 1;
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

				case 'b':
					data_type = 0;
					break;

				case 'l':
					eolTranslate = 1;
					break;

				case 'r':
					rewrite = 1;
					break;

				case 't':
					tokTranslate = 1;
					break;

				case 'c':
					binary_concat = 1;
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


		ec = CopyDECBFile(argv[j], df, eolTranslate, tokTranslate,
				  binary_concat, rewrite, file_type,
				  data_type);

		if (ec != 0)
		{
			fprintf(stderr, "%s: error %d\n", argv[0], ec);
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



static error_code CopyDECBFile(char *srcfile, char *dstfile, int eolTranslate,
			       int tokTranslate, int binary_concat,
			       int rewrite, int file_type, int data_type)
{
	error_code ec = 0;
	coco_path_id path;
	coco_path_id destpath;
	int mode = FAM_NOCREATE | FAM_WRITE;
	unsigned char *buffer = NULL;
	char *translation_buffer;
	u_int new_translation_size;
	u_int buffer_size;
	coco_file_stat fstat;


	/* 1. Set mode based on rewrite. */

	if (rewrite == 1)
	{
		mode &= ~FAM_NOCREATE;
	}


	/* 2. Open a path to the srcfile. */

	ec = _coco_open(&path, srcfile, FAM_READ);

	if (ec != 0)
	{
		return ec;
	}


	/* 3. Attempt to create the destfile. */

	fstat.perms = FAP_READ | FAP_WRITE | FAP_PREAD;
	ec = _coco_create(&destpath, dstfile, mode, &fstat);

	if (ec != 0)
	{
		_coco_close(path);

		return ec;
	}


	ec = _coco_gs_size(path, &buffer_size);

	if (buffer_size > 0)
	{
		buffer = malloc(buffer_size);

		if (buffer == NULL)
		{
			return -1;
		};

		ec = _coco_read(path, buffer, &buffer_size);

		if (ec != 0)
		{
			return -1;
		}

		if (binary_concat == 1)
		{
			u_char *binconcat_buffer;
			u_int binconcat_size;

			ec = _decb_binconcat(buffer, buffer_size,
					     &binconcat_buffer,
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

		if (tokTranslate == 1)
		{
			if (buffer[0] == 0xff)
			{
				u_char *detokenize_buffer = NULL;
				u_int detokenize_size;

				/* File is already a tokenized BASIC file, de-tokenize it */
				ec = _decb_detoken(buffer, buffer_size,
						   (char **)
						   &detokenize_buffer,
						   &detokenize_size);

				if (ec == 0)
				{
					free(buffer);
					buffer = detokenize_buffer;
					buffer_size = detokenize_size;

					file_type = 0;
					data_type = 0xff;
				}
				else
				{
					if (detokenize_buffer != NULL)
						free(detokenize_buffer);
					return -1;
				}
			}
			else
			{
				unsigned char *entokenize_buffer = NULL;
				u_int entokenize_size;

				/* Tokenized file */
				ec = _decb_entoken(buffer, buffer_size,
						   &entokenize_buffer,
						   &entokenize_size,
						   destpath->type == DECB);

				if (ec == 0)
				{
					free(buffer);
					buffer = entokenize_buffer;
					buffer_size = entokenize_size;

					file_type = 0;
					data_type = 0;

					eolTranslate = 0;

				}
				else
				{
					if (entokenize_buffer != NULL)
						free(entokenize_buffer);
					return -1;
				}
			}
		}

		if (eolTranslate == 1)
		{
			if (path->type == NATIVE && destpath->type != NATIVE)
			{
				/* source is native, destination is coco */

				NativeToDECB((char *) buffer, buffer_size,
					     &translation_buffer,
					     &new_translation_size);

				ec = _coco_write(destpath, translation_buffer,
						 &new_translation_size);

				free(translation_buffer);
			}
			else if (path->type != NATIVE
				 && destpath->type == NATIVE)
			{
				/* source is coco, destination is native */

				DECBToNative((char *) buffer, buffer_size,
					     &translation_buffer,
					     &new_translation_size);
				ec = _coco_write(destpath, translation_buffer,
						 &new_translation_size);

				free(translation_buffer);
			}
		}
		else
		{
			/* One-to-one writing of the data -- no translation needed. */

			ec = _coco_write(destpath, buffer, &buffer_size);
		}

		if (ec == EOS_DF)
		{
			/* Delete file, and return disk full */
			_coco_close(destpath);
			
			ec = _coco_delete(dstfile);
			
			if( ec != 0 )
			{
				fprintf(stderr, "File write failed, then file delete failed.\n");
			}
			
			return EOS_DF;
		}
		
		if (ec != 0)
		{
			return ec;
		}


		/* Copy meta data from file descriptor of source to destination */

		{
			coco_file_stat fd;


			_coco_gs_fd(path, &fd);

			_coco_ss_fd(destpath, &fd);
		}


		/* Special -- if this is a DECB file we wrote to, set passed file type and data type. */

		{
			_path_type t;

			_coco_gs_pathtype(destpath, &t);

			if (t == DECB)
			{
				decb_file_stat f;


				_decb_gs_fd(destpath->path.decb, &f);

				if (file_type >= 0)
				{
					f.file_type = file_type;
				}

				if (data_type >= 0)
				{
					f.data_type = data_type;
				}

				_decb_ss_fd(destpath->path.decb, &f);
			}
		}
	}

	// free whatever buffer ended up being
	if (buffer)
		free(buffer);

	_coco_close(path);
	_coco_close(destpath);

	return (ec);
}

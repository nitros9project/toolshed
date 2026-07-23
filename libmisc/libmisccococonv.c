/********************************************************************
 * $Id$
 *
 * Functions to facilitate line-ending, error number, string and
 * time conversions between platforms.
 ********************************************************************/
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <cococonv.h>
#include <cocopath.h>
#include <cocotypes.h>


static EOL_Type DetermineEOLType(char *buffer, int size);

/* Helper function to get the current timestamp, respecting SOURCE_DATE_EPOCH */
time_t GetSafeTimestamp(void)
{
	char *sde_env = getenv("SOURCE_DATE_EPOCH");
	if (sde_env != NULL)
	{
		char *endptr;
		time_t sde_val = (time_t)strtoll(sde_env, &endptr, 10);
		if (*endptr == '\0' && sde_val >= 0)
		{
			return sde_val;
		}
	}

	time_t tp;
	time(&tp);
	return tp;
}


int CoCoToUnixPerms(int attrs)
{
	int ret = 0;

	if (attrs & FAP_DIR)
	{
		ret = S_IFDIR;
	}
	else
	{
		ret = S_IFREG;
	}

	if (attrs & FAP_READ)
	{
		ret |= S_IRUSR;
	}
	if (attrs & FAP_WRITE)
	{
		ret |= S_IWUSR;
	}
	if (attrs & FAP_EXEC)
	{
		ret |= S_IXUSR;
	}
#if !defined(WIN32)
	if (attrs & FAP_PREAD)
	{
		ret |= S_IROTH;
	}
	if (attrs & FAP_PWRITE)
	{
		ret |= S_IWOTH;
	}
	if (attrs & FAP_PEXEC)
	{
		ret |= S_IXOTH;
	}
#endif

	return ret;
}


int UnixToCoCoPerms(int attrs)
{
	int ret = 0;

	if (attrs & S_IRUSR)
	{
		ret |= FAP_READ;
	}
	if (attrs & S_IWUSR)
	{
		ret |= FAP_WRITE;
	}
	if (attrs & S_IXUSR)
	{
		ret |= FAP_EXEC;
	}
#if !defined(WIN32)
	if (attrs & S_IROTH)
	{
		ret |= FAP_PREAD;
	}
	if (attrs & S_IWOTH)
	{
		ret |= FAP_PWRITE;
	}
	if (attrs & S_IXOTH)
	{
		ret |= FAP_PEXEC;
	}
#endif

	return ret;
}


char *UnixToOS9Time(time_t currentTime, char *os9time)
{
    struct tm *x;
    char *sde_env = getenv("SOURCE_DATE_EPOCH");

    if (sde_env != NULL)
    {
        char *endptr;
        time_t sde_val = (time_t)strtoll(sde_env, &endptr, 10);
        
        /* Ensure valid non-negative integer and no leftover junk */
        if (*endptr == '\0' && sde_val >= 0)
        {
            currentTime = sde_val;
            /* Use gmtime for SOURCE_DATE_EPOCH per specification */
            x = gmtime(&currentTime);
        }
        else
        {
            x = localtime(&currentTime);
        }
    }
    else
    {
        x = localtime(&currentTime);
    }

    if (x != NULL)
    {
        os9time[0] = x->tm_year;
        os9time[1] = x->tm_mon + 1;
        os9time[2] = x->tm_mday;
        os9time[3] = x->tm_hour;
        os9time[4] = x->tm_min;
    }

    return os9time;
}

/* Converts a regular string to an OS-9 filename. OS-9 stores filename with the last
 * character in the name as with bit 7 set. This is also used in LSN0 for
 * the disk name.
 */
u_char *CStringToOS9String(u_char * f)
{
	u_char *p;
	int len = strlen((char *) f);

	if (len > 0)
	{
		p = f + len - 1;
		*p |= 0x80;
	}

	return (f);
}


int OS9Strlen(u_char * f)
{
	int count = 0;

	do
	{
		count++;
	}
	while (!(*f++ & 0x80));

	return (count);
}


/*
 * Converts an OS-9 string to a regular C string.
 * OS-9 stores filename with the last character in the name as with bit 7
 * set. This is also used in LSN0 for the disk name.
 */
u_char *OS9StringToCString(u_char * f)
{
	u_char *p;

	p = f;
	while (*p != '\0')
	{
		if (*p & 0x80)
		{
			*p &= ~0x80;
			*(p + 1) = '\0';
		}
		p++;
	}

	return (f);
}


/* Converts a C string to a Disk BASIC filename padded with spaces.
   NOTE: We presume that the passed string is in 8.3 format
 */
void CStringToDECBString(u_char * filename, u_char * ext, u_char * string)
{
	u_char *fp, *fpp;

	memset(filename, 32, 8);
	memset(ext, 32, 3);

	fp = string;
	fpp = filename;
	while (*fp != '.' && *fp != '\0')
	{
		*fpp = *fp;
		fp++;
		fpp++;
	}

	if (*fp == '.')
	{
		fpp = ext;
		fp++;
		while (*fp != '\0')
		{
			*fpp = *fp;
			fp++;
			fpp++;
		}
	}
}


/*
 * Converts a Disk BASIC filename to a regular C string.
 */
void DECBStringToCString(u_char * filename, u_char * ext, u_char * string)
{
	int count = 0;


	/* 1. Copy filename. */

	memcpy(string, filename, 8);
	string += 8;

	while (*(string - 1) == ' ' && count++ < 8)
	{
		string--;
	}

	/* 2. If an extension exists, add it. */

	if (!(ext[0] == ' ' && ext[1] == ' ' && ext[2] == ' '))
	{
		*(string++) = '.';

		/* 1. Copy extension. */

		count = 0;
		memcpy(string, ext, 3);
		string += 3;

		while (*(string - 1) == ' ' && count++ < 3)
		{
			string--;
		}
	}

	*string = '\0';

	return;
}


error_code UnixToCoCoError(int ec)
{
	switch (ec)
	{
	case 0:
		return 0;

	case ENOTDIR:
	case EPERM:
	case EACCES:
		return (EOS_FNA);

	case ENOENT:
		return (EOS_PNNF);

	case EIO:
		return (EOS_PNNF);

	case EBADF:
		return (EOS_BMODE);

	case EEXIST:
		return (EOS_FAE);

	case ENFILE:
	case EMFILE:
		return (EOS_PTHFUL);

	case ENOSPC:
		return (EOS_DF);

	case EROFS:
		return (EOS_BMODE);

	case ENAMETOOLONG:
		return (EOS_BPNAM);

	default:
		return ec;
	}
}


int CoCoToUnixError(error_code ec)
{
	switch (ec)
	{
	case 0:
		return 0;

	case EOS_FNA:
		return (EACCES);

	case EOS_PNNF:
		return (ENOENT);

	case EOS_BMODE:
		return (EBADF);

	case EOS_FAE:
		return (EEXIST);

	case EOS_PTHFUL:
		return (ENFILE);

	case EOS_DF:
		return (ENOSPC);

	case EOS_BPNAM:
		return (ENAMETOOLONG);

	default:
		return -1;
	}
}


/*
 * Scan a buffer to determine the type of end-of-line termination it has.
 *
 * Returns EOL_DOS, EOL_UNIX or EOL_OS9
 */
static EOL_Type DetermineEOLType(char *buffer, int size)
{
	EOL_Type eol = 0;
	int i;


	/* Scan to determine EOL ending type */

	for (i = 0; i < size; i++)
	{
		if (i < size - 1
		    && (buffer[i] == 0x0D && buffer[i + 1] == 0x0A))
		{
			/* We have DOS/Windows line endings (0D0A)... */
			eol = EOL_DOS;

			break;
		}

		if (buffer[i] == 0x0A)
		{
			/* We have unix line endings. */
			eol = EOL_UNIX;

			break;
		}

		if (buffer[i] == 0x0D)
		{
			/* We have OS-9 line endings. */
			eol = EOL_OS9;

			break;
		}
	}


	return eol;
}


/*
 * Converts a buffer containing native EOLs to one with Disk BASIC EOLs.
 *
 * The caller must free the returned buffer in 'newBuffer' once
 * finished with the buffer.
 */
void NativeToDECB(char *buffer, u_int size, char **newBuffer, u_int * newSize)
{
	EOL_Type eolMethod;
	int i;


	eolMethod = DetermineEOLType(buffer, size);

	switch (eolMethod)
	{
	case EOL_UNIX:
		/* Change all occurences of 0x0A to 0x0D */

		for (i = 0; i < size; i++)
		{
			if (buffer[i] == 0x0A)
			{
				buffer[i] = 0x0D;
			}
		}
		*newBuffer = (char *) malloc(size);
		if (*newBuffer == NULL)
		{
			return;
		}

		memcpy(*newBuffer, buffer, size);

		*newSize = size;

		break;

	case EOL_DOS:
		/* Things are a bit more involved here. */

		/* We will strip all 0x0As out of the buffer, leaving the 0x0Ds. */

		{
			u_int dosEOLCount = 0;
			char *newP;
			int i;


			/* 1. First we count up the number of 0x0A line endings. */

			for (i = 0; i < size; i++)
			{
				if (buffer[i] == 0x0A)
				{
					dosEOLCount++;
				}
			}


			/* 2. Now we allocate a buffer to hold the current size -
			   'dosEOLCount' bytes.
			 */

			*newSize = size - dosEOLCount;

			*newBuffer = (char *) malloc(*newSize);

			if (*newBuffer == NULL)
			{
				return;
			}

			newP = *newBuffer;

			for (i = 0; i < size; i++)
			{
				if (buffer[i] != 0x0A)
				{
					*newP = buffer[i];
					newP++;
				}
			}
		}
		break;

	default:
		return;
	}


	return;
}


void DECBToNative(char *buffer, u_int size, char **newBuffer, u_int * newSize)
{
#ifdef WIN32
	u_int dosEOLCount = 0;
	char *newP;
	int i;


	/* Things are a bit more involved here. */

	/* We will add 0x0As after all 0x0Ds. */


	/* 1. First we count up the number of 0x0D Disk BASIC line endings. */

	for (i = 0; i < size; i++)
	{
		if (buffer[i] == 0x0D)
		{
			dosEOLCount++;
		}
	}


	/* 2. Now we allocate a buffer to hold the current size +
	   'dosEOLCount' bytes.
	 */

	*newSize = size + dosEOLCount;
	*newBuffer = (char *) malloc(*newSize);

	if (*newBuffer == NULL)
	{
		return;
	}

	newP = *newBuffer;

	for (i = 0; i < size; i++)
	{
		*newP = buffer[i];
		newP++;

		if (buffer[i] == 0x0D)
		{
			*newP = 0x0A;
			newP++;
		}
	}
#else
	int i;


	/* Change all occurences of 0x0D to 0x0A */

	for (i = 0; i < size; i++)
	{
		if (buffer[i] == 0x0D)
		{
			buffer[i] = 0x0A;
		}
	}

	*newBuffer = (char *) malloc(size);
	if (*newBuffer == NULL)
	{
		return;
	}

	memcpy(*newBuffer, buffer, size);

	*newSize = size;
#endif


	return;
}


/*
 * OS9AttrToString()
 *
 * Returns textual representation of file attributes to standard output
 */
void OS9AttrToString(int attr_byte, char string[9])
{
	int i;
	char *attrs = "dsewrewr";

	/* print attributes */
	for (i = 0; i < 8; i++)
	{
		if (attr_byte & (1 << (7 - i)))
		{
			string[i] = attrs[i];
		}
		else
		{
			string[i] = '-';
		}
	}

	string[i] = '\0';
}

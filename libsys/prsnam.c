/********************************************************************
 * $Id$
 *
 * Parses the passed filename to see if it is legal.
 ********************************************************************/
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <cocopath.h>
#include <cocotypes.h>
#include <os9module.h>


error_code _os9_prsnam(const char *filename)
{
	const char *a = filename;
	int length = 0;

	while (*a != 0)
	{
		length++;
		if (length > D_NAMELEN)
		{
			return (EOS_BPNAM);
		}
		if (!
		    (isalnum((unsigned char)*a) || *a == '_' || *a == '.' || *a == '-'
		     || *a == ' ' || *a == '$'))
		{
			return EOS_BPNAM;
		}
		a++;
	}

	return 0;
}



error_code _decb_prsnam(const char *filename)
{
	error_code ec = 0;

	/* 1. Check if filename contains a dot */
	const char *dot = strchr(filename, '.');

	if (dot != NULL)
	{
		if (dot - filename > 8 || strlen(dot + 1) > 3)
		{
			ec = EOS_BPNAM;
		}
	}
	else
	{
		/* Filename with no extension - just check filename length */
		if (strlen(filename) > 8)
		{
			ec = EOS_BPNAM;
		}
	}

	return ec;
}

error_code _cecb_prsnam(const char *filename)
{
	error_code ec = 0;
	int i;

	if (strlen(filename) > 8)
	{
		ec = EOS_BPNAM;
	}
	else
	{
		for (i = 0; i < strlen(filename); i++)
		{
			if (isgraph((unsigned char)filename[i]) != 0)
				ec = EOS_BPNAM;
		}
	}

	return ec;
}

/********************************************************************
 * $Id$
 *
 * Functions common to all utilities
 ********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <util.h>
#include <cococonv.h>
#include <cocopath.h>
#include <cocotypes.h>


int StrToInt(char *s)
{
	int accum = 0;
	int i, t, power;

	switch (*s)
	{
	case '$':		/* Hex conversion */
		s++;
		t = strlen(s);
		for (i = 0; i < t; i++)
		{
			int c;

			c = toupper((unsigned char)s[i]);
			if (c >= 'A' && c <= 'F')
			{
				c -= 7;
			}

			c -= 48;
			power = pow(16, t - i - 1);

			accum += c * power;
		}
		break;

	case '%':		/* Binary conversion */
		s++;
		t = strlen(s);
		for (i = 0; i < t; i++)
		{
			int c;

			c = s[i];
			c -= 48;
			power = pow(2, t - i - 1);
			accum += c * power;
		}
		break;

	case '0':		/* Octal conversion */
		s++;
		t = strlen(s);
		for (i = 0; i < t; i++)
		{
			int c;

			c = s[i];
			c -= 48;

			power = pow(8, t - i - 1);
			accum += c * power;
		}
		break;


	default:		/* Decimal conversion */
		accum = atoi(s);
		break;
	}

	return (accum);
}

int strendcasecmp(const char *s1, const char *s2)
{
	int a, b;

	a = strlen(s1);
	b = strlen(s2);

	if (a < b)
		return -1;

	return strcasecmp(s1 + a - b, s2);
}


void show_help(char const *const *helpMessage)
{
	char const *const *p = helpMessage;

	while (*p)
	{
		fprintf(stderr, "%s", *(p++));
	}

	return;
}

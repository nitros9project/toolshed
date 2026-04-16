/********************************************************************
 * bitmap.c - OS-9 Bitmap routines
 *
 * $Id$
 ********************************************************************/

#include <stdlib.h>
#include <string.h>

#include "cocotypes.h"
#include "os9path.h"


/* Allocate a bit from the bitmap for numbits, starting at firstbit
 *
 * Note: range checking isn't done here; it is assumed that the caller
 * has checked the bounds before calling.
 */

int _os9_allbit(u_char * bitmap, int firstbit, int numbits)
{
	error_code ec = 0;
	int i, startbyte, startbit;


	startbyte = firstbit / 8;	/* compute start byte */
	startbit = firstbit % 8;	/* and start bit */

	for (i = 0; i < numbits; i++)
	{
		if (startbit > 7)
		{
			startbit = 0;	/* reset bit counter */
			startbyte++;	/* and increase byte counter */
		}

		bitmap[startbyte] |= (1 << (7 - startbit++));
	}

	return (ec);
}



/* Deallocate a bit from the bitmap for numbits, starting at firstbit
 *
 * Note: range checking isn't done here; it is assumed that the caller
 * has checked the bounds before calling.
 */

int _os9_delbit(u_char * bitmap, int firstbit, int numbits)
{
	error_code ec = 0;
	int i, startbyte, startbit;


	startbyte = firstbit / 8;	/* compute start byte */
	startbit = firstbit % 8;	/* and start bit */

	for (i = 0; i < numbits; i++)
	{
		if (startbit > 7)
		{
			startbit = 0;	/* reset bit counter */
			startbyte++;	/* and increase byte counter */
		}

		bitmap[startbyte] &= ~(1 << (7 - startbit++));
	}


	return ec;
}



/* Return state of bit representing LSN
 *
 * Note: range checking isn't done here; it is assumed that the caller
 * has checked the bounds before calling.
 */

int _os9_ckbit(u_char * bitmap, int bitnumber)
{
	int startbyte, startbit;


	startbyte = bitnumber / 8;
	startbit = bitnumber % 8;


	return bitmap[startbyte] & (1 << (7 - startbit));
}



/* Return free bit */

int _os9_getfreebit(u_char * bitmap, int total_sectors)
{
	int i;


	for (i = 2; i < total_sectors; i++)
	{
		if (!_os9_ckbit(bitmap, i))
		{
			/* bit is clear, cluster is free */

			_os9_allbit(bitmap, i, 1);	/* allocate cluster */

			return i;	/* return offset */
		}
	}

	return -1;
}



/* Get segment of SAS sectors
 *
 * Note: if SAS is less than a multiple of cluster size, the multiple will
 * be returned.
 *
 * cluster = cluster starting number
 * size    = number of clusters allocated
 */

error_code _os9_getSASSegment(os9_path_id path, int *cluster, int *size)
{
	int is_osk = (memcmp(path->lsn0->dd_sync, "Cruz", 4) == 0);

	unsigned int pd_sas = is_osk ? int2(path->lsn0->dd_opt.m68k.pd_sas)
				     : int1(path->lsn0->dd_opt.m6809.pd_sas);

	unsigned int pd_tot = int3(path->lsn0->dd_tot);
	u_int i, count;
	u_int prev_free;

	/* Sanity check pd_sas */
	if (pd_sas < 1 || pd_sas > (pd_tot / 2))
	{
		pd_sas = 1;

		if (is_osk)
			_int2(pd_sas, path->lsn0->dd_opt.m68k.pd_sas);
		else
			_int1(pd_sas, path->lsn0->dd_opt.m6809.pd_sas);
	}

	/* Adjust pd_sas so that it is at least a multiple of the
	 * cluster size
	 */

	pd_sas = NextHighestMultiple(pd_sas, path->spc);


	/* Now go and find pd_sas number of contiguous clusters */

	prev_free = 0;		/* to reassure compiler */
	i = count = 0;

	while (count < (pd_sas / path->spc))
	{
		if (i == pd_tot)
		{
			return -1;	/* none found */
		}

		/* Check if same allocation map sector (6x09 RBF limitation) */

		if (count != 0 && i / 2048 != prev_free / 2048)
		{
			/* Restart counting from here if free */
			count = 0;
		}

		if (!_os9_ckbit(path->bitmap, i))
		{
			/* Bit is clear */

			count += 1;
			prev_free = i;
		}
		else
		{
			count = 0;
		}
		i++;
	}

	*cluster = (i - count) * path->spc;
	*size = count * path->spc;

	_os9_allbit(path->bitmap, i - count, count);


	return 0;
}



/* Round up value to the next highest multiple of multiple */

unsigned int NextHighestMultiple(unsigned int value, unsigned int multiple)
{
	return (value / multiple + (value % multiple != 0)) * multiple;
}

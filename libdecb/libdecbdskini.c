/********************************************************************
 * libdecbdskini.c - Disk BASIC Format Routine
 *
 * $Id$
 ********************************************************************/
#include <util.h>
#include <string.h>
#include <decbpath.h>
#include <nativepath.h>
#include <cocotypes.h>
#include <diskimage.h>


#define MAX_BPS 			256
/* How far apart Disk BASIC's sectors are on a new DMK or SDF track. */
#define DSKINI_INTERLEAVE	5

#define MAX_DISKNAME_LEN	9	// the maximum number of characters an HDB-DOS disk name can be

/* Where the sectors of a new disk go: an image's sectors, or a native file. */
typedef struct
{
	diskimage_id disk;
	native_path_id native;
	long long slot;
} dskini_output;


static error_code put_sector(dskini_output *out, const char *sector, int bps)
{
	u_int size = bps;
	error_code ec;

	if (out->disk == NULL)
	{
		return _native_write(out->native, (char *) sector, &size);
	}

	ec = _diskimage_write_sector(out->disk, (u_int) (out->slot / 18), 0, (u_int) (out->slot % 18) + 1, sector, bps);
	out->slot++;

	return ec == EOS_IA ? EOS_SECT : ec;
}


error_code _decb_dskini(char *vdisk, int tracks, char *diskName,
			int hdbdrives, int bps, int skitzo)
{
	return _decb_dskini_format(vdisk, DISKIMAGE_UNKNOWN, tracks, diskName, hdbdrives, bps, skitzo);
}


error_code _decb_dskini_format(char *vdisk, diskimage_format image_format, int tracks, char *diskName,
			       int hdbdrives, int bps, int skitzo)
{
	error_code ec = 0;
	dskini_output out;
	diskimage_format format = DISKIMAGE_UNKNOWN;
	int max_s, i;
	char sector[MAX_BPS];

	if (diskName && strlen(diskName) > MAX_DISKNAME_LEN)
	{
		return EOS_BPNAM;
	}

	memset(&out, 0, sizeof(out));

	/* Only a raw image holds many drives. */

	if (hdbdrives > 1 && image_format != DISKIMAGE_UNKNOWN && image_format != DISKIMAGE_RAW)
	{
		return EOS_BPNAM;
	}

	/* 1. Make a new disk in the format asked for, or check it is the one that's there. */

	if (image_format != DISKIMAGE_UNKNOWN)
	{
		ec = _diskimage_identify(vdisk, &format);

		if (ec == EOS_PNNF)
		{
			if (image_format != DISKIMAGE_RAW)
			{
				ec = _diskimage_create(vdisk, image_format, 1, tracks, bps, 18, 0, DSKINI_INTERLEAVE, 0xFF);

				if (ec != 0)
				{
					return (ec);
				}
			}
		}
		else if (ec != 0)
		{
			return (ec);
		}
		else if (format != image_format && !(image_format == DISKIMAGE_JVC && format == DISKIMAGE_RAW))
		{
			return EOS_FAE;
		}

		ec = 0;
		format = DISKIMAGE_UNKNOWN;
	}

	/* 2. Open a path to the virtual disk. */

	if (_diskimage_identify(vdisk, &format) == 0 && format != DISKIMAGE_RAW && format != DISKIMAGE_UNKNOWN)
	{
		if (hdbdrives > 1)
		{
			return EOS_BPNAM;
		}

		ec = _diskimage_open(&out.disk, vdisk, FAM_READ | FAM_WRITE);

		if (ec != 0)
		{
			return (ec);
		}
	}
	else
	{
		ec = _native_open(&out.native, vdisk, FAM_WRITE);

		if (ec != 0)
		{
			ec = _native_create(&out.native, vdisk, FAM_READ | FAM_WRITE,
					    FAP_READ | FAP_WRITE);

			if (ec != 0)
			{
				return (ec);
			}
		}

		_native_seek(out.native, 0, SEEK_SET);
	}


	for (i = 0; i < hdbdrives && ec == 0; i++)
	{
		/* 2. Write 17 tracks of $FF */

		memset(sector, 0xFF, bps);

		{
			int t, s;

			for (t = 0; t < 17; t++)
			{
				for (s = 1; s < 19; s++)
				{
					if (ec == 0)
					{
						ec = put_sector(&out, sector, bps);
					}
				}
			}
		}


		/* 3. Write directory track. */

		memset(sector, 0x00, bps);

		{
			int s;
			u_int min_s = 0;


			/* 1. Write sector of track 17 (all 0s..). */

			if (ec == 0)
			{
				ec = put_sector(&out, sector, bps);
			}


			/* 2. Write FAT sector. */

			switch (tracks)
			{
			case 40:
				max_s = 78;
				break;

			case 80:
				max_s = 156;
				break;

			case 35:
			default:
				max_s = 68;
				break;
			}

			/* Process skitzo here -- we set the first 34 granules as allocated. */

			if (skitzo == 1)
			{
				min_s = 34;
			}


			for (s = min_s; s < max_s; s++)
			{
				sector[s] = 0xFF;
			}


			if (ec == 0)
			{
				ec = put_sector(&out, sector, bps);
			}


			/* 3. Write 14 sectors of 0xFF. */

			memset(sector, 0xFF, bps);

			for (s = 0; s < 14; s++)
			{
				if (ec == 0)
				{
					ec = put_sector(&out, sector, bps);
				}
			}


			/* 4. If disk name was provided, copy it to sector. */

			if (diskName != NULL)
			{
				strcpy(sector, diskName);
			}


			/* 5. Write 17th sector. */

			if (ec == 0)
			{
				ec = put_sector(&out, sector, bps);
			}


			memset(sector, 0xFF, bps);


			/* 6. Write 18th sector. */

			if (ec == 0)
			{
				ec = put_sector(&out, sector, bps);
			}
		}


		/* 5. Write remaining 17, 22 or 62 tracks of $FF. */

		memset(sector, 0xFF, bps);

		{
			int t, s;

			for (t = 0; t < tracks - 18; t++)
			{
				for (s = 1; s < 19; s++)
				{
					if (ec == 0)
					{
						ec = put_sector(&out, sector, bps);
					}
				}
			}
		}
	}


	if (out.disk != NULL)
	{
		error_code close_ec = _diskimage_close(out.disk);

		if (ec == 0)
		{
			ec = close_ec;
		}
	}
	else
	{
		_native_close(out.native);
	}


	return (ec);
}

/*
 *------------------------------------------------------------------
 *
 * $Source$
 * $RCSfile$
 * $Revision$
 * $Date$
 * $State$
 * $Author$
 * $Locker$
 *
 *------------------------------------------------------------------
 *
 * Carl Kreider
 * 22305 CR 28
 * Goshen, IN  46526
 * (219) 875-7019
 * (71076.76@compuserve.com, ckreider@skyenet.net, carlk@syscon-intl.com)
 *
 *------------------------------------------------------------------
 * $Log$
 * Revision 1.9  2008/11/03 15:19:56  robertgault
 * added WIN32 compensation for SYSV
 *
 * Revision 1.8  2008/10/30 15:52:24  boisy
 * Clenaed up warnings in ar2
 *
 * Revision 1.7  2008/10/30 03:08:48  boisy
 * Additional updates
 *
 * Revision 1.6  2007/10/06 06:27:24  tlindner
 * Updated for CYGWIN, Should not have broken anything. :)
 *
 * ----------------------------------------------------------------------
 *
 * Revision 1.5  2006/09/09 01:59:03  boisy
 * Changes to accomodate compiling under Turbo C++
 *
 * Revision 1.4  2005/09/18 21:45:14  boisy
 * Testing indent
 *
 * Revision 1.3  2005/09/16 23:46:35  boisy
 * ar2 now makes under OS X
 *
 * Revision 1.2  1996/07/20 22:23:02  cc
 * Merged in pwz's unixification (Sunos).
 *
 * Revision 1.1  96/07/20  17:10:36  cc
 * Initial revision
 * 
 *------------------------------------------------------------------
 */

/*
 * System dependent stuff is isolated here (mostly)
 *
 * Also includes some functions that are in my library but not in
 *  Microware's.
 */

#include <stdlib.h>
#include <string.h>
#if defined(SYSV)
# include "o2u.h"
# include <sys/time.h>
#ifndef WIN32
# include <pwd.h>
#endif
# include "o2u.h"
#else /* SYSV */
# ifdef  OSK
#  include <os9.h>
# endif
#endif /* SYSV */

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include "ar.h"


/*
 * If your canned tolower & toupper don't test first 
 * You can use these or can your own
 *  Cut me a break it was late and I'm still writing C'tran
 *  (That's Fortran appended with semicolons)
 */

#ifdef BRAINDEAD
ck_tolower(char ch)
	{
	if (isupper(ch))
		tolower(ch);

	return (ch);
	}


ck_toupper(char ch)
	{
	if (islower(ch))
		toupper(ch);

	return (ch);
	}

#undef toupper
# define toupper ck_toupper
#endif


/*
 *  convert a long read from disk as an array of char
 *   back to a long.
 */

long c4tol(char *s)
	{
	long	x = 0;

	x = (x + (*s++ & 0xff)) << 8;
	x = (x + (*s++ & 0xff)) << 8;
	x = (x + (*s++ & 0xff)) << 8;
	x = (x + (*s & 0xff));
	return (x);
	}
/*page*/
#ifdef SYSV
/* Test file for DIR status */
 
int is_dir(int pn)
	{
	struct stat	stbuf;

	fstat(pn, &stbuf);

	if (S_IFDIR == (stbuf.st_mode & S_IFMT))
		return (1);
	else
		return 0;
	}
#endif


/*
 * get file stats using _os9 for portability
 */

void get_fstat(int pn, FILDES *fs)
	{
#ifdef SYSV
	register char	*p;
	int				i;
	short			s;
	register long	l;
	struct stat		stbuf;

	fstat(pn, &stbuf);
	fs->fd_attr = u2oFmode(stbuf.st_mode);
	for (s = stbuf.st_uid, p = &fs->fd_own[1], i = 0; i < 2; i++, p--, s >>= 8)
		*p = (s & 0xff);

	memcpy(fs->fd_date, u2oDate(stbuf.st_mtime), 5);
	fs->fd_link = (stbuf.st_nlink & 0xff);
	for (l = stbuf.st_size, p = &fs->fd_fsize[3], i = 0; i < 4; i++, p--, l >>= 8)
		*p = (l & 0xff);

	memcpy(fs->fd_dcr, u2oDate(stbuf.st_ctime), 3);

#else
# ifdef OSK
	_gs_gfd(pn, fs, sizeof(FILDES));
# else
#  ifdef CKLIB
	getstat(SS_FD, pn, fs, sizeof(FILDES));
#  endif /* CKLIB */
# endif /* OSK */
#endif /* SYSV */
	}
/*page*/
/*
 * set file attributes
 */

void set_fstat(char *pn, FILDES *fs)
	{
#if defined(SYSV)
	short			mode = o2uFmode(fs->fd_attr);
#if !defined(WIN32)
	char			*p = fs->fd_own;
	short			s;
	struct passwd	*pwdbuf;
#endif
/*
	struct  {
		long	a, m;
		} ubuf;
*/

#if !defined(WIN32)
	s = (*p++&0xff);
	s <<= 8;
	s |= (*p & 0xff);
	pwdbuf = getpwuid(s);
#endif
	chmod(pn, mode);
#if !defined(WIN32)
	chown(pn, s, pwdbuf ? pwdbuf->pw_gid : s);
#endif

	struct utimbuf ubuf;
	ubuf.actime = time(NULL);
	ubuf.modtime = o2uDate(fs->fd_date);
	utime(pn, &ubuf);
#else
# ifdef OSK
	_ss_pfd(pn, fs, sizeof(FILDES));
	_ss_attr(pn, fs->fd_attr);
# else
#  ifdef CKLIB
	setstat(SS_FD, pn, fs, sizeof(FILDES));
	setstat(SS_ATTR, pn, fs->fd_attr);
#  endif /* CKLIB */
# endif /* OSK */
#endif /* SYSV */
	}
/*page*/
/*
 * get the file size
 */

long get_fsize(int pn)
	{
#ifdef SYSV
	struct stat	stbuf;

	fstat(pn, &stbuf);
	return(stbuf.st_size);
#else
	long	size;

# ifdef OSK
	size = _gs_size(pn);
# else
	getstat(SS_SIZE, pn, &size);
# endif
	return (size);
#endif /* SYSV */
   }


/*
 * change the file size
 */

void set_fsize(int pn, long size)
   {
#ifdef SYSV
	ftruncate(pn, size);  /* If you don't have this or something akin, deletes don't work well */
#else
# ifdef   OSK
	_ss_size(pn, size);
# else
	setstat(SS_SIZE, pn, size);
# endif
#endif /* SYSV */
   }
/*page*/
/*+
 * assureDir
 *
 *   Make sure that the given directory exists.
 *
 *   assureDir(path)
 *
 *   char   *path;      The directory that should exist
 */

int assureDir(char *path)
	{
#ifdef SYSV
	char	cmd[80];

	/* if is isn't there	*/
	if (access(path, 0))
		{
		/* then make it	*/
		sprintf(cmd, "mkdir %s", path);
		if (system(cmd))
			return (-1);
		}
#else
	if (access(path, 0x81) == -1)
		if (mknod(path, 0xbf) == -1)  			        /* try to make it	*/
			return (-1);
#endif

	return (0);
	}
/*page*/
/*
 * If you have the proper header then use <filehdr.h>
 * You will want to mung isobject() below also
 */

#ifdef HAVE_FILEHDR
# include <filehdr.h>
#else
# include "filehdr.h"
#endif


int isobject(FILE *input)
	{
	unsigned short	x;

	read(fileno(input), &x, 2);
#ifdef SYSV
  	if (x == SUN4MAGIC || x == DOSMAGIC)
#else
	if (x == OS9MAGIC || x == OSKMAGIC)
#endif
		return (1);

	return (0);
	}
/*page*/
/*
 * function to write a long in a machine independent manner
 */

int writelong(FILE *fp, long l)
	{
	if (putc((int)((l >> 24) & 0xff), fp) != EOF)
		if (putc((int)((l >> 16) & 0xff), fp) != EOF)
			if (putc((int)((l >> 8) & 0xff), fp) != EOF)
				if (putc((int)(l & 0xff), fp) != EOF)
					return (0);

	return (EOF);
	}


/*
 * function to write a short in a machine independent manner
 */

int writeshort(FILE *fp, short s)
	{
	if (putc((s >> 8) & 0xff, fp) != EOF)
		if (putc(s & 0xff, fp) != EOF)
			return (0);

	return (EOF);
	}
/*page*/
/*
 * function ot read a long in a machine independent manner
 */

int readlong(FILE *fp, long *lp)
	{
	int		i;
	long	l = 0;

	if ((i = getc(fp)) != EOF)
		{
		l = i;
		if ((i = getc(fp)) != EOF)
			{
			l = (l << 8) | i;
			if ((i = getc(fp)) != EOF)
				{
				l = (l << 8) | i;
				if ((i = getc(fp)) != EOF)
					{
					l = (l << 8) | i;
					*lp = l;
					return (0);
					}
				}
			}
		}

	return (EOF);
	}


/*
 * function ot read a short in a machine independent manner
 */

int readshort(FILE *fp, short *sp)
	{
	int		i;
	short	s = 0;

	if ((i = getc(fp)) != EOF)
		{
		s = i;
		if ((i = getc(fp)) != EOF)
			{
			s = (s << 8) | i;
			*sp = s;
			return (0);
			}
		}

	return (EOF);
	}

/*
 * function ot read an unsigned short in a machine independent manner
 */

int readushort(FILE *fp, unsigned short *sp)
	{
	int		i;
	unsigned short	s = 0;

	if ((i = getc(fp)) != EOF)
		{
		s = i;
		if ((i = getc(fp)) != EOF)
			{
			s = (s << 8) | i;
			*sp = s;
			return (0);
			}
		}

	return (EOF);
	}

/*page*/
#ifndef CKLIB
/*
 *      Returns true if string s matches pattern p.
 */

#define ifup(a)		(f ? toupper(a) : a)

int patmatch(char *p, char *s, int f)
	{
	char	pc,							/* a single character from pattern	*/
			sc;							/* a single character from string	*/
	int		found, compl;

	while ((pc = ifup(*p++)))
		{
		if (pc == '*')
			{
			do {						/* look for match till s exhausted	*/
				if (patmatch(p, s, f))
					return (1);

				} while (*s++);

			return (0);
			}
		else
			if (*s == 0)
				return (0);							/* s exhausted, p not	*/
			else
				if (pc == '?')
					s++;						/* matches all, just bump	*/
				else
					if (pc == '[')						/* character class	*/
						{
						sc = ifup(*s++);
						if ((compl = (*p == '^')))		/* class inversion?	*/
							p++;
						found = 0;
						while ((pc = ifup(*p++)) != ']')
							if (pc == 0)				/* check for end	*/
								{					/* no terminating ']'	*/
								p--;
								break;
								}
							else
								if (*p == '-')			/* check for range	*/
									{
									++p;
									found |= ((pc <= sc) && (sc <= ifup(*p++)));
									}
								else
									found |= (pc == sc);

						if (!found ^compl)
							return (0);
						}
					else
						if (pc != ifup(*s++))
							return (0);
		}

	return (*s == 0);				/* p exhausted, ret true if s exhausted	*/
	}
#endif
/*page*/
#ifndef CKLIB
# ifndef SYSV
/*
 * Set an array of n chars starting at s to the character c.
 * Return s.
 */

char			*memset(s, c, n)
register char	*s, c;
int				n;
	{
	char	*os = s;

	while (n-- > 0)
		*s++ = c;

	return (os);
	}


/*
 * special strcmp to ignore case
 */

strucmp(s1, s2)
char			*s1;
register char	*s2;
	{
	while (toupper(*s1) == toupper(*s2))
		{
		if (*s2++ == 0)
			return (0);

		s1++;
		}

	return (toupper(*s1) - toupper(*s2));
	}

# endif
#endif
/*page*/
#ifdef SYSV
#if 0
# include <sys/types.h>
# include <sys/param.h>
# include <sys/dir.h>

#define DIRECT  struct direct

DIR		*opendir(name)
char	*name;
	{
	register DIR	*dirp;
	register int	fd;
	char			*malloc();

	if ((fd = open(name, 0)) == -1)
		return (0);

	if ((dirp = (DIR *) malloc(sizeof(DIR))) == 0)
		{
		close(fd);
		return (0);
		}

	dirp->dd_fd = fd;
	dirp->dd_loc = 0;
	return (dirp);
	}


DIRECT			*readdir(dirp)
register DIR	*dirp;
	{
	register DIRECT	*dp;
	static char		de[sizeof(DIRECT) + 1];

	for (; ; )
		{
		if (dirp->dd_loc == 0)
			{
			dirp->dd_size = read(dirp->dd_fd, dirp->dd_buf, BUFSIZ);
			if (dirp->dd_size <= 0)
				return (0);
			}

		if (dirp->dd_loc >= dirp->dd_size)
			{
			dirp->dd_loc = 0;
			continue;
			}

		dp = (DIRECT *)(dirp->dd_buf + dirp->dd_loc);
		dirp->dd_loc += sizeof(DIRECT);
		if (dp->d_ino)
			{
			/*
			 * This is rude, but is the best I can do on short notice.
			 *  Problem is, the method of using strncpy upstairs will
			 *  fail on a 14 character name, because the name will not
			 *  get null terminated.  The upstairs code relys on getting
			 *  a 'struct direct *' back.
			 */
			memcpy(de, dp, sizeof(DIRECT));			/* copy to larger spot	*/
			de[sizeof(DIRECT)] = '\0';				/* terminate the name	*/
			return ((DIRECT *) de);
			}
		}
	}


closedir(dirp)
register DIR	*dirp;
	{
	close(dirp->dd_fd);
	dirp->dd_fd = -1;
	dirp->dd_loc = 0;
	free(dirp);
	return (0);
	}

#endif
#else
# ifndef OSK
#  ifndef CKLIB

#include <dir.h>
long  lseek();

/*
 * routine to close up and tidy up
 */

closedir(dirp)
DIR		*dirp;
	{
	close(dirp->dd_fd);
	free(dirp);
	}


/*
 * routine to open a directory and set up the data structures
 */

DIR		*opendir(name)
char	*name;
	{
	DIR		*dirp = (DIR *) 0;

	if ((dirp = malloc(sizeof(DIR))) != 0)
		if ((dirp->dd_fd = open(name, 0x81)) < 0)
			{
			free(dirp);
			dirp = (DIR *) 0;
			}

	return (dirp);
	}


/*
 * routine to return the next directory entry
 *  this could be buffered, but I don't want to
 *  mess with sorting out the seeks
 */

struct direct	*readdir(dirp)
DIR				*dirp;
	{
	static struct direct	de;

	do	{
		if (read(dirp->dd_fd, dirp->dd_buf, 32) <= 0)
			return (0);

		} while (dirp->dd_buf[0] == '\0');

	strhcpy(de.d_name, dirp->dd_buf);
	de.d_addr = ((dirp->dd_buf[29] & 0xff) << 8) + (dirp->dd_buf[30] & 0xff);
	de.d_addr = (de.d_addr << 8) + (dirp->dd_buf[31] & 0xff);
	return (&de);
	}
/*page*/
/*
 * routine to seek to a specific location
 */

seekdir(dirp, loc)
DIR		*dirp;
long	loc;
	{
	lseek(dirp->dd_fd, loc, 0);
	}


/*
 * routine to return the current position in a directory
 */

long	telldir(dirp)
DIR		*dirp;
	{
	return (lseek(dirp->dd_fd, 0L, 1));
	}

#  endif
# endif
#endif

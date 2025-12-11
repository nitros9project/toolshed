/********************************************************************
 * cocotypes.h - CoCoTools type definitions
 *
 * $Id$
 ********************************************************************/
#ifndef	_COCOTYPES_H
#define	_COCOTYPES_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#if !defined(sun)
#include <stdint.h>
#endif

#include <sys/types.h>

#ifdef  WIN32
typedef unsigned char u_char;
typedef unsigned int u_int;
#else
#if !defined(__u_char_defined) && !defined(__APPLE__) && !defined(sun)
typedef unsigned char u_char;
typedef unsigned int u_int;
#endif

#endif
typedef int error_code;


/* necessary byte order conversion prototypes */
unsigned int int4(u_char *a);
unsigned int int3(u_char *a);
unsigned int int2(u_char *a);
unsigned int int1(u_char *a);

void _int4(unsigned int a, u_char *b);
void _int3(unsigned int a, u_char *b);
void _int2(unsigned int a, u_char *b);
void _int1(unsigned int a, u_char *b);

unsigned short swap_short(unsigned short in);
unsigned int swap_int(unsigned int in);

extern int decb_granule_count;

/* Read multibyte values stored in little endian format in file */
size_t fread_le_char( unsigned char *ptr, FILE *stream );
size_t fread_le_short( unsigned short *ptr, FILE *stream );
size_t fread_le_sshort( signed short *ptr, FILE *stream );
size_t fread_le_int( unsigned int *ptr, FILE *stream );
size_t fread_le_sint( int *ptr, FILE *stream );
size_t fread_le_24bit(signed int *ptr, FILE *stream);
size_t fread_le_float(double *ptr, FILE *stream);
size_t fread_le_double(double *ptr, FILE *stream);
size_t fwrite_le_int(unsigned int data, FILE * stream);
size_t fwrite_le_short(unsigned short data, FILE * stream);
size_t fwrite_le_char(unsigned char data, FILE * stream);

#ifdef __cplusplus
}
#endif

#endif /* _COCOTYPES_H */

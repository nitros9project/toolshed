# Makefile for librbf.a
include ../rules.mak

vpath %.c ../../../librbf

CFLAGS	+= -I../../../include -Wall

%.a:
	$(AR) -r $@ $^
	$(RANLIB) $@

librbf.a:	librbfbitmap.o librbfmakdir.o librbfread.o librbfrename.o librbfss.o librbfdelete.o \
		librbfgs.o librbfopen.o librbfreadln.o librbfseek.o librbfwrite.o librbfformat.o

clean:
	$(RM) *.o *.a

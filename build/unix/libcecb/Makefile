# Makefile for libcecb.a
include ../rules.mak

vpath %.c ../../../libcecb

CFLAGS	+= -I../../../include -Wall

%.a:
	$(AR) -r $@ $^
	$(RANLIB) $@

libcecb.a:	libcecbbulkerase.o libcebcopen.o libcecbgs.o libcecbwav.o \
                libcecbcas.o libcecbread.o libcecbwrite.o libcecbtokenize.o \
                libcecbseek.o

clean:
	$(RM) *.o *.a

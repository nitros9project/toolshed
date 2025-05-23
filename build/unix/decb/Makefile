# Makefile for decb
include ../rules.mak

vpath %.c ../../../decb ../../../os9

CFLAGS	+= -I../../../include -Wall
LDFLAGS	+= -L../libtoolshed -L../libcoco -L../libnative -L../libcecb -L../librbf -L../libdecb -L../libmisc -L../libsys -ltoolshed -lcoco -lnative -lcecb -lrbf -ldecb -lmisc -lsys -lm

decb$(SUFEXE):	decb_main.o decbattr.o decbcopy.o decbdir.o decbdskini.o decbfree.o decbfstat.o \
	decbhdbconv.o decbkill.o decblist.o decbrename.o os9dump.o decbdsave.o os9dsave.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o decb decb.exe

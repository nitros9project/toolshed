# Makefile for cecb
include ../rules.mak

vpath %.c ../../../cecb ../../../os9

CFLAGS	+= -I../../../include -Wall
LDFLAGS	+= -L../libtoolshed -L../libcoco -L../libcecb -L../libnative -L../librbf -L../libdecb -L../libmisc -L../libsys -ltoolshed -lcoco -ldecb -lcecb -lnative -lrbf -lmisc -lsys -lm

cecb$(SUFEXE):	cecbbulkerase.o cecbdir.o cecbfstat.o cecb_main.o cecbcopy.o ../os9/os9dump.o ../decb/decblist.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o cecb cecb.exe

# Makefile for cecb
include ../rules.mak

vpath %.c ../../../cecb ../../../os9

CFLAGS	+= -I../../../include -Wall
LDFLAGS	+= -L../libtoolshed -L../libcoco -L../libcecb -L../libnative -L../librbf -L../libdecb -L../libmisc -L../libsys -ltoolshed -lcoco -ldecb -lcecb -lnative -lrbf -lmisc -lsys -lm

cecb$(SUFEXE):	cecbbulkerase.o cecbdir.o cecbfstat.o cecb_main.o cecbcopy.o cecbleader.o ../os9/os9dump.o ../decb/decblist.o \
		../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libcecb/libcecb.a ../libnative/libnative.a \
		../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a ../libsys/libsys.a 
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o cecb cecb.exe

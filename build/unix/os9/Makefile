# Makefile for os9
include ../rules.mak

vpath %.c ../../../os9

LDFLAGS	+= -L../libtoolshed -L../libcecb -L../libcoco -L../libnative -L../libdecb -L../libmisc -L../librbf -L../libsys -ltoolshed -lcoco -lnative -ldecb -lcecb -lmisc -lrbf -lsys -lm

os9$(SUFEXE):	os9copy.o os9dsave.o os9gen.o os9modbust.o os9dcheck.o os9dump.o \
	os9id.o os9padrom.o os9_main.o os9del.o os9format.o os9ident.o \
	os9rename.o os9attr.o os9deldir.o os9free.o os9list.o os9cmp.o \
	os9dir.o os9fstat.o os9makdir.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o os9 os9.exe

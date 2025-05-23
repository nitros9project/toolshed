# Makefile for mamou
include ../rules.mak

vpath %.c ../../../mamou

DEBUG	= -g
CFLAGS	+= -DLINUX -I../../../include $(DEBUG) -Wall
LDFLAGS	+= -L../libcoco -L../libnative -L../libcecb -L../libdecb -L../librbf -L../libmisc -L../libsys -lcoco -lnative -ldecb -lcecb -lrbf -lmisc -lsys -lm $(DEBUG)

mamou$(SUFEXE):		mamou_main.o evaluator.o pseudo.o h6309.o ffwd.o \
		print.o util.o symbol_bucket.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o mamou mamou.exe

install: mamou
	cp mamou $(HOME)/bin

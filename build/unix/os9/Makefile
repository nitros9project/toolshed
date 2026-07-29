# Makefile for os9
include ../rules.mak

vpath %.c ../../../os9

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = os9copy.c os9dsave.c os9gen.c os9modbust.c os9dcheck.c os9dump.c \
	os9id.c os9padrom.c os9_main.c os9del.c os9format.c os9ident.c \
	os9rename.c os9attr.c os9deldir.c os9free.c os9list.c os9cmp.c \
	os9dir.c os9fstat.c os9makdir.c os9reveal.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libnative/libnative.a \
	../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

os9$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d os9 os9.exe

-include $(DEPS)

# Makefile for dgndos
include ../rules.mak

vpath %.c ../../../dgndos

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = dgndos_main.c dgndosattr.c dgndoscopy.c dgndosdir.c dgndosdskinit.c dgndosfree.c \
	dgndosfstat.c dgndoskill.c dgndoslist.c dgndosrename.c dgndosdsave.c dgndosreveal.c

OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libdragondos/libdragondos.a ../libdiskimage/libdiskimage.a \
	../libnative/libnative.a ../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

dgndos$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d dgndos dgndos.exe

-include $(DEPS)

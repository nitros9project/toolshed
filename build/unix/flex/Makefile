# Makefile for flex
include ../rules.mak

vpath %.c ../../../flex

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = flex_main.c flexattr.c flexcopy.c flexdir.c flexdskinit.c flexdsave.c flexfree.c \
	flexfstat.c flexgen.c flexkill.c flexlist.c flexrename.c

OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libdragondos/libdragondos.a ../libflex/libflex.a ../libdiskimage/libdiskimage.a \
	../libnative/libnative.a ../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

flex$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d flex flex.exe

-include $(DEPS)

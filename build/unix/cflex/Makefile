# Makefile for cflex
include ../rules.mak

vpath %.c ../../../cflex

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = cflex_main.c cflexutil.c cflexattr.c cflexcopy.c cflexdir.c cflexdskinit.c cflexdsave.c cflexfree.c \
	cflexfstat.c cflexgen.c cflexkill.c cflexlist.c cflexrename.c

OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libdragondos/libdragondos.a ../libflex/libflex.a ../libdiskimage/libdiskimage.a \
	../libnative/libnative.a ../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

cflex$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d cflex cflex.exe

-include $(DEPS)

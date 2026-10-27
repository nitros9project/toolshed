# Makefile for decb
include ../rules.mak

vpath %.c ../../../decb ../../../os9

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = decb_main.c decbattr.c decbcopy.c decbdir.c decbdskini.c decbfree.c decbfstat.c \
	decbhdbconv.c decbkill.c decblist.c decbrename.c decbdsave.c decbbinbust.c \
	os9dsave.c
	
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libnative/libnative.a \
	../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

decb$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d decb decb.exe

-include $(DEPS)

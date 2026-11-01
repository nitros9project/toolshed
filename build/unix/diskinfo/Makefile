# Makefile for diskinfo
include ../rules.mak

vpath %.c ../../../diskinfo

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = diskinfo.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libdiskimage/libdiskimage.a ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a \
	../libnative/libnative.a ../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a \
	../libmisc/libmisc.a ../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

diskinfo$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d diskinfo diskinfo.exe

-include $(DEPS)

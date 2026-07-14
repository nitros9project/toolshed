# Makefile for cecb
include ../rules.mak

vpath %.c ../../../cecb ../../../os9

CFLAGS	+= -I../../../include -Wall -MMD -MP

SRCS = cecbbulkerase.c cecbdir.c cecbfstat.c cecb_main.c cecbcopy.c cecbleader.c \
	cecbdumpblock.c ../os9/os9dump.c ../decb/decblist.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libnative/libnative.a \
	../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

cecb$(SUFEXE): $(OBJS) $(LIBDEPS) 
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	$(RM) *.o *.d cecb cecb.exe

-include $(DEPS)

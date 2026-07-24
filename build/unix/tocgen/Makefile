include ../rules.mak

vpath %.c ../../../tocgen
vpath %.h ../../../tocgen

SRCS = tocgen_main.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

LIBDEPS = ../libtoolshed/libtoolshed.a ../libcoco/libcoco.a ../libnative/libnative.a \
	../libcecb/libcecb.a ../librbf/librbf.a ../libdecb/libdecb.a ../libmisc/libmisc.a \
	../libsys/libsys.a

CFLAGS  += -I../../../include -Wall
#LDFLAGS += -L../libcoco -L../libnative -L../libdecb -L../libmisc -L../librbf -L../libsys -L../libcecb -lcoco -lnative -lcecb -ldecb -lmisc -lrbf -lsys -lm
BINARY	= tocgen
OBJS	= tocgen_main.o

# Derive -L flags from the directory part of each entry in LIBDEPS
LDFLAGS += $(patsubst %,-L%,$(dir $(LIBDEPS))) \
           $(patsubst ../lib%/lib%.a,-l%,$(LIBDEPS)) \
           -lm

$(BINARY)$(SUFEXE): $(OBJS) $(LIBDEPS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS):	Makefile

clean:
	$(RM) $(BINARY) $(BINARY).exe *.o

install: $(BINARY)
	cp $(BINARY) $(HOME)/bin

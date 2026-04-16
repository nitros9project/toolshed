include ../rules.mak

BINARY = cocofuse

vpath %.c ../../../$(BINARY)

OS := $(shell uname)
ifeq ($(OS),Darwin)
CFLAGS	+= -I../../../include -Wall -DDEBUG
CFLAGS  += -F/Library/Frameworks
LDFLAGS	+= -L/usr/local/lib -L../libtoolshed -L../libcoco -L../libnative -L../libcecb -L../libdecb -L../libmisc -L../librbf -L../libsys -ltoolshed -lcoco -lnative -lcecb -ldecb -lrbf -lmisc -lsys -lm
LDFLAGS += -lfuse-t
else
CFLAGS	+= -I../../../include -Wall -DDEBUG
LDFLAGS	+= -L/usr/local/lib -L../libtoolshed -L../libcoco -L../libnative -L../libcecb -L../libdecb -L../libmisc -L../librbf -L../libsys -ltoolshed -lcoco -lnative -lcecb -ldecb -lrbf -lmisc -lsys -lm -lfuse
endif

$(BINARY):	$(BINARY).o
	-$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o $(BINARY)

include ../rules.mak

vpath %.c ../../../tocgen
vpath %.h ../../../tocgen

CFLAGS  += -I../../../include -Wall
LDFLAGS += -L../libcoco -L../libdragondos -L../libdiskimage -L../libnative -L../libdecb -L../libmisc -L../librbf -L../libsys -L../libcecb -lcoco -ldragondos -ldiskimage -lnative -lcecb -ldecb -lmisc -lrbf -lsys -lm
BINARY	= tocgen
OBJS	= tocgen_main.o


$(BINARY)$(SUFEXE):	$(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(DEBUG)

$(OBJS):	Makefile

clean:
	$(RM) $(BINARY) $(BINARY).exe *.o

install: $(BINARY)
	cp $(BINARY) $(HOME)/bin

include ../rules.mak

vpath %.c ../../../ar2
vpath %.h ../../../ar2

CFLAGS	+= -DSYSV

BINARY	= ar2
OBJS	= ar.o arsup.o lz1.o o2u.o

$(BINARY)$(SUFEXE):	$(OBJS)
	$(CC) $(OBJS) -o $@ $(DEBUG)

$(OBJS):

clean:
	$(RM) $(BINARY) $(BINARY).exe *.o

install: $(BINARY)
	cp $(BINARY) $(HOME)/bin

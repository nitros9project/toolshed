include ../rules.mak

vpath %.c ../../../makewav
vpath %.h ../../../makewav

# CFLAGS += -DDEBUG

BINARY	= makewav
OBJS	= makewav.o
LIBS	= -lm

$(BINARY)$(SUFEXE):	$(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $@

clean:
	$(RM) $(BINARY) $(BINARY).exe *.o

install: $(BINARY)
	cp $(BINARY) $(HOME)/bin

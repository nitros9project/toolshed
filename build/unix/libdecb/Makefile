# Makefile for libdecb.a
include ../rules.mak

CFLAGS += -I../../../include -Wall -MMD -MP

SRCS = libdecbgs.c libdecbkill.c libdecbopen.c libdecbread.c libdecbrename.c \
	libdecbseek.c libdecbss.c libdecbread.c libdecbwrite.c libdecbtokenize.c \
	libdecbbinconcat.c libdecbsrec.c libdecbdskini.c libdecbdisk.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

vpath %.c ../../../libdecb

libdecb.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o *.a *.d

-include $(DEPS)

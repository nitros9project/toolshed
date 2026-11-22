# Makefile for libflex.a
include ../rules.mak

CFLAGS += -I../../../include -Wall -MMD -MP

SRCS = libflexboot.c libflexchain.c libflexdir.c libflexdisk.c libflexdskinit.c libflexkill.c libflexopen.c \
	libflexread.c libflextext.c libflexwrite.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

vpath %.c ../../../libflex

libflex.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o *.a *.d

-include $(DEPS)

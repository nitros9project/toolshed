# Makefile for libdragondos.a
include ../rules.mak

CFLAGS += -I../../../include -Wall -MMD -MP

SRCS = libdragondosbitmap.c libdragondosdir.c libdragondosdisk.c libdragondosdskini.c \
	libdragondoskill.c libdragondosopen.c libdragondosread.c libdragondoswrite.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

vpath %.c ../../../libdragondos

libdragondos.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o *.a *.d

-include $(DEPS)

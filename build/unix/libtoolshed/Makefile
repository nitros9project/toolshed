# Makefile for libtoolshed.a
include ../rules.mak

CFLAGS += -I../../../include -Wall -MMD -MP

SRCS = toolshed.c cocodump.c cocodumpcommand.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

vpath %.c ../../../libtoolshed

libtoolshed.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o *.a *.d

-include $(DEPS)

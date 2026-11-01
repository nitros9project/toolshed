# Makefile for libdiskimage.a
include ../rules.mak

CFLAGS += -I../../../include -Wall -MMD -MP

SRCS = libdiskimagecreate.c libdiskimagedmk.c libdiskimagejvc.c libdiskimageopen.c \
	libdiskimagesdf.c libdiskimagesector.c libdiskimagestream.c libdiskimagetable.c \
	libdiskimagevdk.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

vpath %.c ../../../libdiskimage

libdiskimage.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o *.a *.d

-include $(DEPS)

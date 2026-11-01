# Makefile for librbf.a
include ../rules.mak

CFLAGS += -I../../../include -Wall -MMD -MP

SRCS = librbfbitmap.c librbfmakdir.c librbfread.c librbfrename.c librbfss.c \
	librbfdelete.c librbfgs.c librbfopen.c librbfreadln.c librbfseek.c librbfwrite.c \
	librbfformat.c librbfdisk.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

vpath %.c ../../../librbf

librbf.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -f *.o *.a *.d

-include $(DEPS)

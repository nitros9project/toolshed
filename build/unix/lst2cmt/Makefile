# Makefile for lst2cmt
include ../rules.mak

vpath %.c ../../../lst2cmt

CFLAGS	+= -I../../../include -Wall
#LDFLAGS	+=

lst2cmt:	lst2cmt.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o lst2cmt lst2cmt.exe

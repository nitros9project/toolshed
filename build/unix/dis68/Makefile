# Makefile for dis68
include ../rules.mak

vpath %.c ../../../dis68

dis68$(SUFEXE): dis68.o disasm.o addmod.o symtbl.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o dis68 dis68.exe

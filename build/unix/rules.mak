# Project-Wide Rules

VERSION = 2.2

MERGE		= cat
SOFTLINK	= ln -s
RM		= rm -f
INSTALL		= install
MAKE		= make
CFLAGS		= -Dunix -DUNIX -O3 -I. -I../../../include -Wall -DTOOLSHED_VERSION=\"$(VERSION)\" -D_FILE_OFFSET_BITS=64 -Wno-unused-result -Werror
ASM		= rma
AR		= $(CROSS)ar
RANLIB		= $(CROSS)ranlib
CC		= $(CROSS)cc

ifneq ($(WIN),)
SUFEXE		= .exe
endif

%.l: %.r
	$(MERGE) $< > $@

%.r: %.a
	$(ASM) $< -o=$@

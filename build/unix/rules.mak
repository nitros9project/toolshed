# Project-Wide Rules

VERSION = 2.6.1

MERGE		= cat
SOFTLINK	= ln -s
RM			= rm -f
INSTALL		= install
MAKE		= make
CFLAGS		= -Dunix -DUNIX -O3 -I. -I../../../include -Wall -DTOOLSHED_VERSION=\"$(VERSION)\" -D_FILE_OFFSET_BITS=64 -Wno-unused-result -Werror
#CFLAGS		+= -g -O0
#CFLAGS		+= -g -O0 -fsanitize=address -Wno-error=deprecated-declarations
#LDFLAGS    += -fsanitize=address
ASM			= rma
AR			= $(CROSS)ar
RANLIB		= $(CROSS)ranlib
CC			= $(CROSS)cc

ifneq ($(WIN),)
SUFEXE		= .exe
# MinGW's own C99 printf, so %zu and %lld work and are checked as such
# rather than against the old msvcrt formats.
CFLAGS		+= -D__USE_MINGW_ANSI_STDIO=1
endif

%.l: %.r
	$(MERGE) $< > $@

%.r: %.a
	$(ASM) $< -o=$@

# Makefile for ToolShed

include rules.mak

vpath %.c ../../mamou:../../os9

PACKAGENAME_UNIX	= toolshed-unix64-$(VERSION).tgz
PACKAGENAME_WIN		= toolshed-win64-$(VERSION).zip

prefix      ?= /usr/local
bindir      ?= $(prefix)/bin
docdir      ?= $(prefix)/share/doc/toolshed

ifneq ($(DESTDIR),)
INSTALLDIR  = $(DESTDIR)$(bindir)
DOCDIR      = $(DESTDIR)$(docdir)
else
INSTALLDIR  = $(bindir)
DOCDIR      = $(docdir)
endif

APPS 		= ar2 os9 mamou cecb decb tocgen makewav dis68 lst2cmt diskinfo dgndos

DIRS		= libtoolshed libnative libcecb librbf libcoco libdecb libdiskimage libdragondos libmisc libsys \
		decb cecb os9 makewav tocgen lst2cmt diskinfo dgndos \
		dis68 mamou ar2 \
		doc

ifneq ($(WIN),)
PACKAGENAME	= $(PACKAGENAME_WIN)
MAKEPACKAGE = zip -9
else
PACKAGENAME	= $(PACKAGENAME_UNIX)
MAKEPACKAGE = tar czvf
ifneq ($(NO_COCOFUSE),)
DIRS		:= $(DIRS) unittest
else
DIRS		:= $(DIRS) cocofuse unittest
APPS		:= $(APPS) cocofuse
endif
endif

APPS		:= $(foreach item,$(APPS),$(item)/$(item)$(SUFEXE))

all:
	$(foreach dir, $(DIRS), (cd $(dir); $(MAKE));)

install: all
	$(INSTALL) -d $(INSTALLDIR) $(DOCDIR)
	$(INSTALL) $(APPS) $(INSTALLDIR)
	$(INSTALL) -m 0644 doc/ToolShed.html $(DOCDIR)

package: INSTALLDIR=toolshed-$(VERSION)
package: DOCDIR = $(INSTALLDIR)
package: install
	$(MAKEPACKAGE) $(PACKAGENAME) toolshed-$(VERSION)/*
	rm -rf toolshed-$(VERSION)

cleandirs:
	$(foreach dir, $(DIRS), (cd $(dir); $(MAKE) clean);)
	
clean: cleandirs
	$(RM) $(PACKAGENAME_WIN) $(PACKAGENAME_UNIX)

unixpackage:
	$(MAKE) -f ./Makefile package

winpackage:
	$(MAKE) -f ./Makefile cleandirs
	$(MAKE) -f ./Makefile CC=x86_64-w64-mingw32-gcc WIN=1 package

allpackages: clean unixpackage winpackage 



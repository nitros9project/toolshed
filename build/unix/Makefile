# Makefile for ToolShed

include rules.mak

vpath %.c ../../mamou:../../os9

PACKAGENAME	= toolshed-unix64-$(VERSION).tgz
ifneq ($(DESTDIR),)
INSTALLDIR	= $(DESTDIR)/usr/bin
DOCDIR		= $(DESTDIR)/usr/share/doc/toolshed
else
INSTALLDIR	= /usr/local/bin
DOCDIR		= /usr/local/share/toolshed
endif
APPS 		= ar2 os9 mamou cecb decb tocgen makewav dis68

DIRS		= libtoolshed libnative libcecb librbf libcoco libdecb libmisc libsys \
		decb cecb os9 makewav tocgen \
		dis68 mamou ar2 \
		doc

ifneq ($(WIN),)
PACKAGENAME	= toolshed-win64-$(VERSION).tgz
else
DIRS		:= $(DIRS) cocofuse lst2cmt unittest
APPS		:= $(APPS) cocofuse
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
	tar czvf $(PACKAGENAME) toolshed-$(VERSION)
	rm -rf toolshed-$(VERSION)

cleandirs:
	$(foreach dir, $(DIRS), (cd $(dir); $(MAKE) clean);)
	
clean: cleandirs
	$(RM) $(PACKAGENAME)

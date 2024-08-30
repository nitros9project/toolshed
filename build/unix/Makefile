# Makefile for ToolShed

include rules.mak

vpath %.c ../../mamou:../../os9

PACKAGENAME	= toolshed-$(VERSION).tgz
ifneq ($(DESTDIR),)
INSTALLDIR	= $(DESTDIR)/usr/bin
DOCDIR		= $(DESTDIR)/usr/share/doc/toolshed
else
INSTALLDIR	= /usr/local/bin
DOCDIR		= /usr/local/share/toolshed
endif
DIRS		= libtoolshed libnative libcecb librbf libcoco libdecb libmisc libsys \
		decb cecb os9 makewav tocgen cocofuse \
		dis68 mamou ar2 \
		doc lst2cmt unittest

all:
	$(foreach dir, $(DIRS), (cd $(dir); $(MAKE));)

install: all
	$(INSTALL) -d $(INSTALLDIR) $(DOCDIR)
	$(INSTALL) ar2/ar2 os9/os9 mamou/mamou cecb/cecb decb/decb \
	    tocgen/tocgen makewav/makewav dis68/dis68 lst2cmt/lst2cmt $(INSTALLDIR)
	-$(INSTALL) cocofuse/cocofuse $(INSTALLDIR)
	$(INSTALL) -m 0644 doc/ToolShed.html $(DOCDIR)

package: INSTALLDIR=toolshed-$(VERSION)
package: DOCDIR = $(INSTALLDIR)
package: install
	tar czvf $(PACKAGENAME) toolshed-$(VERSION)
	rm -rf toolshed-$(VERSION)

clean:
	$(foreach dir, $(DIRS), (cd $(dir); $(MAKE) clean);)
	$(RM) $(PACKAGENAME)

# Makefile for ToolShed Quick Look, a macOS Quick Look extension that shows
# the directory of a CoCo or Dragon disk image.
#
# Needs only the Command Line Tools: Xcode is not used.
#
#   make                    build build/ToolShedQuickLook.app
#   make check              test the listings and the page made of them
#   make install            copy the app to ~/Applications and register it
#   make uninstall          remove it again
#
#   ARCHS="arm64 x86_64"    build for these architectures (default: this Mac's)
#   SIGN="Developer ID..."  sign with this identity (default: ad hoc, "-")

ROOT		= ../..
BUILD		= build
ARCHS		?= $(shell uname -m)
MIN_MACOS	= 12.0
SIGN		?= -
INSTALLDIR	?= $(HOME)/Applications
VERSION		= $(shell sed -n 's/^VERSION *= *//p' $(ROOT)/build/unix/rules.mak)

APPNAME		= ToolShedQuickLook
APP		= $(BUILD)/$(APPNAME).app
APPEX		= $(APP)/Contents/PlugIns/ToolShedPreview.appex
LSREGISTER	= /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister

ARCHFLAGS	= $(foreach arch,$(ARCHS),-arch $(arch)) -mmacosx-version-min=$(MIN_MACOS)
CFLAGS		= $(ARCHFLAGS) -Dunix -DUNIX -D_FILE_OFFSET_BITS=64 -DTOOLSHED_VERSION=\"$(VERSION)\" \
		  -O2 -Wall -Wno-unused-result -Werror -I$(ROOT)/include -MMD -MP
SWIFTFLAGS	= -O -parse-as-library -import-objc-header Preview/toolshed_ql.h

# ToolShed's libraries, the commands whose listings are shown, and the
# helpers those commands share.
LIBDIRS		= libtoolshed libcoco libnative libcecb librbf libdecb libdiskimage \
		  libdragondos libflex libmisc libsys
TOOLSHED_SRCS	= $(foreach dir,$(LIBDIRS),$(wildcard $(ROOT)/$(dir)/*.c)) \
		  $(ROOT)/os9/os9dir.c $(ROOT)/decb/decbdir.c \
		  $(ROOT)/dgndos/dgndosdir.c $(ROOT)/dgndos/dgndosutil.c \
		  $(ROOT)/cflex/cflexdir.c $(ROOT)/cflex/cflexutil.c
OBJS		= $(patsubst $(ROOT)/%.c,$(BUILD)/obj/%.o,$(TOOLSHED_SRCS)) $(BUILD)/obj/toolshed_ql.o
LIB		= $(BUILD)/libtoolshed_ql.a

.PHONY: all check install uninstall clean

all: $(APP)

$(BUILD)/obj/%.o: $(ROOT)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/toolshed_ql.o: Preview/toolshed_ql.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	$(RM) $@
	@libtool -static -o $@ $^

# The extension, built for each architecture and then joined.
$(BUILD)/%/ToolShedPreview: Preview/PreviewProvider.swift Preview/toolshed_ql.h $(LIB)
	@mkdir -p $(@D)
	swiftc $(SWIFTFLAGS) -target $*-apple-macos$(MIN_MACOS) -module-name ToolShedPreview \
		-Xlinker -e -Xlinker _NSExtensionMain \
		Preview/PreviewProvider.swift $(LIB) -framework QuickLookUI -o $@

$(BUILD)/%/$(APPNAME): App/main.swift
	@mkdir -p $(@D)
	swiftc -O -target $*-apple-macos$(MIN_MACOS) -module-name $(APPNAME) App/main.swift -o $@

$(BUILD)/ToolShedPreview: $(foreach arch,$(ARCHS),$(BUILD)/$(arch)/ToolShedPreview)
	lipo -create $^ -output $@

$(BUILD)/$(APPNAME): $(foreach arch,$(ARCHS),$(BUILD)/$(arch)/$(APPNAME))
	lipo -create $^ -output $@

PLIST_SED	= sed -e 's/@VERSION@/$(VERSION)/g' -e 's/@MIN_MACOS@/$(MIN_MACOS)/g'

$(APP): $(BUILD)/ToolShedPreview $(BUILD)/$(APPNAME) Preview/Info.plist Preview/Preview.entitlements App/Info.plist
	rm -rf $@
	mkdir -p $(APPEX)/Contents/MacOS $@/Contents/MacOS $@/Contents/Resources
	$(PLIST_SED) Preview/Info.plist > $(APPEX)/Contents/Info.plist
	cp $(BUILD)/ToolShedPreview $(APPEX)/Contents/MacOS/
	$(PLIST_SED) App/Info.plist > $@/Contents/Info.plist
	cp $(BUILD)/$(APPNAME) $@/Contents/MacOS/
	cp $(ROOT)/gui/wx/packaging/org.nitros9.diskshed.icns $@/Contents/Resources/AppIcon.icns
	codesign --force --sign "$(SIGN)" --options runtime --entitlements Preview/Preview.entitlements $(APPEX)
	codesign --force --sign "$(SIGN)" --options runtime $@

# The tests run the listings on the unit tests' images, natively.
$(BUILD)/qltest: tests/qltest.c Preview/toolshed_ql.h $(LIB)
	$(CC) $(CFLAGS) -IPreview -I$(ROOT)/unittest tests/qltest.c $(LIB) -lm -o $@

# The page the extension makes, written by a command instead:
#   build/listing image... > page.html
$(BUILD)/listing: tests/listing/main.swift Preview/PreviewProvider.swift Preview/toolshed_ql.h $(LIB)
	swiftc $(filter-out -parse-as-library,$(SWIFTFLAGS)) -module-name Listing \
		tests/listing/main.swift Preview/PreviewProvider.swift $(LIB) -framework QuickLookUI -o $@

TESTDATA	= $(ROOT)/unittest/testdata

check: $(BUILD)/qltest $(BUILD)/listing
	cd $(BUILD) && ./qltest ../$(TESTDATA)
	$(BUILD)/listing "$(TESTDATA)/os9 files mame.dmk" | grep -q '<h1>os9 files mame.dmk</h1>'
	$(BUILD)/listing "$(TESTDATA)/os9 files mame.dmk" | grep -q 'Directory of os9 files mame.dmk,./DEEP'
	$(BUILD)/listing "$(TESTDATA)/rsdos mame.dmk" | grep -q '<h2>Disk BASIC</h2>'
	$(BUILD)/listing "$(TESTDATA)/dragondos mame.vdk" | grep -q '<h2>Dragon DOS</h2>'
	ln -sf "../$(TESTDATA)/rsdos mame.dmk" "$(BUILD)/comma,image.dsk"
	$(BUILD)/listing "$(BUILD)/comma,image.dsk" | grep -q 'Directory of: comma,image.dsk,'
	rm -f "$(BUILD)/comma,image.dsk"
	! $(BUILD)/listing Makefile 2>/dev/null
	@echo "Listing tests passed"

install: $(APP)
	mkdir -p "$(INSTALLDIR)"
	rm -rf "$(INSTALLDIR)/$(APPNAME).app"
	cp -R $(APP) "$(INSTALLDIR)/"
	$(LSREGISTER) -f -R "$(INSTALLDIR)/$(APPNAME).app"
	pluginkit -a "$(INSTALLDIR)/$(APPNAME).app/Contents/PlugIns/ToolShedPreview.appex"
	pluginkit -e use -i org.nitros9.toolshed.quicklook.preview
	qlmanage -r >/dev/null
	qlmanage -r cache >/dev/null

uninstall:
	-pluginkit -r "$(INSTALLDIR)/$(APPNAME).app/Contents/PlugIns/ToolShedPreview.appex"
	-$(LSREGISTER) -u "$(INSTALLDIR)/$(APPNAME).app"
	rm -rf "$(INSTALLDIR)/$(APPNAME).app"
	qlmanage -r >/dev/null

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

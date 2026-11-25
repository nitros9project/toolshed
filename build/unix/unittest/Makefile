# Makefile for Unit Tests
include ../rules.mak

.DEFAULT_GOAL := testall
.PHONY: testall clean

vpath %.c ../../../unittest

# A test is rebuilt when a header it includes changes.
CFLAGS += -MMD -MP

LDLIBS += -L../libtoolshed -L../libcoco -L../libcecb -L../libdecb -L../libdragondos -L../libflex -L../libdiskimage \
          -L../librbf -L../libnative -L../libmisc -L../libsys \
          -ltoolshed -lcoco -lcecb -ldecb -ldragondos -lflex -ldiskimage -lrbf -lnative -lmisc -lsys -lm

TEST_LIBS = \
    ../libtoolshed/libtoolshed.a \
    ../libcoco/libcoco.a \
    ../libcecb/libcecb.a \
    ../libdecb/libdecb.a \
    ../libdragondos/libdragondos.a \
    ../libflex/libflex.a \
    ../libdiskimage/libdiskimage.a \
    ../librbf/librbf.a \
    ../libnative/libnative.a \
    ../libmisc/libmisc.a \
    ../libsys/libsys.a


TESTS = librbftest libdecbtest libcecbtest libtoolshedtest libnativetest libmisctest os9commandtest \
	decbcommandtest cecbcommandtest libsystest libdiskimagetest infocommandtest libdragondostest dgndoscommandtest libflextest cflexcommandtest commandaliastest

# On Windows the programs end in .exe.
TEST_PROGRAMS = $(addsuffix $(SUFEXE),$(TESTS))

# Only the source and libraries are linked, not the headers the .d files add.
$(TEST_PROGRAMS): %$(SUFEXE): %.c $(TEST_LIBS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(TEST_LIBS) $(LDLIBS) -o $@

testall: $(TEST_PROGRAMS)
ifeq (,$(NOTEST))
	./librbftest
	./libdecbtest
	./libcecbtest
	./libtoolshedtest
	./libnativetest
	./libmisctest
	./libsystest
	./os9commandtest
	./decbcommandtest
	./cecbcommandtest
	./libdiskimagetest
	./infocommandtest
	./libdragondostest
	./dgndoscommandtest
	./libflextest
	./cflexcommandtest
	./commandaliastest
endif

clean:
	$(RM) *.o *.d *.dsk *.wav *.cas *.c10 $(TEST_PROGRAMS)

-include $(TESTS:=.d)

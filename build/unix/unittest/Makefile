# Makefile for Unit Tests
include ../rules.mak

.DEFAULT_GOAL := testall
.PHONY: testall clean

vpath %.c ../../../unittest

LDLIBS += -L../libtoolshed -L../libcoco -L../libcecb -L../libdecb -L../libdragondos -L../libdiskimage \
          -L../librbf -L../libnative -L../libmisc -L../libsys \
          -ltoolshed -lcoco -lcecb -ldecb -ldragondos -ldiskimage -lrbf -lnative -lmisc -lsys -lm

TEST_LIBS = \
    ../libtoolshed/libtoolshed.a \
    ../libcoco/libcoco.a \
    ../libcecb/libcecb.a \
    ../libdecb/libdecb.a \
    ../libdragondos/libdragondos.a \
    ../libdiskimage/libdiskimage.a \
    ../librbf/librbf.a \
    ../libnative/libnative.a \
    ../libmisc/libmisc.a \
    ../libsys/libsys.a


TESTS = librbftest libdecbtest libcecbtest libtoolshedtest libnativetest libmisctest os9commandtest \
	decbcommandtest cecbcommandtest libsystest libdiskimagetest diskinfocommandtest libdragondostest

$(TESTS): $(TEST_LIBS)

testall: $(TESTS)
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
	./diskinfocommandtest
	./libdragondostest
endif

clean:
	$(RM) *.o *.dsk *.wav *.cas *.c10 $(TESTS)

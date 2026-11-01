# Makefile for Unit Tests
include ../rules.mak

.DEFAULT_GOAL := testall
.PHONY: testall clean

vpath %.c ../../../unittest

LDLIBS += -L../libtoolshed -L../libcoco -L../libcecb -L../libdecb -L../libdiskimage \
          -L../librbf -L../libnative -L../libmisc -L../libsys \
          -ltoolshed -lcoco -lcecb -ldecb -ldiskimage -lrbf -lnative -lmisc -lsys -lm

TEST_LIBS = \
    ../libtoolshed/libtoolshed.a \
    ../libcoco/libcoco.a \
    ../libcecb/libcecb.a \
    ../libdecb/libdecb.a \
    ../libdiskimage/libdiskimage.a \
    ../librbf/librbf.a \
    ../libnative/libnative.a \
    ../libmisc/libmisc.a \
    ../libsys/libsys.a


TESTS = librbftest libdecbtest libcecbtest libtoolshedtest libnativetest libmisctest os9commandtest \
	decbcommandtest cecbcommandtest libsystest libdiskimagetest diskinfocommandtest

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
endif

clean:
	$(RM) *.o *.dsk *.wav *.cas *.c10 $(TESTS)

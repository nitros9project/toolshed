# Makefile for Unit Tests
include ../rules.mak

vpath %.c ../../../unittest

LDLIBS += -L../libtoolshed -L../libcoco -L../libcecb -L../libdecb \
          -L../librbf -L../libnative -L../libmisc -L../libsys \
          -ltoolshed -lcoco -lcecb -ldecb -lrbf -lnative -lmisc -lsys -lm

TESTS = librbftest libdecbtest libcecbtest libtoolshedtest os9commandtest

testall: $(TESTS)
ifeq (,$(NOTEST))
	./librbftest
	./libdecbtest
	./libcecbtest
	./libtoolshedtest
	./os9commandtest
endif

clean:
	$(RM) *.o *.dsk *.wav *.cas $(TESTS)

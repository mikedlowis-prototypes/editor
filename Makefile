INCS = -Iinc/
MAKEFLAGS = -j4
BINS = tide
MAN1 = docs/tide.1

LIBEDIT_OBJS =     \
	lib/buf.o      \
	lib/utf8.o     \
	lib/utils.o    \
	lib/job.o      \
	lib/view.o     \
	lib/x11.o      \
	lib/config.o

TEST_BINS =     \
	tests/libedit

include config.mk

.PHONY: all docs clean install uninstall test

all: $(BINS) test

docs:
	ronn --roff docs/*.md

clean:
	find . -name '*.[oad]' -delete
	find . \( -name '*.gcno' -o -name '*.gcda' \) -delete
	$(RM) $(BINS) $(TEST_BINS) flaws.txt

install:
	mkdir -p $(PREFIX)/bin
	cp -f tide $(PREFIX)/bin
	cp -f xcpd $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/tide
	rm -f $(PREFIX)/bin/xcpd

test: $(TEST_BINS)
	for t in $(TEST_BINS); do ./$$t || exit 1; done

bugs:
	make clean
	scan-build make

gcov:
	make clean
	make GCOV=1 test

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

tide: tide.o libedit.a
xcpd: xcpd.o libedit.a
tests/libedit: tests/libedit.o tests/lib/buf.o tests/lib/utf8.o libedit.a

# define implicit rule for building binaries
%: %.o
	$(LD) -o $@ $^ $(LDFLAGS)

# load generate dependencies
-include *.d lib/*.d tests/*.d tests/lib/*.d

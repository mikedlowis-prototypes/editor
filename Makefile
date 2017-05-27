INCS = -Iinc/

BINS = tide xpick xcpd term
MAN1 = docs/tide.1 docs/xpick.1 docs/xtagpick.1 docs/xfilepick.1

LIBEDIT_OBJS =     \
	lib/buf.o      \
	lib/filetype.o \
	lib/utf8.o     \
	lib/utils.o    \
	lib/exec.o     \
	lib/view.o     \
	lib/x11.o      \
	lib/win.o

TEST_BINS =      \
	tests/tide  \
	tests/xpick  \
	tests/term   \
	tests/libedit

include config.mk

.PHONY: all docs clean install uninstall test

all: $(BINS) test $(MAN1)

docs:
	ronn --roff docs/*.md

clean:
	find . -name '*.[oad]' -delete
	$(RM) xpick tide xcpd term tests/libedit
	$(RM) $(TEST_BINS)

install: all
	mkdir -p $(PREFIX)/bin
	cp -f tide $(PREFIX)/bin
	cp -f xpick $(PREFIX)/bin
	cp -f xcpd $(PREFIX)/bin
	cp -f xfilepick $(PREFIX)/bin
	cp -f xtagpick $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/tide
	rm -f $(PREFIX)/bin/xpick
	rm -f $(PREFIX)/bin/xcpd
	rm -f $(PREFIX)/bin/xfilepick
	rm -f $(PREFIX)/bin/xtagpick

test: $(TEST_BINS)
	for t in $(TEST_BINS); do ./$$t || exit 1; done

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

tide: tide.o libedit.a
xpick: xpick.o libedit.a
xcpd: xcpd.o libedit.a
term: term.o libedit.a
tests/libedit: tests/libedit.o tests/lib/buf.o tests/lib/utf8.o libedit.a
tests/tide: tests/tide.o libedit.a
tests/xpick: tests/xpick.o libedit.a
tests/term: tests/term.o libedit.a

# define implicit rule for building binaries
%: %.o
	$(LD) -o $@ $^ $(LDFLAGS)

# load generate dependencies
-include *.d lib/*.d tests/*.d tests/lib/*.d


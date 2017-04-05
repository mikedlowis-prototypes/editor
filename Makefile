INCS = -Iinc/

BINS = xedit xpick xcpd term
MAN1 = docs/edit.1 docs/xedit.1 docs/xpick.1 docs/xtagpick.1 docs/xfilepick.1

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
	tests/xedit  \
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
	$(RM) xpick xedit term tests/libedit
	$(RM) $(TEST_BINS)

install: all
	mkdir -p $(PREFIX)/bin
	cp -f xedit $(PREFIX)/bin
	cp -f xpick $(PREFIX)/bin
	cp xfilepick $(PREFIX)/bin
	cp xtagpick $(PREFIX)/bin
	cp xman $(PREFIX)/bin
	cp edit $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/xedit
	rm -f $(PREFIX)/bin/xpick
	rm -f $(PREFIX)/bin/xfilepick
	rm -f $(PREFIX)/bin/xtagpick
	rm -f $(PREFIX)/bin/xman
	rm -f $(PREFIX)/bin/edit

test: $(TEST_BINS)
	for t in $(TEST_BINS); do ./$$t || exit 1; done

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

xedit: xedit.o libedit.a
xpick: xpick.o libedit.a
xcpd: xcpd.o libedit.a
term: term.o libedit.a
tests/libedit: tests/libedit.o tests/lib/buf.o tests/lib/utf8.o libedit.a
tests/xedit: tests/xedit.o libedit.a
tests/xpick: tests/xpick.o libedit.a
tests/term: tests/term.o libedit.a

# load generate dependencies
-include *.d lib/*.d tests/*.d tests/lib/*.d


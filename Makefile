INCS = -Iinc/

LIBEDIT_OBJS =     \
	lib/buf.o      \
	lib/filetype.o \
	lib/utf8.o     \
	lib/utils.o    \
	lib/exec.o     \
	lib/view.o     \
	lib/x11.o      \
	lib/win.o

TEST_BINS = \
	tests/xedit \
	tests/xpick \
	tests/libedit

include config.mk

all: xedit xpick term test

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
	for t in $(TEST_BINS); do ./$$t; done

xedit: xedit.o libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)

xpick: xpick.o libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)
	
term: term.o libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

#unittests: $(TEST_OBJS) libedit.a

tests/libedit: tests/lib/buf.o tests/lib/utf8.o libedit.a
tests/xedit: tests/xedit.o libedit.a
tests/xpick: tests/xpick.o libedit.a

-include *.d lib/*.d tests/*.d


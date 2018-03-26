INCS = -Iinc/
MAKEFLAGS = -j4
BINS = tide
MAN1 = docs/tide.1 docs/pick.1 docs/picktag.1 docs/pickfile.1

LIBEDIT_OBJS =     \
	lib/buf.o      \
	lib/utf8.o     \
	lib/utils.o    \
	lib/job.o      \
	lib/view.o     \
	lib/x11.o      \
	lib/config.o

TEST_BINS =     \
	tests/tide  \
	tests/pick  \
	tests/libedit

include config.mk

.PHONY: all docs clean install uninstall test

all: $(BINS) $(MAN1)

docs:
	ronn --roff docs/*.md

clean:
	find . -name '*.[oad]' -delete
	find . \( -name '*.gcno' -o -name '*.gcda' \) -delete
	$(RM) $(BINS) $(TEST_BINS)

install:
	mkdir -p $(PREFIX)/bin
	cp -f tcmd $(PREFIX)/bin
	cp -f tfetch $(PREFIX)/bin
	cp -f tide $(PREFIX)/bin
	cp -f tide-hl.rb $(PREFIX)/bin
	cp -f tctl $(PREFIX)/bin
	cp -f pick $(PREFIX)/bin
	cp -f xcpd $(PREFIX)/bin
	cp -f pickfile $(PREFIX)/bin
	cp -f picktag $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/tcmd
	rm -f $(PREFIX)/bin/tfetch
	rm -f $(PREFIX)/bin/tide
	rm -f $(PREFIX)/bin/tide-hl.rb
	rm -f $(PREFIX)/bin/tctl
	rm -f $(PREFIX)/bin/pick
	rm -f $(PREFIX)/bin/xcpd
	rm -f $(PREFIX)/bin/pickfile
	rm -f $(PREFIX)/bin/picktag

test: $(TEST_BINS)
	@echo "Warning: unit tests disabled temporarily. Re-enable later."
#	for t in $(TEST_BINS); do ./$$t || exit 1; done

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

tide: tide.o libedit.a
pick: pick.o libedit.a
xcpd: xcpd.o libedit.a
tfetch: tfetch.o
tctl: tctl.o libedit.a
tests/libedit: tests/libedit.o tests/lib/buf.o tests/lib/utf8.o libedit.a
tests/tide: tests/tide.o libedit.a
tests/pick: tests/pick.o libedit.a

# define implicit rule for building binaries
%: %.o
	$(LD) -o $@ $^ $(LDFLAGS)

# load generate dependencies
-include *.d lib/*.d tests/*.d tests/lib/*.d


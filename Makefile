INCS = -Iinc/

LIBEDIT_OBJS =     \
	lib/buf.o      \
	lib/filetype.o \
	lib/utf8.o     \
	lib/utils.o    \
	lib/exec.o     \
	lib/view.o     \
	lib/x11.o

TEST_OBJS =      \
	unittests.o  \
	tests/buf.o  \
	tests/utf8.o \
	tests/xedit.o

include config.mk

all: xedit xpick

clean:
	$(RM) *.o lib*/*.o tests/*.o *.a xpick xedit unittests
	$(RM) *.d lib*/*.d tests/*.d
	$(RM) *.gcno lib*/*.gcno tests/*.gcno
	$(RM) *.gcda lib*/*.gcda tests/*.gcda
	$(RM) *.gcov lib*/*.gcov tests/*.gcov

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

test: unittests
	./unittests

xedit: xedit.o libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)

xpick: xpick.o libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

unittests: $(TEST_OBJS) libedit.a

-include *.d lib/*.d tests/*.d


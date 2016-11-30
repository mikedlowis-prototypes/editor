INCS = -Iinc/

LIBEDIT_OBJS =         \
	libedit/buf.o      \
	libedit/charset.o  \
	libedit/utf8.o     \
	libedit/utils.o    \
	libedit/exec.o     \
	libedit/view.o

LIBX_OBJS = \
	libx/x11.o

TEST_OBJS =     \
	unittests.o \
	tests/buf.o \
	tests/utf8.o

include config.mk

all: xedit xpick

clean:
	$(RM) *.o lib*/*.o test/*.o *.a xpick xedit unittests

install: all
	mkdir -p $(PREFIX)/bin
	cp xedit $(PREFIX)/bin
	cp xpick $(PREFIX)/bin
	cp xfilepick $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/xedit
	rm -f $(PREFIX)/bin/xpick
	rm -f $(PREFIX)/bin/xfilepick

test: unittests
	./unittests

xedit: xedit.o libx.a libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)

xpick: xpick.o libx.a libedit.a
	$(LD) -o $@ $^ $(LDFLAGS)

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

libx.a: $(LIBX_OBJS)
	$(AR) $(ARFLAGS) $@ $^

unittests: $(TEST_OBJS) libedit.a

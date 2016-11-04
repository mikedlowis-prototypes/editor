CC = c99
LDFLAGS = -L/usr/X11/lib -lX11 -lXft -lfontconfig
CFLAGS = -Os -Iinc/ -I/usr/X11/include -I/usr/X11/include/freetype2

LIBEDIT_OBJS =         \
	libedit/buf.o      \
	libedit/charset.o  \
	libedit/keyboard.o \
	libedit/mouse.o    \
	libedit/screen.o   \
	libedit/utf8.o     \
	libedit/utils.o

LIBX_OBJS = \
	libx/x11.o

TEST_OBJS =     \
	unittests.o \
	tests/buf.o \
	tests/utf8.o

all: xedit xpick test

clean:
	$(RM) *.o lib*/*.o test/*.o *.a xpick xedit unittests

test: unittests
	./unittests

xedit: xedit.o libx.a libedit.a
xpick: xpick.o libx.a libedit.a

libedit.a: $(LIBEDIT_OBJS)
	$(AR) rcs $@ $^

libx.a: $(LIBX_OBJS)
	$(AR) rcs $@ $^

unittests: $(TEST_OBJS) libedit.a

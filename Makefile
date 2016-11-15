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

all: xedit

clean:
	$(RM) *.o lib*/*.o test/*.o *.a xpick xedit unittests

test: unittests
	./unittests

xedit: xedit.o libx.a libedit.a
xpick: xpick.o libx.a libedit.a

libedit.a: $(LIBEDIT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

libx.a: $(LIBX_OBJS)
	$(AR) $(ARFLAGS) $@ $^

unittests: $(TEST_OBJS) libedit.a

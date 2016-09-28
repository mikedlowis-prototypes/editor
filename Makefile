LDFLAGS  = -L/opt/X11/lib -lX11 -lXft
CFLAGS   = --std=c99 -Wall -Wextra -I. -I/opt/X11/include -I/opt/local/include/freetype2 -I/usr/include/freetype2
SRCS     = xedit.c buf.c screen.c
OBJS     = $(SRCS:.c=.o)
TESTSRCS = tests/tests.c tests/buf.c
TESTOBJS = $(TESTSRCS:.c=.o)

all: edit test

test: unittests
	./unittests

edit: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

unittests: $(TESTOBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) edit unittests $(OBJS) $(TESTOBJS)

$(OBJS): edit.h Makefile

.PHONY: all test

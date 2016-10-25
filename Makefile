#GCOV     = --coverage
LDFLAGS  = $(GCOV) -L/opt/X11/lib -lX11 -lXft -lfontconfig
CFLAGS   = $(GCOV) -Os --std=gnu99 -Wall -Wextra -I. -I/opt/X11/include -I/opt/local/include/freetype2 -I/usr/include/freetype2
OBJS     = buf.o screen.o utf8.o keyboard.o mouse.o charset.o utils.o
TESTOBJS = tests/tests.o tests/buf.o tests/utf8.o

all: edit test

test: unittests
	./unittests

edit: xedit.o $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

unittests: $(TESTOBJS) $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

coverage: test
	gcov -fabc $(OBJS) > coverage.txt


clean:
	$(RM) edit unittests xedit.o $(OBJS) $(TESTOBJS) coverage.txt
	$(RM) *.gcno *.gcda *.gcov tests/*.gcno tests/*.gcda tests/*.gcov

$(OBJS): edit.h Makefile
xedit.o: edit.h Makefile

.PHONY: all test

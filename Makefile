#------------------------------------------------------------------------------
# Build Configuration
#------------------------------------------------------------------------------
# name and version
PROJNAME = edit
VERSION  = 0.0.1

# tools
CC = c99
LD = ${CC}

# flags
INCS     = -Isource/
CPPFLAGS = -D_XOPEN_SOURCE=700
CFLAGS   = ${INCS} ${CPPFLAGS}
LDFLAGS  = ${LIBS} -lncurses

# commands
COMPILE = @echo CC $@; ${CC} ${CFLAGS} -c -o $@ $<
LINK    = @echo LD $@; ${LD} -o $@ $^ ${LDFLAGS}
CLEAN   = @rm -f

#------------------------------------------------------------------------------
# Build-Specific Macros
#------------------------------------------------------------------------------
# library macros
BIN  = ${PROJNAME}
DEPS = ${OBJS:.o=.d}
OBJS = source/main.o

# distribution dir and tarball settings
DISTDIR   = ${PROJNAME}-${VERSION}
DISTTAR   = ${DISTDIR}.tar
DISTGZ    = ${DISTTAR}.gz
DISTFILES = config.mk LICENSE.md Makefile README.md source

# load user-specific settings
-include config.mk

#------------------------------------------------------------------------------
# Phony Targets
#------------------------------------------------------------------------------
.PHONY: all options tests

all: options ${BIN}

options:
	@echo "Toolchain Configuration:"
	@echo "  CC       = ${CC}"
	@echo "  CFLAGS   = ${CFLAGS}"
	@echo "  LD       = ${LD}"
	@echo "  LDFLAGS  = ${LDFLAGS}"

dist: clean
	@echo DIST ${DISTGZ}
	@mkdir -p ${DISTDIR}
	@cp -R ${DISTFILES} ${DISTDIR}
	@tar -cf ${DISTTAR} ${DISTDIR}
	@gzip ${DISTTAR}
	@rm -rf ${DISTDIR}

clean:
	${CLEAN} ${DEPS}
	${CLEAN} ${BIN} ${OBJS}
	${CLEAN} ${OBJS:.o=.gcno} ${OBJS:.o=.gcda}
	${CLEAN} ${DISTTAR} ${DISTGZ}

#------------------------------------------------------------------------------
# Target-Specific Rules
#------------------------------------------------------------------------------
.c.o:
	${COMPILE}

${BIN}: ${OBJS}
	${LINK}

# load dependency files
-include ${DEPS}


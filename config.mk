# Customize this file to fit your system

# Install location
PREFIX = $(HOME)

# OSX X11 Flags
INCS += -I/usr/X11/include           \
		-I/usr/X11/include/freetype2 \
		-I.
LIBS += -L/usr/X11/lib

# Linux Freetype2 Flags
INCS += -I/usr/include/freetype2

# Compiler Setup
CC = cc
CFLAGS = --std=c99 -MMD $(INCS)

# Linker Setup
LD = $(CC)
LDFLAGS = $(LIBS) -lX11 -lXft -lfontconfig -lutil

# Archive Setup
AR = ar
ARFLAGS = rcs

# Treat all  warnings as errors (poor man's lint?)
#CFLAGS += -Wall -Wextra -Werror

# GCC Debugging
ifeq ($(DEBUG), 1)
    CFLAGS  += -g -O0
    LDFLAGS += -g -O0
endif

# GCC Profiling
ifeq ($(GPROF), 1)
    CFLAGS  += -pg
    LDFLAGS += -pg
endif

# GCC Coverage
ifeq ($(GCOV), 1)
    CFLAGS  += --coverage
    LDFLAGS += --coverage
endif

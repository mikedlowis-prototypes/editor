# Customize this file to fit your system

# Install location
PREFIX = $(HOME)

# OSX X11 Flags
INCS += -I/usr/X11/include           \
		-I/usr/X11/include/freetype2
LIBS += -L/usr/X11/lib

# Linux Freetype2 Flags
INCS += -I/usr/include/freetype2

# Compiler Setup
CC = cc
CFLAGS = --std=c99 -MMD -g -O0 $(INCS)

# Linker Setup
LD = $(CC)
LDFLAGS = $(LIBS) -lX11 -lXft -lfontconfig

# Archive Setup
AR = ar
ARFLAGS = rcs

# Treat all  warnings as errors (poor man's lint?)
#CFLAGS += -Wall -Wextra -Werror

# Gcov Coverage
#CFLAGS  += --coverage
#LDFLAGS += --coverage

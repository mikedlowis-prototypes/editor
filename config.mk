# Customize this file to fit your system

# Install location
PREFIX = $(HOME)

# Compiler Setup
CC = cc
CFLAGS = --std=c99 -MMD -g -O0 $(INCS)

#CFLAGS += -Wall -Wextra -Werror

# Linker Setup
LD = $(CC)
LDFLAGS = $(LIBS) -lX11 -lXft -lfontconfig

# Archive Setup
AR = ar
ARFLAGS = rcs

# OSX X11 Flags
INCS += -I/usr/X11/include           \
		-I/usr/X11/include/freetype2
LIBS += -L/usr/X11/lib

# Linux Freetype2 Flags
INCS += -I/usr/include/freetype2

# Gcov Coverage
#CFLAGS  += --coverage
#LDFLAGS += --coverage

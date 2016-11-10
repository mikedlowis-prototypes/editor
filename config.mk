# Customize this file to fit your system

# Compiler Setup
CC = c99
CFLAGS = -Os $(INCS)

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
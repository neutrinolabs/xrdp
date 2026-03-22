# Makefile for macOS native capture module for xrdp

CC = clang
OBJC = clang

# Module name
MODULE = libmacos.dylib

# Source files
SRCS_C = macos_module.c
SRCS_M = macos_capture.m macos_input.m

# Object files
OBJS = $(SRCS_C:.c=.o) $(SRCS_M:.m=.o)

# Install paths
PREFIX = /usr/local
LIBDIR = $(PREFIX)/lib/xrdp

# Compiler flags
CFLAGS = -Wall -O2 -fPIC -DPIC -I.

OBJCFLAGS = $(CFLAGS) -fobjc-arc

# Linker flags
LDFLAGS = -dynamiclib \
          -framework Foundation \
          -framework ScreenCaptureKit \
          -framework CoreGraphics \
          -framework CoreMedia \
          -framework IOSurface \
          -framework Carbon \
          -framework CoreVideo

# No external libraries needed
LIBS =

all: $(MODULE)

$(MODULE): $(OBJS)
	$(OBJC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "Built $(MODULE)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.m
	$(OBJC) $(OBJCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(MODULE)

install: $(MODULE)
	mkdir -p $(DESTDIR)$(LIBDIR)
	install -m 755 $(MODULE) $(DESTDIR)$(LIBDIR)/
	@echo "Installed $(MODULE) to $(DESTDIR)$(LIBDIR)/"

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(MODULE)

.PHONY: all clean install uninstall

CC ?= clang
CFLAGS+=`/Library/Frameworks/Mono.framework/Versions/Current/bin/pkg-config --cflags gobject-2.0 mono-2`
LDFLAGS+=`/Library/Frameworks/Mono.framework/Versions/Current/bin/pkg-config --libs gobject-2.0 mono-2 $(optional_libs)`

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

MONODEVELOP_BIN_DIR?=`pwd`/../monodevelop/main/build/bin

all: libgobject-tracker.dylib

clean:
	rm -f libgobject-list.so $(OBJS)

libgobject-tracker.dylib: $(OBJS)
	$(CC) -shared -Wl,-install_name,$@ -o $@ $^ -lc -ldl ${LDFLAGS}

install: libgobject-tracker.dylib
	@if [ -d "$(MONODEVELOP_BIN_DIR)" ]; then \
		cp libgobject-tracker.dylib $(MONODEVELOP_BIN_DIR) && echo "Installed to $(MONODEVELOP_BIN_DIR))/libgobject-tracker.dylib"; \
	else \
		echo "Set 'MONODEVELOP_BIN_DIR' to a compiled monodevelop's 'bin' directory, then run 'make install'"; \
	fi

uninstall:
	rm -f "$(MONODEVELOP_BIN_DIR)/libgobject-tracker.dylib"

.PHONY: all clean

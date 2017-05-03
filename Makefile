CC ?= clang
CFLAGS+=`/Library/Frameworks/Mono.framework/Versions/Current/bin/pkg-config --cflags gobject-2.0 mono-2`
LDFLAGS+=`/Library/Frameworks/Mono.framework/Versions/Current/bin/pkg-config --libs gobject-2.0 mono-2 $(optional_libs)`

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: libgobject-tracker.dylib

clean:
	rm -f libgobject-list.so $(OBJS)

libgobject-tracker.dylib: $(OBJS)
	$(CC) -shared -Wl,-install_name,$@ -o $@ $^ -lc -ldl ${LDFLAGS}

.PHONY: all clean

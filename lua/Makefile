PREFIX = /usr/local
LUA_VERSION = 5.2

CFLAGS = -I/usr/include/lua$(LUA_VERSION)
CFLAGS += -std=c11 -fPIC -shared -g -Wall -Werror
CFLAGS += -O2 -DNDEBUG -march=native -mtune=native -fomit-frame-pointer

LIB = mini.so

all: $(LIB)

clean:
	rm -f $(LIB)

install: $(LIB)
	install -pm 0755 $(LIB) $(PREFIX)/lib/lua/$(LUA_VERSION)/$(LIB)

uninstall:
	rm -f $(PREFIX)/lib/lua/$(LUA_VERSION)/$(LIB)

.PHONY: all clean install uninstall

$(LIB): mini.c ../mini.c ../mini.h
	$(CC) $(CFLAGS) $< ../mini.c -o $@

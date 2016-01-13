PREFIX = /usr/local

CFLAGS = -std=c11 -Wall -Werror -g
CFLAGS += -O2 -s -DNDEBUG -march=native -mtune=native -fomit-frame-pointer
CFLAGS += -flto -fdata-sections -ffunction-sections -Wl,--gc-sections

#--------------------------------------
# Abstract targets
#--------------------------------------

all: mini

clean:
	rm -f mini lua/mini.so

check: lua/mini.so
	cd test && valgrind --leak-check=full --error-exitcode=1 lua test.lua

install: mini
	install -spm 0755 $< $(PREFIX)/bin/mini

uninstall:
	rm -f $(PREFIX)/bin/mini

.PHONY: all clean check install uninstall


#--------------------------------------
# Concrete targets
#--------------------------------------

cmd/mini.ih: cmd/mini.txt
	cmd/mkcstring.py < $< > $@

mini: $(wildcard cmd/*) mini.h mini.c
	$(CC) $(CFLAGS) cmd/mini.c cmd/cmd.c mini.c -o $@

lua/mini.so: mini.h mini.c lua/mini.c
	$(MAKE) -C lua

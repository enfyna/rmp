default: build/rmp

CFLAGS=-Wall -Wextra -pedantic -Og -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g
SOURCE=rmp.c rmp_build.c rmp_play.c rmp_add.c

TAGLIB=`pkg-config --cflags --libs taglib_c`
NCURSES=`pkg-config --cflags --libs ncurses`

build/rmp: $(SOURCE) Makefile rmp.h
	@mkdir -p build
	gcc $(NCURSES) $(TAGLIB) $(SOURCE) $(CFLAGS) -o build/rmp

.PHONY: clean
clean:
	rm -fr build

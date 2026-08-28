default: build/rmp

MODE ?= FINAL

DEBUG=-Og -fsanitize=address,undefined,leak -fno-omit-frame-pointer -g
FINAL=-Oz

CFLAGS=-Wall -Wextra -pedantic 
ifeq ($(MODE),FINAL)
    CFLAGS += $(FINAL)
else
    CFLAGS += $(DEBUG)
endif

SOURCE=rmp.c rmp_build.c rmp_play.c rmp_add.c music.c
OBJS=$(patsubst %.c,build/%.o,$(SOURCE))

TAGLIB=`pkg-config --cflags --libs taglib_c`
NCURSES=`pkg-config --cflags --libs ncurses`
VLC=`pkg-config --cflags --libs libvlc`

build/rmp: $(OBJS) rmp.h music.h Makefile
	gcc $(OBJS) $(VLC) $(NCURSES) $(TAGLIB) $(CFLAGS) -o build/rmp 

build/%.o: %.c rmp.h music.h Makefile | build
	gcc $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build

.PHONY: clean
clean:
	rm -fr build

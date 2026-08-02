default: build/rmp

CFLAGS=-Wall -Wextra -pedantic -fsanitize=address,undefined,leak -Og -g

build/rmp: rmp.c rmp_build.c rmp_play.c rmp_add.c
	@mkdir -p build
	gcc $^ -o build/rmp $(CFLAGS)

.PHONY: clean
clean:
	rm -fr build

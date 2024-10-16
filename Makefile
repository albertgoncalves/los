MAKEFLAGS += --silent
CC = mold -run clang
CFLAGS = \
	-D_DEFAULT_SOURCE \
	-ferror-limit=1 \
	-fsanitize=bounds \
	-fsanitize=float-divide-by-zero \
	-fsanitize=implicit-conversion \
	-fsanitize=integer \
	-fsanitize=nullability \
	-fsanitize=undefined \
	-fshort-enums \
	-lGL \
	-lglfw \
	-lm \
	-march=native \
	-O3 \
	-std=c99 \
	-Werror \
	-Weverything \
	-Wno-c23-extensions \
	-Wno-declaration-after-statement \
	-Wno-padded \
	-Wno-unsafe-buffer-usage

.PHONY: all
all: bin/main

.PHONY: clean
clean:
	rm -rf bin/

.PHONY: run
run: all
	./bin/main

bin/main: src/main.c
	mkdir -p bin/
	clang-format -i src/*.glsl src/*.c
	$(CC) $(CFLAGS) -o bin/main src/main.c

MAKEFLAGS += --silent
CC = mold -run clang
CFLAGS = \
	-D_DEFAULT_SOURCE \
	-ferror-limit=1 \
	-fsanitize=bounds \
	-fsanitize=float-divide-by-zero \
	-fsanitize-ignorelist=ignorelist.txt \
	-fsanitize=implicit-conversion \
	-fsanitize=integer \
	-fsanitize=nullability \
	-fsanitize=undefined \
	-fshort-enums \
	-g \
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

.PHONY: profile
profile: all
	sudo sh -c "echo 1 > /proc/sys/kernel/perf_event_paranoid"
	sudo sh -c "echo 0 > /proc/sys/kernel/kptr_restrict"
	perf record --call-graph fp ./bin/main
	perf report
	rm perf.data*

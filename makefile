CFLAGS=-Os -Wall -Wextra -pedantic -std=c11

scm: src/main.c src/util.c
	$(CC) $^ -o $@

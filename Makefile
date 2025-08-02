all:
	gcc src/basic.c src/file.c src/parse.c src/assemble.c src/main.c -o wl -Wall -Wextra -O0 -g3
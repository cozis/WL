all: WL.c
	gcc WL.c main.c -o wl -Wall -Wextra -O0 -g3

WL.c: $(wildcard src/*.c src/*.h)
	py amalg.py

AFL_CC ?= afl-clang-fast
AFL_CXX ?= afl-clang-fast++

.PHONY: all coverage coverage_report install uninstall

all: wl

wl: wl.c wl.h main.c
	gcc main.c wl.c -o wl   -g3 -O0

test: wl.c wl.h test.c
	gcc test.c wl.c -o test -g3 -O0

coverage:
	gcc main.c wl.c -o wl_cov   -g3 -O0 --coverage -fprofile-arcs -ftest-coverage
	gcc test.c wl.c -o test_cov -g3 -O0 --coverage -fprofile-arcs -ftest-coverage

coverage_report:
	gcov wl.c main.c test.c
	lcov --capture --directory . --output-file coverage.info
	lcov --remove coverage.info '/usr/*' --output-file coverage_filtered.info
	genhtml coverage_filtered.info --output-directory coverage_html --title "WL Template Engine Coverage"

fuzz: fuzz.c wl.c wl.h
	$(AFL_CC) fuzz.c wl.c -o fuzz \
		-g -O3 -Wall \
		-fsanitize=address,undefined \
		-fno-omit-frame-pointer

bugs: fuzz
	AFL_SKIP_CPUFREQ=1 afl-fuzz \
	-i fuzz_input \
	-o /tmp/fuzz_output \
	-m none \
	./fuzz

install: wl
	sudo cp wl /usr/local/bin

uninstall:
	sudo rm /usr/local/bin/wl

clean:
	rm *.gcda *.gcno *.info

all:
	gcc mylz.c -o mylz

build-debug:
	gcc mylz.c -o mylz -O0 -ggdb
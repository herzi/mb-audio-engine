all: demo

demo: main.c
	gcc -o $@ $^ $(shell pkg-config --cflags --libs gtk+-2.0 gstreamer-0.10) -ltermcap

check-valgrind:
	valgrind --leak-check=full ./demo

check: check-valgrind


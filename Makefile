all: demo

demo: main.c
	gcc -o $@ $^ $(shell pkg-config --cflags --libs gtk+-2.0)

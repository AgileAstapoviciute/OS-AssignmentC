CC=gcc
CFLAGS=-Wall -Wextra
LIBS=-lpthread

all:  prodcons

clean:
	rm prodcons

prodcons: prodcons.c prodcons.h
	$(CC) $(CFLAGS) -o prodcons prodcons.c $(LIBS)
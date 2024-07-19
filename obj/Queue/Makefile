#Makefile
CC=gcc
CFLAGS=-Wall -Wextra -Werror -pedantic -std=c99
LDFLAGS=-fsanitize=address -g
LDLIBS=-lcriterion

all: test

test: queue.o testQueue.o ../loader/loader.o ../treeGen/tree.o
	$(CC) $(LDFLAGS) -o test queue.o testQueue.o ../loader/loader.o ../treeGen/tree.o $(LDLIBS)

.PHONY: clean

clean:
	rm -f *.o test
#End

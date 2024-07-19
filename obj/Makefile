CC=gcc
CFLAGS= -Wall -Wextra -pedantic -std=c99 -Wvla -Werror
LDFLAGS=-g
LDLIBS=-lcriterion

QSRC=$(wildcard Queue/*.c) treeGen/tree.c $(wildcard Loader/*.c)
QOBJ=$(QSRC:.c=.o)
SRC=$(wildcard Loader/*.c) treeGen/tree.c main.c
OBJ= $(SRC:.c=.o)

all : test qtest

qtest : $(QOBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o qtest $(QOBJ) $(LDLIBS)

test : ${OBJ}
	$(CC) $(CFLAGS) $(LDLFLAGS) -o test $(OBJ)

.PHONY : clean

clean:
	rm $(OBJ) $(QOBJ) qtest test

CC=gcc
CFLAGS= -Wall -Wextra -g -pedantic -std=c99 -fsanitize=address

SRC=$(wildcard Loader/*.c) treeGen/tree.c main.c
OBJ= $(SRC:.c=.o)

test : ${OBJ}
	$(CC) $(CFLAGS) -o test $(OBJ)

.PHONY : clean

clean:
	rm $(OBJ)
	rm test

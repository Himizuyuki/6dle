CC=gcc
CFLAGS= -Wall -Wextra -std=c99 -pedantic -fsanitize=address -g

SRC = $(wildcard *.c) 
OBJ = $(SRC:.c=.o)

main: ${OBJ}
	$(CC) $(CFLAGS) -o main ${OBJ}

.PHONY: clean

clean:
	rm  $(OBJ) main

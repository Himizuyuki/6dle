CC=gcc
CFLAGS= -Wall -Wextra -std=c99 -pedantic -fsanitize=address

SRC= main.c src/game.c obj/treeGen/tree.c obj/Loader/loader.c
OBJ=$(SRC:.c=.o)

all = main clean

main : $(OBJ)
	$(CC) $(CFLAGS) -o main $(OBJ)

.PHONY : clean

clean:
	rm $(OBJ)
	rm main

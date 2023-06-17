CC=gcc
CFLAGS= -Wall -Wextra -std=c99 -pedantic -fsanitize=address

SRC= main.c src/game.c obj/treeGen/tree.c obj/Loader/loader.c obj/random.c
OBJ=$(SRC:.c=.o)

all = game clean

game : $(OBJ)
	$(CC) $(CFLAGS) -o game $(OBJ)

.PHONY : clean

clean:
	rm $(OBJ)
	rm game

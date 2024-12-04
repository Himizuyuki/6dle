CC=gcc
CFLAGS= -Wall -Wextra -std=c99 -pedantic #-fsanitize=address

SRC= main.c src/game.c obj/treeGen/tree.c obj/Loader/loader.c obj/random.c solver/solver.c
OBJ=$(SRC:.c=.o)

all = game

game : $(OBJ)
	$(CC) $(CFLAGS) -o game $(OBJ)

debug: CFLAGS+=-g
debug: game

.PHONY : clean

clean:
	rm $(OBJ)
	rm game

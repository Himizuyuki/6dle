#ifndef GAME_H
#define GAME_H

#define maxGuesses 5

#include "../obj/treeGen/tree.h"
#include "../obj/Loader/loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <err.h>
#include <ctype.h>
#include <string.h>

typedef struct Game {
    Tree* WB; // Word Bank
    int nb_Guesses;
    char Hword[7];
    char guessedWords[maxGuesses][7];
    char found; // 0 -> false | 1 -> true
}Game;

Game* initGame(char* WBpath);

void freeGame(Game* game);

void findRandom(char* word, Tree* wb);

char* checkWord(char* input, Game* game);

void prettyPrint(char* input, char* color);

void GameLoop(char* WBpath);

#endif 

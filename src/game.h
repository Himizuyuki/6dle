#ifndef GAME_H
#define GAME_H

#define maxGuesses 7

#include "../obj/treeGen/tree.h"
#include "../obj/Loader/loader.h"
#include "../obj/random.h"
#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <err.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <termios.h>

typedef struct Game {
    Tree* WB; // Word Bank
    Tree* Solutions; //Correct Words
    int nb_Guesses;
    char Hword[7];
    char guessedWords[maxGuesses][7];
    char colorWords[maxGuesses][6];
    char colorLetters[26];
    char found; // 0 -> false | 1 -> true
}Game;

Game* initGame(char* WBpath);

void freeGame(Game* game);

void findRandom(char* word, Tree* wb);

char* checkWord(char* input, Game* game);

void prettyPrint(Game* game);

void GameLoop(char* WBpath);

#endif 

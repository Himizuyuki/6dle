#ifndef GAME_H
#define GAME_H

#include "pattern.h"
#include "rng.h"
#include "words.h"

#define MAX_GUESSES 7

typedef enum GameStatus
{
    GAME_PLAYING,
    GAME_WON,
    GAME_LOST,
} GameStatus;

typedef enum GuessResult
{
    GUESS_OK,
    GUESS_BAD_LENGTH, // not WORD_LEN letters
    GUESS_NOT_A_WORD, // not in the word list
    GUESS_REPEATED, // already guessed this game
    GUESS_GAME_OVER, // the game is already won or lost
} GuessResult;

// What the on-screen keyboard knows about a letter. Ordered so a better
// colour always overrides a worse one.
typedef enum KeyState
{
    KEY_UNKNOWN = 0,
    KEY_ABSENT,
    KEY_PRESENT,
    KEY_CORRECT,
} KeyState;

// Pure game state: no I/O, shared by the GTK and terminal front ends and the
// solver.
typedef struct Game
{
    const WordList *words;
    char secret[WORD_LEN + 1];
    char guesses[MAX_GUESSES][WORD_LEN + 1];
    Pattern patterns[MAX_GUESSES];
    int nguesses;
    GameStatus status;
    KeyState keys[ALPHABET];
} Game;

// Start a game with a given secret (must be WORD_LEN letters; it does not
// have to be in the list, which lets tests pick any word).
void game_start(Game *game, const WordList *words, const char *secret);

// Start a game with a secret drawn uniformly from the word list.
void game_start_random(Game *game, const WordList *words, Rng *rng);

GuessResult game_guess(Game *game, const char *word);

const char *guess_result_message(GuessResult result);

#endif // GAME_H

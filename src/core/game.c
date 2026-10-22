#include "game.h"

#include <ctype.h>
#include <string.h>

void game_start(Game *game, const WordList *words, const char *secret)
{
    memset(game, 0, sizeof(*game));
    game->words = words;
    for (size_t i = 0; i < WORD_LEN; i++)
        game->secret[i] = (char)tolower((unsigned char)secret[i]);
    game->status = GAME_PLAYING;
}

void game_start_random(Game *game, const WordList *words, const Prior *prior,
                       Rng *rng)
{
    size_t index =
        prior != NULL ? prior_sample(prior, rng) : rng_below(rng, words->count);
    // Uniform draw: skip the words that must never be the secret.
    for (size_t tries = 0; prior == NULL && !word_can_be_secret(words, index)
         && tries < words->count;
         tries++)
        index = rng_below(rng, words->count);
    game_start(game, words, words->items[index]);
}

static void update_keys(Game *game, const char *guess, Pattern p)
{
    Mark marks[WORD_LEN];
    pattern_to_marks(p, marks);
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        KeyState state = marks[i] == MARK_CORRECT ? KEY_CORRECT
            : marks[i] == MARK_PRESENT            ? KEY_PRESENT
                                                  : KEY_ABSENT;
        KeyState *key = &game->keys[guess[i] - 'a'];
        if (state > *key)
            *key = state;
    }
}

GuessResult game_guess(Game *game, const char *word)
{
    if (game->status != GAME_PLAYING)
        return GUESS_GAME_OVER;
    if (strlen(word) != WORD_LEN)
        return GUESS_BAD_LENGTH;

    char guess[WORD_LEN + 1];
    for (size_t i = 0; i <= WORD_LEN; i++)
        guess[i] = (char)tolower((unsigned char)word[i]);
    if (wordlist_find(game->words, guess) < 0)
        return GUESS_NOT_A_WORD;
    for (int i = 0; i < game->nguesses; i++)
    {
        if (strcmp(game->guesses[i], guess) == 0)
            return GUESS_REPEATED;
    }

    Pattern p = pattern_compute(guess, game->secret);
    memcpy(game->guesses[game->nguesses], guess, WORD_LEN + 1);
    game->patterns[game->nguesses] = p;
    game->nguesses++;
    update_keys(game, guess, p);

    if (p == PATTERN_WIN)
        game->status = GAME_WON;
    else if (game->nguesses == MAX_GUESSES)
        game->status = GAME_LOST;
    return GUESS_OK;
}

const char *guess_result_message(GuessResult result)
{
    switch (result)
    {
    case GUESS_OK:
        return "";
    case GUESS_BAD_LENGTH:
        return "Not enough letters";
    case GUESS_NOT_A_WORD:
        return "Not in word list";
    case GUESS_REPEATED:
        return "Already guessed";
    case GUESS_GAME_OVER:
        return "The game is over";
    }
    return "";
}

#ifndef KNOWLEDGE_H
#define KNOWLEDGE_H

#include <stddef.h>
#include <stdint.h>

#include "core/game.h"
#include "core/pattern.h"
#include "core/words.h"

// Everything the solver has learned from the feedback so far: the exact set
// of words still consistent with every (guess, pattern) pair, how likely each
// of them is to be the secret, and the letter constraints in a form the
// neural network can read.
typedef struct Knowledge
{
    const WordList *words;
    const float *prior; // weight of each word as a secret (NULL: uniform)
    uint32_t *cands; // indices of the words still possible, sorted
    size_t ncands;
    double mass; // total prior weight of the candidates
    float *post; // posterior of every word: P(secret = w | feedback)
    uint32_t guessed[MAX_GUESSES]; // indices of the words played so far
    int turn; // number of guesses applied
    signed char green[WORD_LEN]; // known letter at each position, or -1
    unsigned char excluded[WORD_LEN][ALPHABET]; // letter is not at position
    unsigned char min_count[ALPHABET]; // word has at least this many
    unsigned char max_count[ALPHABET]; // and at most this many
} Knowledge;

// prior: weights of the words as secrets, all > 0 (NULL: uniform). They need
// not sum to 1, and the array must outlive k. The solver itself plays with a
// uniform prior: weighting its choices by word frequency made it slightly
// worse (see README), so priors only decide the secret and the displayed
// odds.
int knowledge_init(Knowledge *k, const WordList *words, const float *prior);
void knowledge_reset(Knowledge *k);
void knowledge_free(Knowledge *k);

// Record the feedback of a guess and drop every candidate that would not
// have produced exactly that pattern. The posterior is renormalised over the
// survivors (Bayes' rule: the feedback is either possible or not).
void knowledge_apply(Knowledge *k, const char *guess, Pattern p);

// 1 if the word (index) was already played.
int knowledge_was_guessed(const Knowledge *k, uint32_t word);

// Input vector of the network, see knowledge.c for the layout.
#define FEATURE_DIM 628
void knowledge_features(const Knowledge *k, float *x);

// A word is fed to the output layer as a sparse vector. Static part: letter
// at each position (156), letter present (26), letter present twice or more
// (26). Dynamic part, which depends on the game: the word is still possible,
// and its posterior probability.
#define WORD_FEATURE_STATIC 208
#define WORD_FEATURE_CANDIDATE 208
#define WORD_FEATURE_POSTERIOR 209
#define WORD_FEATURE_DIM 210
#define WORD_FEATURE_MAX 16
// Fills idx with the active static entries; returns how many.
int word_features(const char *word, uint16_t idx[WORD_FEATURE_MAX]);

#endif // KNOWLEDGE_H

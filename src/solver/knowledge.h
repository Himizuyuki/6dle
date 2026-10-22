#ifndef KNOWLEDGE_H
#define KNOWLEDGE_H

#include <stddef.h>
#include <stdint.h>

#include "core/pattern.h"
#include "core/words.h"

// Everything the solver has learned from the feedback so far: the exact set
// of words still consistent with every (guess, pattern) pair, plus the
// letter constraints in a form the neural network can read.
typedef struct Knowledge
{
    const WordList *words;
    uint32_t *cands; // indices of the words still possible, sorted
    size_t ncands;
    int turn; // number of guesses applied
    signed char green[WORD_LEN]; // known letter at each position, or -1
    unsigned char excluded[WORD_LEN][ALPHABET]; // letter is not at position
    unsigned char min_count[ALPHABET]; // word has at least this many
    unsigned char max_count[ALPHABET]; // and at most this many
} Knowledge;

int knowledge_init(Knowledge *k, const WordList *words);
void knowledge_reset(Knowledge *k);
void knowledge_free(Knowledge *k);

// Record the feedback of a guess and drop every candidate that would not
// have produced exactly that pattern.
void knowledge_apply(Knowledge *k, const char *guess, Pattern p);

// Input vector of the network, see knowledge.c for the layout.
#define FEATURE_DIM 626
void knowledge_features(const Knowledge *k, float *x);

// A word is fed to the output layer as a sparse binary vector: letter at each
// position (156), letter present (26), letter present twice or more (26).
#define WORD_FEATURE_DIM 208
#define WORD_FEATURE_MAX 16
// Fills idx with the active entries; returns how many.
int word_features(const char *word, uint16_t idx[WORD_FEATURE_MAX]);

#endif // KNOWLEDGE_H

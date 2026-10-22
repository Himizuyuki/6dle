#ifndef PRIOR_H
#define PRIOR_H

#include <stddef.h>

#include "rng.h"
#include "words.h"

// How the secret word is picked: the difficulty levels. Everything except
// PRIOR_UNIFORM needs word frequencies (words->zipf).
typedef enum PriorKind
{
    PRIOR_UNIFORM, // "any word": all 22k words equally likely
    PRIOR_EASY, // familiar words with few look-alikes
    PRIOR_NORMAL, // common words, rarer ones now and then
    PRIOR_HARD, // less familiar words, and familiar "trap" words
} PriorKind;

#define PRIOR_KINDS 4

// A word is familiar from this Zipf frequency (about 1 use in 3 million
// words: "aviary", "myopia").
#define PRIOR_FAMILIAR 2.5f

// PRIOR_NORMAL weight: a smooth step on the Zipf frequency,
//   sigmoid((zipf - MIDPOINT) / WIDTH) + FLOOR
// Words around Zipf 3 and above ("bistro", "condor") get ~1, rare words
// ("augury", "aaliis") only FLOOR: about 73% of secrets have Zipf >= 3 and
// 0.5% are words the frequency corpus has never seen.
#define PRIOR_MIDPOINT 2.75f
#define PRIOR_WIDTH 0.25f
#define PRIOR_FLOOR 0.002f

// PRIOR_EASY: Zipf >= 3.5 and at most 2 familiar words one letter away.
// PRIOR_HARD: Zipf in [2, 3), or Zipf >= 3 with 5+ familiar words one
// letter away (BATTER: BETTER, BITTER, BUTTER, MATTER, LATTER...).
// Words outside the level keep a tiny weight so that any secret (e.g. one
// typed in the Solver tab) still has a defined probability.
#define PRIOR_EASY_ZIPF 3.5f
#define PRIOR_EASY_NEIGHBOURS 2
#define PRIOR_HARD_ZIPF_LOW 2.0f
#define PRIOR_HARD_ZIPF_HIGH 3.0f
#define PRIOR_TRAP_NEIGHBOURS 5
#define PRIOR_OUTSIDE 1e-6f

typedef struct Prior
{
    PriorKind kind;
    size_t count;
    float *weight; // probability of each word, sums to 1, all > 0
    double *cumulative; // running sum of weight, for sampling
} Prior;

// Returns -1 on allocation failure, or if kind needs frequencies and
// words->zipf is NULL.
int prior_init(Prior *prior, const WordList *words, PriorKind kind);
void prior_free(Prior *prior);

// Draw a word index according to the weights.
size_t prior_sample(const Prior *prior, Rng *rng);

// Short name for menus ("Easy", "Normal", "Hard", "Any word").
const char *prior_name(PriorKind kind);

// For every word, how many familiar words (Zipf >= PRIOR_FAMILIAR) differ
// from it in exactly one position. words->zipf must be set. Returns -1 on
// allocation failure.
int prior_neighbours(const WordList *words, unsigned *out);

#endif // PRIOR_H

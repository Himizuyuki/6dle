#ifndef EXPERT_H
#define EXPERT_H

#include <stddef.h>
#include <stdint.h>

#include "core/words.h"
#include "knowledge.h"

// The "expert" is a classic look-ahead-free solver. For a guess, it groups
// the possible answers by the colours the guess would show, then estimates
// how many guesses each group still needs. It is exact about the groups but
// costs O(guesses x answers) pattern computations per move (~0.5 billion
// for the very first move), so it is used to teach the neural network
// offline and to analyse openers, not in the GUI.

typedef struct GuessStats
{
    double entropy; // expected information, in bits
    double cost; // expected number of guesses to finish, this one included
    double expected_left; // expected number of words left afterwards
    double worst_left; // largest group of words giving the same colours
    double win; // probability that the guess is the secret
} GuessStats;

// Statistics of guess over the answers. weights: probability of each answer
// (any positive scale; NULL: all equal).
void expert_stats(const WordList *words, const char *guess,
                  const uint32_t *answers, const float *weights,
                  size_t nanswers, GuessStats *out);

// Entropy, in bits, of the colours of guess over equally likely answers.
double expert_entropy(const WordList *words, const char *guess,
                      const uint32_t *answers, size_t nanswers);

// out[i] = expert_stats(words[guesses[i]], ...).cost, computed in one pass.
void expert_costs(const WordList *words, const uint32_t *guesses,
                  size_t nguesses, const uint32_t *answers,
                  const float *weights, size_t nanswers, float *out);

// Estimated guesses still needed, the winning one included, when the
// secret is one of `effective_words` equally likely words. Measured from the
// expert's own games: 1 for one word, 1.5 for two, ~2.3 for 16, ~3.7 for the
// whole list.
double expert_remaining(double effective_words);

typedef struct ExpertOptions
{
    int allow_probes; // consider words that can't be the secret
    size_t shortlist_words; // best words overall kept for exact scoring
    size_t shortlist_cands; // best candidates kept for exact scoring
    size_t max_answers; // answers sampled to estimate the costs
} ExpertOptions;

// Default options used for training.
ExpertOptions expert_default_options(void);

// The expert's view of the current position: fills pool with the words it
// scored exactly (a shortlist of the most informative words, plus the
// candidates, see ExpertOptions) and cost with their expected number of
// guesses. Never includes words already played. pool and cost need room for
// shortlist_words + shortlist_cands entries. Returns the pool size.
size_t expert_evaluate(const Knowledge *k, const ExpertOptions *opt,
                       uint32_t *pool, float *cost);

// Turn values into a probability distribution peaked on the best ones:
// p[i] is proportional to exp((v[i] - max v) / temperature).
void expert_soft_targets(const float *v, size_t n, float temperature, float *p);

#endif // EXPERT_H

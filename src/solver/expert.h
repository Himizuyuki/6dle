#ifndef EXPERT_H
#define EXPERT_H

#include <stddef.h>
#include <stdint.h>

#include "core/words.h"

// The "expert" is the classic information-theory solver: the best guess is
// the one whose feedback splits the remaining words most evenly, i.e. has the
// highest Shannon entropy. It is exact but costs O(guesses * answers) pattern
// computations per move (~0.5 billion for the very first move), so it is used
// to teach the neural network offline, not in the GUI.

// Entropy, in bits, of the feedback pattern of guess over the answers.
double expert_entropy(const WordList *words, const char *guess,
                      const uint32_t *answers, size_t nanswers);

// out[i] = expert_entropy(words[guesses[i]], answers).
void expert_scores(const WordList *words, const uint32_t *guesses,
                   size_t nguesses, const uint32_t *answers, size_t nanswers,
                   float *out);

// Turn entropies into a probability distribution peaked on the best guesses:
// p[i] is proportional to exp((h[i] - max h) / temperature).
void expert_soft_targets(const float *entropy, size_t n, float temperature,
                         float *p);

#endif // EXPERT_H

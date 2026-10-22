#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h>
#include <stdint.h>

#include "knowledge.h"
#include "nn.h"

/*
 * Neural network solver.
 *
 * The network reads the knowledge features (FEATURE_DIM inputs) and outputs a
 * "query" vector of WORD_FEATURE_DIM values. Each still-possible word w is
 * scored with query . features(w), and a softmax over those scores gives the
 * probability that w is the best next guess. Words ruled out by the feedback
 * get probability 0, so the solver never wastes a guess on an impossible word.
 *
 * Scoring words through their letter features (instead of one output neuron
 * per word) keeps the model small (~1 MB) and makes it work with any word
 * list. It is trained to imitate the entropy expert (see expert.h), see
 * tools/train.c.
 */

typedef struct Suggestion
{
    uint32_t word; // index in the word list
    float prob; // probability the network gives it, in [0, 1]
} Suggestion;

typedef struct Solver
{
    const WordList *words;
    const Mlp *model;
    Knowledge k;
    uint16_t (*wfeat)[WORD_FEATURE_MAX]; // features of every word
    unsigned char *nwfeat;
    float x[FEATURE_DIM];
    MlpWork work;
    float *probs; // probs[i] belongs to k.cands[i]
    int evaluated; // probs are up to date with k
} Solver;

// Returns -1 on allocation failure or if the model does not have
// FEATURE_DIM inputs and WORD_FEATURE_DIM outputs.
int solver_init(Solver *s, const WordList *words, const Mlp *model);
void solver_free(Solver *s);
void solver_reset(Solver *s);

// Feed back the result of a guess.
void solver_apply(Solver *s, const char *guess, Pattern p);

size_t solver_candidates(const Solver *s);

// Run the network on the current state (fills probs).
void solver_evaluate(Solver *s);

// The n most probable guesses, best first. Returns how many were written.
size_t solver_top(Solver *s, Suggestion *out, size_t n);

#define SOLVER_NO_WORD UINT32_MAX

// Index of the word to play next (the most probable one), or SOLVER_NO_WORD
// if no word fits the feedback (only possible if the secret is not in the
// word list).
uint32_t solver_best(Solver *s);

// Training: given the target distribution over k.cands, add the gradient of
// the cross-entropy loss to grad and return the loss. Must follow
// solver_evaluate on the same state.
float solver_backward(Solver *s, const float *target, float *grad);

#endif // SOLVER_H

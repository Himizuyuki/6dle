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
 * "query" vector of WORD_FEATURE_DIM values. Each word w is scored with
 * query . features(w), where the features say which letters w has where,
 * whether w is still possible and how likely it is to be the secret. A
 * softmax over the scores gives the probability that w is the best next
 * guess.
 *
 * The network plays as if every word were equally likely to be the secret,
 * whatever the difficulty: weighting its choices by word frequency made it
 * slightly worse (see README). The difficulty's prior is only used for the
 * exact "most likely secret" odds (solver_top_answers).
 *
 * With probes allowed, every word not played yet may be chosen, including
 * words that can't be the secret but split the remaining ones well. Without
 * (hard mode), and always once two words or fewer remain, only still
 * possible words are considered.
 *
 * Scoring words through their features (instead of one output neuron per
 * word) keeps the model small (~1 MB) and makes it work with any word list.
 * It is trained to imitate the expert (see expert.h), see tools/train.c.
 */

typedef struct Suggestion
{
    uint32_t word; // index in the word list
    float prob; // probability, in [0, 1]
} Suggestion;

typedef struct Solver
{
    const WordList *words;
    const Mlp *model;
    Knowledge k; // uniform: the network ignores the prior
    const float *answer_prior; // for solver_top_answers (NULL: uniform)
    int allow_probes;
    uint16_t (*wfeat)[WORD_FEATURE_MAX]; // static features of every word
    unsigned char *nwfeat;
    float x[FEATURE_DIM];
    MlpWork work;
    const float *query; // network output of the last evaluation
    float *scores; // scores[w], valid for the words of the pool
    uint32_t *pool; // words the next guess is chosen from
    size_t npool;
    float *probs; // probs[i] belongs to pool[i]
    float dquery[WORD_FEATURE_DIM]; // training: accumulated gradient
    int evaluated; // pool and probs are up to date with k
} Solver;

// prior: weight of each word as a secret, all > 0 (NULL: uniform), used for
// the odds of solver_top_answers; it must outlive s. Returns -1 on
// allocation failure or if the model does not have FEATURE_DIM inputs and
// WORD_FEATURE_DIM outputs. Probes are allowed by default.
int solver_init(Solver *s, const WordList *words, const Mlp *model,
                const float *prior);
void solver_free(Solver *s);
void solver_reset(Solver *s);
void solver_set_probes(Solver *s, int allow);

// Switch to another prior (as in solver_init) and start over.
void solver_set_prior(Solver *s, const float *prior);

// Feed back the result of a guess.
void solver_apply(Solver *s, const char *guess, Pattern p);

size_t solver_candidates(const Solver *s);
int solver_is_candidate(const Solver *s, uint32_t word);

// Run the network on the current state (fills pool and probs).
void solver_evaluate(Solver *s);

// The n most probable next guesses, best first. Returns how many were
// written.
size_t solver_top(Solver *s, Suggestion *out, size_t n);

// The n words most likely to be the secret, with their exact probability
// given the feedback (Bayes' rule on the prior), best first.
size_t solver_top_answers(const Solver *s, Suggestion *out, size_t n);

#define SOLVER_NO_WORD UINT32_MAX

// Index of the word to play next (the most probable one), or SOLVER_NO_WORD
// if no word fits the feedback (only possible if the secret is not in the
// word list).
uint32_t solver_best(Solver *s);

// Training. solver_loss adds the gradient of the cross-entropy between the
// current pool's probabilities and target (aligned with pool) to an internal
// buffer and returns the loss; it may be called once per pool (e.g. with and
// without probes). solver_backprop then adds the parameter gradients to grad
// and clears the buffer. Both must follow solver_evaluate on the same state.
float solver_loss(Solver *s, const float *target);
void solver_backprop(Solver *s, float *grad);

#endif // SOLVER_H

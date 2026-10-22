#include "solver.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int solver_init(Solver *s, const WordList *words, const Mlp *model)
{
    memset(s, 0, sizeof(*s));
    if (model->sizes[0] != FEATURE_DIM
        || model->sizes[model->nlayers] != WORD_FEATURE_DIM)
        return -1;
    s->words = words;
    s->model = model;
    s->wfeat = malloc(words->count * sizeof(*s->wfeat));
    s->nwfeat = malloc(words->count);
    s->probs = malloc(words->count * sizeof(*s->probs));
    if (s->wfeat == NULL || s->nwfeat == NULL || s->probs == NULL
        || knowledge_init(&s->k, words) != 0
        || mlp_work_init(&s->work, model) != 0)
    {
        solver_free(s);
        return -1;
    }
    for (size_t i = 0; i < words->count; i++)
        s->nwfeat[i] =
            (unsigned char)word_features(words->items[i], s->wfeat[i]);
    return 0;
}

void solver_free(Solver *s)
{
    knowledge_free(&s->k);
    mlp_work_free(&s->work);
    free(s->wfeat);
    free(s->nwfeat);
    free(s->probs);
    s->wfeat = NULL;
    s->nwfeat = NULL;
    s->probs = NULL;
}

void solver_reset(Solver *s)
{
    knowledge_reset(&s->k);
    s->evaluated = 0;
}

void solver_apply(Solver *s, const char *guess, Pattern p)
{
    knowledge_apply(&s->k, guess, p);
    s->evaluated = 0;
}

size_t solver_candidates(const Solver *s)
{
    return s->k.ncands;
}

void solver_evaluate(Solver *s)
{
    knowledge_features(&s->k, s->x);
    const float *query = mlp_forward(s->model, &s->work, s->x);

    float best = -INFINITY;
    for (size_t c = 0; c < s->k.ncands; c++)
    {
        uint32_t w = s->k.cands[c];
        float score = 0.0f;
        for (int j = 0; j < s->nwfeat[w]; j++)
            score += query[s->wfeat[w][j]];
        s->probs[c] = score;
        if (score > best)
            best = score;
    }
    double total = 0.0;
    for (size_t c = 0; c < s->k.ncands; c++)
    {
        s->probs[c] = expf(s->probs[c] - best);
        total += s->probs[c];
    }
    for (size_t c = 0; c < s->k.ncands; c++)
        s->probs[c] = (float)(s->probs[c] / total);
    s->evaluated = 1;
}

size_t solver_top(Solver *s, Suggestion *out, size_t n)
{
    if (!s->evaluated)
        solver_evaluate(s);
    if (n > s->k.ncands)
        n = s->k.ncands;
    if (n == 0)
        return 0;
    // Partial insertion sort: n is tiny (the GUI shows 5).
    size_t filled = 0;
    for (size_t c = 0; c < s->k.ncands; c++)
    {
        Suggestion cur = { s->k.cands[c], s->probs[c] };
        if (filled == n && !(cur.prob > out[n - 1].prob))
            continue;
        size_t pos = filled < n ? filled++ : n - 1;
        while (pos > 0 && out[pos - 1].prob < cur.prob)
        {
            out[pos] = out[pos - 1];
            pos--;
        }
        out[pos] = cur;
    }
    return filled;
}

uint32_t solver_best(Solver *s)
{
    Suggestion top;
    return solver_top(s, &top, 1) ? top.word : SOLVER_NO_WORD;
}

float solver_backward(Solver *s, const float *target, float *grad)
{
    float dquery[WORD_FEATURE_DIM] = { 0 };
    float loss = 0.0f;
    for (size_t c = 0; c < s->k.ncands; c++)
    {
        float diff = s->probs[c] - target[c];
        if (target[c] > 0.0f)
            loss -= target[c] * logf(s->probs[c] + 1e-12f);
        if (diff == 0.0f)
            continue;
        uint32_t w = s->k.cands[c];
        for (int j = 0; j < s->nwfeat[w]; j++)
            dquery[s->wfeat[w][j]] += diff;
    }
    mlp_backward(s->model, &s->work, dquery, grad);
    return loss;
}

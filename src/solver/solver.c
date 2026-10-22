#include "solver.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int solver_init(Solver *s, const WordList *words, const Mlp *model,
                const float *prior)
{
    memset(s, 0, sizeof(*s));
    if (model->sizes[0] != FEATURE_DIM
        || model->sizes[model->nlayers] != WORD_FEATURE_DIM)
        return -1;
    s->words = words;
    s->model = model;
    s->answer_prior = prior;
    s->allow_probes = 1;
    s->wfeat = malloc(words->count * sizeof(*s->wfeat));
    s->nwfeat = malloc(words->count);
    s->scores = malloc(words->count * sizeof(*s->scores));
    s->pool = malloc(words->count * sizeof(*s->pool));
    s->probs = malloc(words->count * sizeof(*s->probs));
    if (s->wfeat == NULL || s->nwfeat == NULL || s->scores == NULL
        || s->pool == NULL || s->probs == NULL
        || knowledge_init(&s->k, words, NULL) != 0
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
    free(s->scores);
    free(s->pool);
    free(s->probs);
    s->wfeat = NULL;
    s->nwfeat = NULL;
    s->scores = NULL;
    s->pool = NULL;
    s->probs = NULL;
}

void solver_reset(Solver *s)
{
    knowledge_reset(&s->k);
    s->evaluated = 0;
}

void solver_set_prior(Solver *s, const float *prior)
{
    s->answer_prior = prior;
    solver_reset(s);
}

static void build_pool(Solver *s)
{
    s->npool = 0;
    if (s->allow_probes && s->k.ncands > 2)
    {
        for (uint32_t w = 0; w < s->words->count; w++)
        {
            if (!knowledge_was_guessed(&s->k, w))
                s->pool[s->npool++] = w;
        }
    }
    else
    {
        memcpy(s->pool, s->k.cands, s->k.ncands * sizeof(*s->pool));
        s->npool = s->k.ncands;
    }
}

static float score_word(const Solver *s, uint32_t w)
{
    float score = 0.0f;
    for (int j = 0; j < s->nwfeat[w]; j++)
        score += s->query[s->wfeat[w][j]];
    float post = s->k.post[w];
    if (post > 0.0f)
        score += s->query[WORD_FEATURE_CANDIDATE]
            + s->query[WORD_FEATURE_POSTERIOR] * post;
    return score;
}

static void softmax_pool(Solver *s)
{
    float best = -INFINITY;
    for (size_t i = 0; i < s->npool; i++)
    {
        float score = s->scores[s->pool[i]];
        if (score > best)
            best = score;
    }
    double total = 0.0;
    for (size_t i = 0; i < s->npool; i++)
    {
        s->probs[i] = expf(s->scores[s->pool[i]] - best);
        total += s->probs[i];
    }
    for (size_t i = 0; i < s->npool; i++)
        s->probs[i] = (float)(s->probs[i] / total);
}

void solver_set_probes(Solver *s, int allow)
{
    int widen = allow && !s->allow_probes;
    s->allow_probes = allow;
    if (!s->evaluated)
        return;
    if (widen)
    {
        // New words need scores: evaluate again when asked.
        s->evaluated = 0;
        return;
    }
    // The pool can only shrink: reuse the scores.
    build_pool(s);
    softmax_pool(s);
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

int solver_is_candidate(const Solver *s, uint32_t word)
{
    return s->k.post[word] > 0.0f;
}

void solver_evaluate(Solver *s)
{
    knowledge_features(&s->k, s->x);
    s->query = mlp_forward(s->model, &s->work, s->x);
    build_pool(s);
    for (size_t i = 0; i < s->npool; i++)
        s->scores[s->pool[i]] = score_word(s, s->pool[i]);
    softmax_pool(s);
    s->evaluated = 1;
}

// Keep the n best suggestions of a stream in out, best first.
static void insert_top(Suggestion *out, size_t n, size_t *filled,
                       Suggestion cur)
{
    if (*filled == n && !(cur.prob > out[n - 1].prob))
        return;
    size_t pos = *filled < n ? (*filled)++ : n - 1;
    while (pos > 0 && out[pos - 1].prob < cur.prob)
    {
        out[pos] = out[pos - 1];
        pos--;
    }
    out[pos] = cur;
}

size_t solver_top(Solver *s, Suggestion *out, size_t n)
{
    if (!s->evaluated)
        solver_evaluate(s);
    if (n == 0)
        return 0;
    size_t filled = 0;
    for (size_t i = 0; i < s->npool; i++)
        insert_top(out, n, &filled, (Suggestion){ s->pool[i], s->probs[i] });
    return filled;
}

size_t solver_top_answers(const Solver *s, Suggestion *out, size_t n)
{
    if (n == 0 || s->k.ncands == 0)
        return 0;
    // P(secret = w | feedback) = prior(w) / sum of prior over candidates.
    double mass = 0.0;
    for (size_t i = 0; i < s->k.ncands; i++)
        mass += s->answer_prior ? s->answer_prior[s->k.cands[i]] : 1.0;
    // Only zero-weight words left (e.g. a never-secret word typed as the
    // secret in the GUI): fall back to equal odds.
    const float *prior = mass > 0.0 ? s->answer_prior : NULL;
    if (prior == NULL)
        mass = (double)s->k.ncands;
    size_t filled = 0;
    for (size_t i = 0; i < s->k.ncands; i++)
    {
        uint32_t w = s->k.cands[i];
        double weight = prior ? prior[w] : 1.0;
        insert_top(out, n, &filled, (Suggestion){ w, (float)(weight / mass) });
    }
    return filled;
}

uint32_t solver_best(Solver *s)
{
    Suggestion top;
    return solver_top(s, &top, 1) ? top.word : SOLVER_NO_WORD;
}

float solver_loss(Solver *s, const float *target)
{
    float loss = 0.0f;
    for (size_t i = 0; i < s->npool; i++)
    {
        float diff = s->probs[i] - target[i];
        if (target[i] > 0.0f)
            loss -= target[i] * logf(s->probs[i] + 1e-12f);
        if (diff == 0.0f)
            continue;
        uint32_t w = s->pool[i];
        for (int j = 0; j < s->nwfeat[w]; j++)
            s->dquery[s->wfeat[w][j]] += diff;
        float post = s->k.post[w];
        if (post > 0.0f)
        {
            s->dquery[WORD_FEATURE_CANDIDATE] += diff;
            s->dquery[WORD_FEATURE_POSTERIOR] += diff * post;
        }
    }
    return loss;
}

void solver_backprop(Solver *s, float *grad)
{
    mlp_backward(s->model, &s->work, s->dquery, grad);
    memset(s->dquery, 0, sizeof(s->dquery));
}

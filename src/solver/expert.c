#include "expert.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/pattern.h"

// Answers grouped by the colours a guess shows for them.
typedef struct Buckets
{
    uint32_t count[PATTERN_COUNT];
    double mass[PATTERN_COUNT]; // sum of the answers' weights
    double wlogw[PATTERN_COUNT]; // sum of w * ln(w), for the group entropy
    Pattern seen[PATTERN_COUNT];
    size_t nseen;
} Buckets;

// wlogw[a] = weights[a] * ln(weights[a]), precomputed by the caller (NULL
// when weights is NULL).
static void buckets_fill(Buckets *b, const WordList *words, const char *guess,
                         const uint32_t *answers, const float *weights,
                         const double *wlogw, size_t nanswers)
{
    for (size_t a = 0; a < nanswers; a++)
    {
        Pattern p = pattern_compute(guess, words->items[answers[a]]);
        if (b->count[p]++ == 0)
            b->seen[b->nseen++] = p;
        if (weights != NULL)
        {
            b->mass[p] += weights[a];
            b->wlogw[p] += wlogw[a];
        }
        else
            b->mass[p] += 1.0;
    }
}

// Summarise the groups into out, and empty them for the next guess.
static void buckets_drain(Buckets *b, int weighted, GuessStats *out)
{
    double total = 0.0;
    for (size_t i = 0; i < b->nseen; i++)
        total += b->mass[b->seen[i]];
    *out = (GuessStats){ .cost = 1.0 };
    for (size_t i = 0; i < b->nseen; i++)
    {
        Pattern p = b->seen[i];
        double m = total > 0.0 ? b->mass[p] / total : 0.0;
        if (m <= 0.0)
        {
            // Only zero-weight answers (never secrets): no contribution.
            b->count[p] = 0;
            b->mass[p] = 0.0;
            b->wlogw[p] = 0.0;
            continue;
        }
        out->entropy -= m * log2(m);
        out->expected_left += m * b->count[p];
        if (b->count[p] > out->worst_left)
            out->worst_left = b->count[p];
        if (p == PATTERN_WIN)
            out->win = m;
        else
        {
            // Effective group size: exp(entropy of the group's weights).
            double n = weighted
                ? exp(log(b->mass[p]) - b->wlogw[p] / b->mass[p])
                : b->count[p];
            out->cost += m * expert_remaining(n);
        }
        b->count[p] = 0;
        b->mass[p] = 0.0;
        b->wlogw[p] = 0.0;
    }
    b->nseen = 0;
}

static double *weights_log(const float *weights, size_t n)
{
    if (weights == NULL)
        return NULL;
    double *wlogw = malloc((n ? n : 1) * sizeof(*wlogw));
    if (wlogw == NULL)
        return NULL;
    for (size_t a = 0; a < n; a++)
        wlogw[a] = weights[a] > 0.0f ? weights[a] * log(weights[a]) : 0.0;
    return wlogw;
}

void expert_stats(const WordList *words, const char *guess,
                  const uint32_t *answers, const float *weights,
                  size_t nanswers, GuessStats *out)
{
    *out = (GuessStats){ 0 };
    if (nanswers == 0)
        return;
    Buckets *b = calloc(1, sizeof(*b));
    double *wlogw = weights_log(weights, nanswers);
    if (b == NULL || (weights != NULL && wlogw == NULL))
    {
        free(b);
        free(wlogw);
        return;
    }
    buckets_fill(b, words, guess, answers, weights, wlogw, nanswers);
    buckets_drain(b, weights != NULL, out);
    free(wlogw);
    free(b);
}

double expert_entropy(const WordList *words, const char *guess,
                      const uint32_t *answers, size_t nanswers)
{
    GuessStats stats;
    expert_stats(words, guess, answers, NULL, nanswers, &stats);
    return stats.entropy;
}

void expert_costs(const WordList *words, const uint32_t *guesses,
                  size_t nguesses, const uint32_t *answers,
                  const float *weights, size_t nanswers, float *out)
{
    Buckets *b = calloc(1, sizeof(*b));
    double *wlogw = weights_log(weights, nanswers);
    for (size_t i = 0; i < nguesses; i++)
    {
        GuessStats stats = { .cost = 1.0 };
        if (b != NULL && nanswers > 0 && (weights == NULL || wlogw != NULL))
        {
            buckets_fill(b, words, words->items[guesses[i]], answers, weights,
                         wlogw, nanswers);
            buckets_drain(b, weights != NULL, &stats);
        }
        out[i] = (float)stats.cost;
    }
    free(wlogw);
    free(b);
}

double expert_remaining(double effective_words)
{
    // Average guesses to finish, measured over 4,800 games of this expert
    // (with probes, all won), against log2(words left); linear in between.
    static const double X[] = { 0.0, 1.0, 1.585, 2.0, 3.0, 4.0,
                                5.0, 6.0, 7.0,   8.0, 9.0, 14.4 };
    static const double Y[] = { 1.00, 1.50, 1.72, 1.84, 2.10, 2.27,
                                2.47, 2.62, 2.72, 2.86, 2.98, 3.66 };
    const size_t n = sizeof(X) / sizeof(X[0]);
    if (effective_words <= 1.0)
        return 1.0;
    double x = log2(effective_words);
    for (size_t i = 1; i < n; i++)
    {
        if (x <= X[i])
            return Y[i - 1]
                + (Y[i] - Y[i - 1]) * (x - X[i - 1]) / (X[i] - X[i - 1]);
    }
    return Y[n - 1]
        + (Y[n - 1] - Y[n - 2]) / (X[n - 1] - X[n - 2]) * (x - X[n - 1]);
}

ExpertOptions expert_default_options(void)
{
    return (ExpertOptions){ .allow_probes = 1,
                            .shortlist_words = 600,
                            .shortlist_cands = 600,
                            .max_answers = 500 };
}

typedef struct Entry
{
    float score;
    uint32_t item;
} Entry;

static void sift_down(Entry *heap, size_t n, size_t i)
{
    for (;;)
    {
        size_t smallest = i;
        size_t l = 2 * i + 1;
        size_t r = l + 1;
        if (l < n && heap[l].score < heap[smallest].score)
            smallest = l;
        if (r < n && heap[r].score < heap[smallest].score)
            smallest = r;
        if (smallest == i)
            return;
        Entry tmp = heap[i];
        heap[i] = heap[smallest];
        heap[smallest] = tmp;
        i = smallest;
    }
}

// Append to out the (at most) k items with the highest score that are not
// taken yet, and mark them taken. items == NULL means 0 .. n-1.
static size_t add_top(const float *score, const uint32_t *items, size_t n,
                      size_t k, unsigned char *taken, uint32_t *out)
{
    if (k == 0)
        return 0;
    Entry *heap = malloc(k * sizeof(*heap));
    if (heap == NULL)
        return 0;
    size_t size = 0;
    for (size_t i = 0; i < n; i++)
    {
        uint32_t item = items != NULL ? items[i] : (uint32_t)i;
        float s = score[item];
        if (taken[item] || s == -INFINITY)
            continue;
        if (size < k)
        {
            // Min-heap: sift the new entry up.
            size_t j = size++;
            heap[j] = (Entry){ s, item };
            while (j > 0 && heap[(j - 1) / 2].score > heap[j].score)
            {
                Entry tmp = heap[j];
                heap[j] = heap[(j - 1) / 2];
                heap[(j - 1) / 2] = tmp;
                j = (j - 1) / 2;
            }
        }
        else if (s > heap[0].score)
        {
            heap[0] = (Entry){ s, item };
            sift_down(heap, size, 0);
        }
    }
    for (size_t i = 0; i < size; i++)
    {
        out[i] = heap[i].item;
        taken[heap[i].item] = 1;
    }
    free(heap);
    return size;
}

static float binary_entropy(float p)
{
    if (p <= 0.0f || p >= 1.0f)
        return 0.0f;
    return -(p * log2f(p) + (1.0f - p) * log2f(1.0f - p));
}

// Cheap estimate of the information of every word as a guess: how evenly
// each of its letters (and each letter-at-position) splits the candidates.
static void heuristic_scores(const Knowledge *k, float *score)
{
    float pos[WORD_LEN][ALPHABET] = { { 0 } };
    float has[ALPHABET] = { 0 };
    for (size_t c = 0; c < k->ncands; c++)
    {
        const char *w = k->words->items[k->cands[c]];
        float p = k->post[k->cands[c]];
        unsigned seen = 0;
        for (int i = 0; i < WORD_LEN; i++)
        {
            int l = w[i] - 'a';
            pos[i][l] += p;
            if (!(seen & (1u << l)))
                has[l] += p;
            seen |= 1u << l;
        }
    }
    for (int i = 0; i < WORD_LEN; i++)
        for (int l = 0; l < ALPHABET; l++)
            pos[i][l] = binary_entropy(pos[i][l]);
    for (int l = 0; l < ALPHABET; l++)
        has[l] = binary_entropy(has[l]);

    for (size_t w = 0; w < k->words->count; w++)
    {
        const char *word = k->words->items[w];
        unsigned seen = 0;
        float s = 0.0f;
        for (int i = 0; i < WORD_LEN; i++)
        {
            int l = word[i] - 'a';
            s += pos[i][l];
            if (!(seen & (1u << l)))
                s += has[l];
            seen |= 1u << l;
        }
        score[w] = s;
    }
    for (int t = 0; t < k->turn && t < MAX_GUESSES; t++)
    {
        if (k->guessed[t] != UINT32_MAX)
            score[k->guessed[t]] = -INFINITY;
    }
}

size_t expert_evaluate(const Knowledge *k, const ExpertOptions *opt,
                       uint32_t *pool, float *cost)
{
    if (k->ncands == 0)
        return 0;
    size_t nwords = k->words->count;
    float *score = malloc(nwords * sizeof(*score));
    unsigned char *taken = calloc(nwords, 1);
    size_t nanswers =
        k->ncands < opt->max_answers ? k->ncands : opt->max_answers;
    uint32_t *answers = malloc(nanswers * sizeof(*answers));
    float *weights = malloc(nanswers * sizeof(*weights));
    size_t npool = 0;
    if (score != NULL && taken != NULL && answers != NULL && weights != NULL)
    {
        heuristic_scores(k, score);
        // A probe can't beat guessing one of two remaining words.
        if (opt->allow_probes && k->ncands > 2)
            npool +=
                add_top(score, NULL, nwords, opt->shortlist_words, taken, pool);
        npool += add_top(score, k->cands, k->ncands, opt->shortlist_cands,
                         taken, pool + npool);

        // Evenly spaced subset of the candidates: an unbiased estimate.
        for (size_t i = 0; i < nanswers; i++)
        {
            answers[i] = k->cands[i * k->ncands / nanswers];
            weights[i] = k->post[answers[i]];
        }
        expert_costs(k->words, pool, npool, answers,
                     k->prior != NULL ? weights : NULL, nanswers, cost);
    }
    free(score);
    free(taken);
    free(answers);
    free(weights);
    return npool;
}

void expert_soft_targets(const float *v, size_t n, float temperature, float *p)
{
    if (n == 0)
        return;
    float best = v[0];
    for (size_t i = 1; i < n; i++)
    {
        if (v[i] > best)
            best = v[i];
    }
    double total = 0.0;
    for (size_t i = 0; i < n; i++)
    {
        p[i] = expf((v[i] - best) / temperature);
        total += p[i];
    }
    for (size_t i = 0; i < n; i++)
        p[i] = (float)(p[i] / total);
}

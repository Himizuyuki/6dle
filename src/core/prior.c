#include "prior.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Masked
{
    char key[WORD_LEN + 1]; // the word with one letter replaced by '_'
    uint32_t word;
} Masked;

static int compare_masked(const void *a, const void *b)
{
    return strcmp(((const Masked *)a)->key, ((const Masked *)b)->key);
}

int prior_neighbours(const WordList *words, unsigned *out)
{
    size_t n = words->count;
    Masked *masked = malloc(n * sizeof(*masked));
    if (masked == NULL)
        return -1;
    memset(out, 0, n * sizeof(*out));
    // Two words differ only at position i iff they are equal with that
    // letter masked: sort by masked key and count within each group.
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        for (size_t w = 0; w < n; w++)
        {
            memcpy(masked[w].key, words->items[w], WORD_LEN + 1);
            masked[w].key[i] = '_';
            masked[w].word = (uint32_t)w;
        }
        qsort(masked, n, sizeof(*masked), compare_masked);
        for (size_t start = 0; start < n;)
        {
            size_t end = start + 1;
            unsigned familiar =
                words->zipf[masked[start].word] >= PRIOR_FAMILIAR;
            while (end < n && strcmp(masked[end].key, masked[start].key) == 0)
                familiar += words->zipf[masked[end++].word] >= PRIOR_FAMILIAR;
            for (size_t j = start; j < end; j++)
                out[masked[j].word] +=
                    familiar - (words->zipf[masked[j].word] >= PRIOR_FAMILIAR);
            start = end;
        }
    }
    free(masked);
    return 0;
}

int prior_init(Prior *prior, const WordList *words, PriorKind kind)
{
    prior->kind = kind;
    prior->count = words->count;
    prior->weight = malloc(words->count * sizeof(*prior->weight));
    prior->cumulative = malloc(words->count * sizeof(*prior->cumulative));
    unsigned *neighbours = NULL;
    if (kind == PRIOR_EASY || kind == PRIOR_HARD)
        neighbours = malloc(words->count * sizeof(*neighbours));
    if (prior->weight == NULL || prior->cumulative == NULL
        || (kind != PRIOR_UNIFORM && words->zipf == NULL)
        || ((kind == PRIOR_EASY || kind == PRIOR_HARD)
            && (neighbours == NULL
                || prior_neighbours(words, neighbours) != 0)))
    {
        free(neighbours);
        prior_free(prior);
        return -1;
    }

    double total = 0.0;
    for (size_t i = 0; i < words->count; i++)
    {
        double w = 1.0;
        float zipf = kind != PRIOR_UNIFORM ? words->zipf[i] : 0.0f;
        switch (kind)
        {
        case PRIOR_UNIFORM:
            break;
        case PRIOR_NORMAL:
            w = 1.0 / (1.0 + exp(-(zipf - PRIOR_MIDPOINT) / PRIOR_WIDTH))
                + PRIOR_FLOOR;
            break;
        case PRIOR_EASY:
            w = zipf >= PRIOR_EASY_ZIPF
                    && neighbours[i] <= PRIOR_EASY_NEIGHBOURS
                ? 1.0
                : PRIOR_OUTSIDE;
            break;
        case PRIOR_HARD:
            w = (zipf >= PRIOR_HARD_ZIPF_LOW && zipf < PRIOR_HARD_ZIPF_HIGH)
                    || (zipf >= PRIOR_HARD_ZIPF_HIGH
                        && neighbours[i] >= PRIOR_TRAP_NEIGHBOURS)
                ? 1.0
                : PRIOR_OUTSIDE;
            break;
        }
        prior->cumulative[i] = total += w;
    }
    // Weights from the raw running sums first, then normalise the sums.
    for (size_t i = 0; i < words->count; i++)
    {
        double before = i ? prior->cumulative[i - 1] : 0.0;
        prior->weight[i] = (float)((prior->cumulative[i] - before) / total);
    }
    for (size_t i = 0; i < words->count; i++)
        prior->cumulative[i] /= total;
    free(neighbours);
    return 0;
}

void prior_free(Prior *prior)
{
    free(prior->weight);
    free(prior->cumulative);
    prior->weight = NULL;
    prior->cumulative = NULL;
    prior->count = 0;
}

size_t prior_sample(const Prior *prior, Rng *rng)
{
    double r = rng_double(rng);
    size_t lo = 0;
    size_t hi = prior->count - 1;
    // First index whose cumulative weight exceeds r.
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        if (prior->cumulative[mid] > r)
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

const char *prior_name(PriorKind kind)
{
    switch (kind)
    {
    case PRIOR_UNIFORM:
        return "Any word";
    case PRIOR_EASY:
        return "Easy";
    case PRIOR_NORMAL:
        return "Normal";
    case PRIOR_HARD:
        return "Hard";
    }
    return "";
}

#include "expert.h"

#include <math.h>

#include "core/pattern.h"

double expert_entropy(const WordList *words, const char *guess,
                      const uint32_t *answers, size_t nanswers)
{
    if (nanswers == 0)
        return 0.0;
    uint32_t counts[PATTERN_COUNT] = { 0 };
    Pattern seen[PATTERN_COUNT];
    size_t nseen = 0;
    for (size_t a = 0; a < nanswers; a++)
    {
        Pattern p = pattern_compute(guess, words->items[answers[a]]);
        if (counts[p]++ == 0)
            seen[nseen++] = p;
    }
    // H = log2(N) - sum(c * log2(c)) / N
    double sum = 0.0;
    for (size_t i = 0; i < nseen; i++)
    {
        double c = counts[seen[i]];
        sum += c * log2(c);
    }
    double n = (double)nanswers;
    return log2(n) - sum / n;
}

void expert_scores(const WordList *words, const uint32_t *guesses,
                   size_t nguesses, const uint32_t *answers, size_t nanswers,
                   float *out)
{
    for (size_t i = 0; i < nguesses; i++)
        out[i] = (float)expert_entropy(words, words->items[guesses[i]], answers,
                                       nanswers);
}

void expert_soft_targets(const float *entropy, size_t n, float temperature,
                         float *p)
{
    if (n == 0)
        return;
    float best = entropy[0];
    for (size_t i = 1; i < n; i++)
    {
        if (entropy[i] > best)
            best = entropy[i];
    }
    double total = 0.0;
    for (size_t i = 0; i < n; i++)
    {
        p[i] = expf((entropy[i] - best) / temperature);
        total += p[i];
    }
    for (size_t i = 0; i < n; i++)
        p[i] = (float)(p[i] / total);
}

#include "knowledge.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double weight_of(const Knowledge *k, uint32_t word)
{
    return k->prior != NULL ? k->prior[word] : 1.0;
}

int knowledge_init(Knowledge *k, const WordList *words, const float *prior)
{
    k->words = words;
    k->prior = prior;
    k->cands = malloc(words->count * sizeof(*k->cands));
    k->post = malloc(words->count * sizeof(*k->post));
    if (k->cands == NULL || k->post == NULL)
    {
        knowledge_free(k);
        return -1;
    }
    knowledge_reset(k);
    return 0;
}

void knowledge_reset(Knowledge *k)
{
    k->mass = 0.0;
    for (size_t i = 0; i < k->words->count; i++)
    {
        k->cands[i] = (uint32_t)i;
        k->mass += weight_of(k, (uint32_t)i);
    }
    for (size_t i = 0; i < k->words->count; i++)
        k->post[i] = (float)(weight_of(k, (uint32_t)i) / k->mass);
    k->ncands = k->words->count;
    k->turn = 0;
    memset(k->green, -1, sizeof(k->green));
    memset(k->excluded, 0, sizeof(k->excluded));
    memset(k->min_count, 0, sizeof(k->min_count));
    memset(k->max_count, WORD_LEN, sizeof(k->max_count));
}

void knowledge_free(Knowledge *k)
{
    free(k->cands);
    free(k->post);
    k->cands = NULL;
    k->post = NULL;
    k->ncands = 0;
}

void knowledge_apply(Knowledge *k, const char *guess, Pattern p)
{
    Mark marks[WORD_LEN];
    pattern_to_marks(p, marks);
    unsigned char hits[ALPHABET] = { 0 };
    unsigned char missed[ALPHABET] = { 0 };
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        int letter = guess[i] - 'a';
        if (marks[i] == MARK_CORRECT)
            k->green[i] = (signed char)letter;
        else
            k->excluded[i][letter] = 1;
        if (marks[i] == MARK_ABSENT)
            missed[letter] = 1;
        else
            hits[letter]++;
    }
    for (size_t l = 0; l < ALPHABET; l++)
    {
        if (hits[l] > k->min_count[l])
            k->min_count[l] = hits[l];
        // A grey copy means the secret has exactly as many as were coloured.
        if (missed[l] && hits[l] < k->max_count[l])
            k->max_count[l] = hits[l];
    }

    // The pattern check is the ground truth: it handles repeated letters
    // exactly, which a letter-by-letter filter easily gets wrong.
    size_t kept = 0;
    double mass = 0.0;
    for (size_t i = 0; i < k->ncands; i++)
    {
        uint32_t c = k->cands[i];
        if (pattern_compute(guess, k->words->items[c]) == p)
        {
            k->cands[kept++] = c;
            mass += weight_of(k, c);
        }
        else
            k->post[c] = 0.0f;
    }
    k->ncands = kept;
    k->mass = mass;
    for (size_t i = 0; i < kept; i++)
        k->post[k->cands[i]] = (float)(weight_of(k, k->cands[i]) / mass);

    if (k->turn < MAX_GUESSES)
    {
        long index = wordlist_find(k->words, guess);
        k->guessed[k->turn] = index >= 0 ? (uint32_t)index : UINT32_MAX;
    }
    k->turn++;
}

int knowledge_was_guessed(const Knowledge *k, uint32_t word)
{
    int n = k->turn < MAX_GUESSES ? k->turn : MAX_GUESSES;
    for (int i = 0; i < n; i++)
    {
        if (k->guessed[i] == word)
            return 1;
    }
    return 0;
}

/*
 * Feature layout (FEATURE_DIM = 628), all values in [0, 1]. "Share" means
 * posterior probability mass, i.e. a plain fraction of the candidates when
 * secrets are uniform.
 *   [  0, 156) letter known green at position      (pos * 26 + letter)
 *   [156, 312) letter known not to be at position
 *   [312, 390) letter count is at least 1, 2, 3     (letter * 3 + n - 1)
 *   [390, 416) letter count is known exactly
 *   [416, 572) share of candidates with letter at position
 *   [572, 598) share of candidates containing letter
 *   [598, 624) share of candidates containing letter twice or more
 *   624        log(candidates) / log(dictionary size)
 *   625        turn / MAX_GUESSES
 *   626        log(effective candidates) / log(dictionary size), where
 *              effective = exp(entropy of the posterior)
 *   627        probability of the most likely candidate
 */
#define F_GREEN 0
#define F_EXCLUDED 156
#define F_MIN 312
#define F_EXACT 390
#define F_POS_FREQ 416
#define F_HAS_FREQ 572
#define F_TWICE_FREQ 598
#define F_SIZE 624
#define F_TURN 625
#define F_EFFECTIVE 626
#define F_TOP 627

void knowledge_features(const Knowledge *k, float *x)
{
    memset(x, 0, FEATURE_DIM * sizeof(*x));
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        if (k->green[i] >= 0)
            x[F_GREEN + i * ALPHABET + (size_t)k->green[i]] = 1.0f;
        for (size_t l = 0; l < ALPHABET; l++)
            x[F_EXCLUDED + i * ALPHABET + l] = k->excluded[i][l];
    }
    for (size_t l = 0; l < ALPHABET; l++)
    {
        for (unsigned n = 1; n <= 3; n++)
            x[F_MIN + l * 3 + n - 1] = k->min_count[l] >= n;
        x[F_EXACT + l] = k->min_count[l] == k->max_count[l];
    }

    double entropy = 0.0;
    float top = 0.0f;
    for (size_t c = 0; c < k->ncands; c++)
    {
        const char *w = k->words->items[k->cands[c]];
        float p = k->post[k->cands[c]];
        unsigned char count[ALPHABET] = { 0 };
        for (size_t i = 0; i < WORD_LEN; i++)
        {
            int l = w[i] - 'a';
            x[F_POS_FREQ + i * ALPHABET + (size_t)l] += p;
            if (++count[l] == 1)
                x[F_HAS_FREQ + l] += p;
            else if (count[l] == 2)
                x[F_TWICE_FREQ + l] += p;
        }
        if (p > 0.0f)
            entropy -= p * log(p);
        if (p > top)
            top = p;
    }
    double log_n = log((double)k->words->count);
    if (k->ncands > 1 && k->words->count > 1)
    {
        x[F_SIZE] = (float)(log((double)k->ncands) / log_n);
        x[F_EFFECTIVE] = (float)(entropy / log_n);
    }
    x[F_TURN] = (float)k->turn / MAX_GUESSES;
    x[F_TOP] = top;
}

int word_features(const char *word, uint16_t idx[WORD_FEATURE_MAX])
{
    unsigned char count[ALPHABET] = { 0 };
    int n = 0;
    for (size_t i = 0; i < WORD_LEN; i++)
    {
        int l = word[i] - 'a';
        idx[n++] = (uint16_t)(i * ALPHABET + (size_t)l);
        count[l]++;
    }
    for (int l = 0; l < ALPHABET; l++)
    {
        if (count[l] >= 1)
            idx[n++] = (uint16_t)(156 + l);
        if (count[l] >= 2)
            idx[n++] = (uint16_t)(182 + l);
    }
    return n;
}

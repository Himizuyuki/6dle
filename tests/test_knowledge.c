#include "core/game.h"
#include "core/rng.h"
#include "solver/knowledge.h"
#include "test.h"

#define MAX_GUESSES_FOR_TEST 4

static WordList bank;
static int bank_loaded;

static const WordList *full_bank(void)
{
    if (!bank_loaded)
        bank_loaded = wordlist_load(&bank, SIXDLE_WORDS) == 0;
    return bank_loaded ? &bank : NULL;
}

static int is_candidate(const Knowledge *k, const char *word)
{
    for (size_t i = 0; i < k->ncands; i++)
    {
        if (strcmp(k->words->items[k->cands[i]], word) == 0)
            return 1;
    }
    return 0;
}

static void duplicate_grey_letter_keeps_the_secret(void)
{
    // Regression: the old solver removed every word containing a letter as
    // soon as one copy was grey, which eliminated the secret here.
    const WordList *words = full_bank();
    REQUIRE(words != NULL);
    Knowledge k;
    REQUIRE(knowledge_init(&k, words) == 0);
    knowledge_apply(&k, "teeter", pattern_compute("teeter", "planet"));
    CHECK(is_candidate(&k, "planet"));
    CHECK(!is_candidate(&k, "teeter"));
    CHECK_INT(k.green[4], 'e' - 'a');
    CHECK_INT(k.min_count['e' - 'a'], 1);
    CHECK_INT(k.max_count['e' - 'a'], 1); // grey copies cap the count
    CHECK_INT(k.min_count['t' - 'a'], 1);
    CHECK_INT(k.max_count['t' - 'a'], 1);
    CHECK_INT(k.max_count['r' - 'a'], 0);
    CHECK(k.excluded[0]['t' - 'a']); // yellow: not here
    CHECK(k.excluded[3]['t' - 'a']); // grey copy: not here either
    knowledge_free(&k);
}

static void green_letters_are_kept_in_place(void)
{
    // Regression: the old updateTree compared letters with the colour code
    // ('g' - 'a') instead of the guessed letter.
    const WordList *words = full_bank();
    REQUIRE(words != NULL);
    Knowledge k;
    REQUIRE(knowledge_init(&k, words) == 0);
    knowledge_apply(&k, "guzzle", pattern_compute("guzzle", "puzzle"));
    REQUIRE(k.ncands > 0);
    for (size_t i = 0; i < k.ncands; i++)
    {
        const char *w = words->items[k.cands[i]];
        CHECK(strncmp(w + 1, "uzzle", 5) == 0 && w[0] != 'g');
    }
    CHECK(is_candidate(&k, "puzzle"));
    knowledge_free(&k);
}

static void filtering_is_exact_on_random_games(void)
{
    // Property: after any sequence of guesses, the candidates are exactly
    // the words that would have produced the same feedback.
    const WordList *words = full_bank();
    REQUIRE(words != NULL);
    Knowledge k;
    REQUIRE(knowledge_init(&k, words) == 0);
    Rng rng;
    rng_seed(&rng, 99);
    int lost_secret = 0;
    int wrong_count = 0;
    const char *guesses[MAX_GUESSES_FOR_TEST];
    Pattern patterns[MAX_GUESSES_FOR_TEST];
    for (int game = 0; game < 40; game++)
    {
        const char *secret = words->items[rng_below(&rng, words->count)];
        knowledge_reset(&k);
        for (int t = 0; t < MAX_GUESSES_FOR_TEST; t++)
        {
            guesses[t] = words->items[rng_below(&rng, words->count)];
            patterns[t] = pattern_compute(guesses[t], secret);
            knowledge_apply(&k, guesses[t], patterns[t]);
            lost_secret += !is_candidate(&k, secret);

            size_t expected = 0;
            for (size_t w = 0; w < words->count; w++)
            {
                int ok = 1;
                for (int j = 0; j <= t && ok; j++)
                    ok = pattern_compute(guesses[j], words->items[w])
                        == patterns[j];
                expected += (size_t)ok;
            }
            wrong_count += expected != k.ncands;
        }
        CHECK_INT(k.turn, MAX_GUESSES_FOR_TEST);
    }
    CHECK_INT(lost_secret, 0);
    CHECK_INT(wrong_count, 0);
    knowledge_free(&k);
}

static void features_are_normalised(void)
{
    const WordList *words = full_bank();
    REQUIRE(words != NULL);
    Knowledge k;
    REQUIRE(knowledge_init(&k, words) == 0);
    float x[FEATURE_DIM];
    knowledge_features(&k, x);
    // Fresh state: nothing known, all words possible.
    int ok = 1;
    for (int i = 0; i < 416; i++)
        ok &= x[i] == 0.0f;
    CHECK(ok);
    CHECK_NEAR(x[624], 1.0, 1e-6); // log(n) / log(n)
    CHECK_NEAR(x[625], 0.0, 1e-6);

    knowledge_apply(&k, "tories", pattern_compute("tories", "planet"));
    knowledge_features(&k, x);
    ok = 1;
    for (int i = 0; i < FEATURE_DIM; i++)
        ok &= x[i] >= 0.0f && x[i] <= 1.0f;
    CHECK(ok);
    CHECK(x[(size_t)4 * 26 + ('e' - 'a')] == 1.0f); // E green at 4
    CHECK(x[156 + ('t' - 'a')] == 1.0f); // T not at 0
    CHECK(x[312 + ('t' - 'a') * 3] == 1.0f); // at least one T
    CHECK(x[390 + ('o' - 'a')] == 1.0f); // O count known (zero)
    // Every candidate has E at position 4 and contains a T.
    CHECK_NEAR(x[416 + 4 * 26 + ('e' - 'a')], 1.0, 1e-6);
    CHECK_NEAR(x[572 + ('t' - 'a')], 1.0, 1e-6);
    CHECK_NEAR(x[572 + ('o' - 'a')], 0.0, 1e-6);
    CHECK_NEAR(x[625], 1.0 / MAX_GUESSES, 1e-6);
    knowledge_free(&k);
}

static void word_feature_indices(void)
{
    uint16_t idx[WORD_FEATURE_MAX];
    // 6 positions + 5 distinct letters + 1 doubled letter (z).
    CHECK_INT(word_features("puzzle", idx), 12);
    CHECK_INT(idx[0], 0 * 26 + ('p' - 'a'));
    CHECK_INT(idx[5], 5 * 26 + ('e' - 'a'));
    int has_double_z = 0;
    for (int i = 0; i < 12; i++)
    {
        CHECK(idx[i] < WORD_FEATURE_DIM);
        has_double_z |= idx[i] == 182 + ('z' - 'a');
    }
    CHECK(has_double_z);
    CHECK_INT(word_features("assess", idx), 6 + 3 + 1); // a, s, e; s doubled
}

static void release_bank(void)
{
    if (bank_loaded)
        wordlist_free(&bank);
    bank_loaded = 0;
}

void knowledge_tests(void)
{
    RUN(duplicate_grey_letter_keeps_the_secret);
    RUN(green_letters_are_kept_in_place);
    RUN(filtering_is_exact_on_random_games);
    RUN(features_are_normalised);
    RUN(word_feature_indices);
    release_bank();
}

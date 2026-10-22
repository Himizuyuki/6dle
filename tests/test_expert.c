#include <math.h>

#include "solver/expert.h"
#include "test.h"

static void entropy_extremes(void)
{
    // Guessing "aaaaaa" against words that all lack an 'a' gives one
    // pattern: no information. Against words with an 'a' in a different
    // position each, every pattern differs: log2(n) bits.
    const char *words[] = { "aaaaaa", "abbbbb", "babbbb", "bbabbb", "bbbabb",
                            "bbbbab", "bbbbba", "cccccc", "dddddd", "eeeeee" };
    WordList list;
    REQUIRE(wordlist_from_array(&list, words, 10) == 0);
    long guess = wordlist_find(&list, "aaaaaa");
    REQUIRE(guess >= 0);

    uint32_t none[3];
    none[0] = (uint32_t)wordlist_find(&list, "cccccc");
    none[1] = (uint32_t)wordlist_find(&list, "dddddd");
    none[2] = (uint32_t)wordlist_find(&list, "eeeeee");
    CHECK_NEAR(expert_entropy(&list, "aaaaaa", none, 3), 0.0, 1e-9);

    uint32_t spread[6];
    const char *one_a[] = { "abbbbb", "babbbb", "bbabbb",
                            "bbbabb", "bbbbab", "bbbbba" };
    for (int i = 0; i < 6; i++)
        spread[i] = (uint32_t)wordlist_find(&list, one_a[i]);
    CHECK_NEAR(expert_entropy(&list, "aaaaaa", spread, 6), log2(6.0), 1e-9);
    CHECK_NEAR(expert_entropy(&list, "aaaaaa", spread, 0), 0.0, 1e-9);

    float scores[1];
    uint32_t g = (uint32_t)guess;
    expert_scores(&list, &g, 1, spread, 6, scores);
    CHECK_NEAR(scores[0], log2(6.0), 1e-5);
    wordlist_free(&list);
}

static void entropy_two_buckets(void)
{
    // 4 answers split 3/1: H = 2 - 3/4 * log2(3).
    const char *words[] = { "aaaaaa", "bbbbbb", "cccccc", "dddddd", "abbbbb" };
    WordList list;
    REQUIRE(wordlist_from_array(&list, words, 5) == 0);
    uint32_t answers[4];
    const char *ans[] = { "bbbbbb", "cccccc", "dddddd", "abbbbb" };
    for (int i = 0; i < 4; i++)
        answers[i] = (uint32_t)wordlist_find(&list, ans[i]);
    CHECK_NEAR(expert_entropy(&list, "aaaaaa", answers, 4),
               2.0 - 0.75 * log2(3.0), 1e-9);
    wordlist_free(&list);
}

static void soft_targets_form_a_distribution(void)
{
    float h[] = { 3.0f, 3.5f, 1.0f, 3.4f };
    float p[4];
    expert_soft_targets(h, 4, 0.1f, p);
    double total = 0.0;
    for (int i = 0; i < 4; i++)
    {
        CHECK(p[i] >= 0.0f);
        total += p[i];
    }
    CHECK_NEAR(total, 1.0, 1e-5);
    CHECK(p[1] > p[3] && p[3] > p[0] && p[0] > p[2]);
    // 0.1 bits apart at temperature 0.1: ratio e.
    CHECK_NEAR(p[1] / p[3], exp(1.0), 1e-3);

    // Higher temperature: flatter distribution.
    float flat[4];
    expert_soft_targets(h, 4, 10.0f, flat);
    CHECK(flat[1] < p[1]);
}

void expert_tests(void)
{
    RUN(entropy_extremes);
    RUN(entropy_two_buckets);
    RUN(soft_targets_form_a_distribution);
}

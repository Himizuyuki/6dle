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

    GuessStats st;
    expert_stats(&list, list.items[guess], spread, NULL, 6, &st);
    CHECK_NEAR(st.entropy, log2(6.0), 1e-9);
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

static void costs_count_the_guesses(void)
{
    // "aaaaaa" tells the six one-A words apart: whichever it is, it is
    // known after this guess and found with the next one.
    const char *words[] = { "aaaaaa", "abbbbb", "babbbb", "bbabbb",
                            "bbbabb", "bbbbab", "bbbbba" };
    WordList list;
    REQUIRE(wordlist_from_array(&list, words, 7) == 0);
    uint32_t others[6];
    for (int i = 0; i < 6; i++)
        others[i] = (uint32_t)wordlist_find(&list, words[i + 1]);
    GuessStats st;
    expert_stats(&list, "aaaaaa", others, NULL, 6, &st);
    CHECK_NEAR(st.cost, 2.0, 1e-9);
    CHECK_NEAR(st.win, 0.0, 1e-9);
    CHECK_NEAR(st.worst_left, 1.0, 1e-9);
    CHECK_NEAR(st.expected_left, 1.0, 1e-9);

    // If it can also be the secret, it wins 1 time in 7.
    uint32_t all[7];
    for (int i = 0; i < 7; i++)
        all[i] = (uint32_t)i;
    expert_stats(&list, "aaaaaa", all, NULL, 7, &st);
    CHECK_NEAR(st.win, 1.0 / 7.0, 1e-9);
    CHECK_NEAR(st.cost, 1.0 + 6.0 / 7.0, 1e-9);
    float batch[1];
    uint32_t g = (uint32_t)wordlist_find(&list, "aaaaaa");
    expert_costs(&list, &g, 1, all, NULL, 7, batch);
    CHECK_NEAR(batch[0], st.cost, 1e-6);
    wordlist_free(&list);
}

static void weighted_answers(void)
{
    // Grey group {bbbbbb, cccccc, dddddd}, green group {abbbbb}; the latter
    // is 100 times more likely than each of the others.
    const char *words[] = { "aaaaaa", "bbbbbb", "cccccc", "dddddd", "abbbbb" };
    WordList list;
    REQUIRE(wordlist_from_array(&list, words, 5) == 0);
    const char *ans[] = { "bbbbbb", "cccccc", "dddddd", "abbbbb" };
    uint32_t answers[4];
    for (int i = 0; i < 4; i++)
        answers[i] = (uint32_t)wordlist_find(&list, ans[i]);
    float weights[] = { 1.0f, 1.0f, 1.0f, 100.0f };
    GuessStats st;
    expert_stats(&list, "aaaaaa", answers, weights, 4, &st);
    double grey = 3.0 / 103.0;
    CHECK_NEAR(st.entropy, -(grey * log2(grey) + (1 - grey) * log2(1 - grey)),
               1e-9);
    CHECK_NEAR(st.cost, 1.0 + grey * expert_remaining(3.0) + (1 - grey), 1e-9);
    // Scaling the weights changes nothing.
    float scaled[] = { 0.01f, 0.01f, 0.01f, 1.0f };
    GuessStats st2;
    expert_stats(&list, "aaaaaa", answers, scaled, 4, &st2);
    CHECK_NEAR(st2.cost, st.cost, 1e-6);
    wordlist_free(&list);
}

static void remaining_guesses_grow_with_the_words_left(void)
{
    CHECK_NEAR(expert_remaining(1.0), 1.0, 1e-12);
    CHECK_NEAR(expert_remaining(2.0), 1.5, 1e-12);
    int increasing = 1;
    for (double n = 1.0; n < 100000.0; n *= 1.3)
        increasing &= expert_remaining(n * 1.3) > expert_remaining(n);
    CHECK(increasing);
    CHECK(expert_remaining(22000.0) > 3.5 && expert_remaining(22000.0) < 4.0);
}

static void evaluate_considers_probes(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, FIXTURE("small.txt")) == 0);
    Knowledge k;
    REQUIRE(knowledge_init(&k, &list, NULL) == 0);
    // ..llet: mallet, pallet, sallet, wallet (+ gullet/mullet are out).
    knowledge_apply(&k, "bullet", pattern_compute("bullet", "mallet"));
    REQUIRE(k.ncands > 2);
    ExpertOptions opt = expert_default_options();
    uint32_t pool[1200];
    float cost[1200];
    size_t n = expert_evaluate(&k, &opt, pool, cost);
    int probes = 0;
    int all_cands = 1;
    long bullet = wordlist_find(&list, "bullet");
    for (size_t i = 0; i < n; i++)
    {
        CHECK(pool[i] != (uint32_t)bullet); // never repeat a guess
        CHECK(cost[i] >= 1.0f && cost[i] < 3.0f);
        probes += k.post[pool[i]] == 0.0f;
    }
    CHECK(probes > 0);
    CHECK_INT(n, list.count - 1); // every other word fits in the shortlist

    opt.allow_probes = 0;
    n = expert_evaluate(&k, &opt, pool, cost);
    CHECK_INT(n, k.ncands);
    for (size_t i = 0; i < n; i++)
        all_cands &= k.post[pool[i]] > 0.0f;
    CHECK(all_cands);
    knowledge_free(&k);
    wordlist_free(&list);
}

void expert_tests(void)
{
    RUN(entropy_extremes);
    RUN(entropy_two_buckets);
    RUN(soft_targets_form_a_distribution);
    RUN(costs_count_the_guesses);
    RUN(weighted_answers);
    RUN(remaining_guesses_grow_with_the_words_left);
    RUN(evaluate_considers_probes);
}

#include <math.h>
#include <stdlib.h>

#include "core/game.h"
#include "core/prior.h"
#include "test.h"

static void uniform_prior(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, FIXTURE("small.txt")) == 0);
    Prior prior;
    REQUIRE(prior_init(&prior, &list, PRIOR_UNIFORM) == 0);
    double total = 0.0;
    for (size_t i = 0; i < list.count; i++)
    {
        CHECK_NEAR(prior.weight[i], 1.0 / (double)list.count, 1e-7);
        total += prior.weight[i];
    }
    CHECK_NEAR(total, 1.0, 1e-5);
    CHECK_NEAR(prior.cumulative[list.count - 1], 1.0, 1e-9);
    prior_free(&prior);
    // The common-words prior needs frequencies.
    CHECK(prior_init(&prior, &list, PRIOR_NORMAL) == -1);
    wordlist_free(&list);
}

static void common_words_weigh_more(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, SIXDLE_WORDS) == 0);
    REQUIRE(wordlist_load_frequencies(&list, SIXDLE_FREQ) == 0);
    Prior prior;
    REQUIRE(prior_init(&prior, &list, PRIOR_NORMAL) == 0);
    double total = 0.0;
    int positive = 1;
    for (size_t i = 0; i < list.count; i++)
    {
        total += prior.weight[i];
        positive &= prior.weight[i] > 0.0f;
    }
    CHECK_NEAR(total, 1.0, 1e-4);
    CHECK(positive); // every word can still be the secret
    float people = prior.weight[wordlist_find(&list, "people")];
    float planet = prior.weight[wordlist_find(&list, "planet")];
    float aaliis = prior.weight[wordlist_find(&list, "aaliis")];
    CHECK(planet > 100 * aaliis);
    CHECK_NEAR(people, planet, people * 0.01); // both clearly common
    // Words the corpus never saw share about 0.5% of the probability.
    double unseen = 0.0;
    for (size_t i = 0; i < list.count; i++)
    {
        if (list.zipf[i] == 0.0f)
            unseen += prior.weight[i];
    }
    CHECK(unseen > 0.002 && unseen < 0.01);
    prior_free(&prior);
    wordlist_free(&list);
}

static void sampling_follows_the_weights(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, FIXTURE("small.txt")) == 0);
    REQUIRE(wordlist_load_frequencies(&list, FIXTURE("small-freq.txt")) == 0);
    Prior prior;
    REQUIRE(prior_init(&prior, &list, PRIOR_NORMAL) == 0);
    Rng rng;
    rng_seed(&rng, 42);
    int *seen = calloc(list.count, sizeof(int));
    const int draws = 200000;
    for (int i = 0; i < draws; i++)
    {
        Game game;
        game_start_random(&game, &list, &prior, &rng);
        seen[wordlist_find(&list, game.secret)]++;
    }
    int bad = 0;
    for (size_t i = 0; i < list.count; i++)
    {
        double expected = prior.weight[i] * draws;
        // Within 5 standard deviations of the binomial count.
        if (fabs(seen[i] - expected) > 5.0 * sqrt(expected) + 1.0)
            bad++;
    }
    CHECK_INT(bad, 0);
    CHECK(seen[wordlist_find(&list, "bottle")]
          > 50 * seen[wordlist_find(&list, "cocoon")]);
    free(seen);
    prior_free(&prior);
    wordlist_free(&list);
}

static void difficulty_levels(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, SIXDLE_WORDS) == 0);
    REQUIRE(wordlist_load_frequencies(&list, SIXDLE_FREQ) == 0);
    unsigned *nb = malloc(list.count * sizeof(unsigned));
    REQUIRE(prior_neighbours(&list, nb) == 0);
    // BATTER: BETTER, BITTER, BUTTER, MATTER, LATTER, BATHER, BANTER...
    CHECK(nb[wordlist_find(&list, "batter")] >= 10);
    CHECK_INT(nb[wordlist_find(&list, "people")], 0);

    Prior easy;
    Prior hard;
    REQUIRE(prior_init(&easy, &list, PRIOR_EASY) == 0);
    REQUIRE(prior_init(&hard, &list, PRIOR_HARD) == 0);
    size_t in_easy = 0;
    size_t in_hard = 0;
    int easy_ok = 1;
    int hard_ok = 1;
    float top_easy = easy.weight[wordlist_find(&list, "people")];
    float top_hard = hard.weight[wordlist_find(&list, "batter")];
    for (size_t i = 0; i < list.count; i++)
    {
        if (easy.weight[i] > top_easy * 0.5f)
        {
            in_easy++;
            easy_ok &= list.zipf[i] >= PRIOR_EASY_ZIPF
                && nb[i] <= PRIOR_EASY_NEIGHBOURS;
        }
        if (hard.weight[i] > top_hard * 0.5f)
        {
            in_hard++;
            hard_ok &= list.zipf[i] < PRIOR_HARD_ZIPF_HIGH
                || nb[i] >= PRIOR_TRAP_NEIGHBOURS;
        }
    }
    CHECK(easy_ok);
    CHECK(hard_ok);
    CHECK(in_easy > 1000 && in_easy < 2000);
    CHECK(in_hard > 3000 && in_hard < 6000);
    // Outside a level, words keep a tiny but positive probability.
    float aaliis = easy.weight[wordlist_find(&list, "aaliis")];
    CHECK(aaliis > 0.0f && aaliis < top_easy * 1e-5f);
    CHECK(hard.weight[wordlist_find(&list, "people")] < top_hard * 1e-5f);
    CHECK_STR(prior_name(PRIOR_HARD), "Hard");
    prior_free(&easy);
    prior_free(&hard);
    free(nb);
    wordlist_free(&list);
}

static void never_secret_words_are_never_drawn(void)
{
    WordList list;
    REQUIRE(wordlist_load(&list, SIXDLE_WORDS) == 0);
    REQUIRE(wordlist_load_frequencies(&list, SIXDLE_FREQ) == 0);
    REQUIRE(wordlist_load_never_secret(&list, SIXDLE_NEVER_SECRET) == 0);
    long slur = wordlist_find(&list, "faggot");
    long fine = wordlist_find(&list, "scrape"); // contains "rape": allowed
    REQUIRE(slur >= 0 && fine >= 0);
    CHECK(!word_can_be_secret(&list, (size_t)slur));
    CHECK(word_can_be_secret(&list, (size_t)fine));
    size_t blocked = 0;
    for (size_t i = 0; i < list.count; i++)
        blocked += !word_can_be_secret(&list, i);
    CHECK(blocked >= 30 && blocked < 100);

    for (int kind = 0; kind < PRIOR_KINDS; kind++)
    {
        Prior prior;
        REQUIRE(prior_init(&prior, &list, (PriorKind)kind) == 0);
        int zero = 1;
        for (size_t i = 0; i < list.count; i++)
        {
            if (!word_can_be_secret(&list, i))
                zero &= prior.weight[i] == 0.0f;
        }
        CHECK(zero);
        // Draw a lot of secrets: none may be blocked.
        Rng rng;
        rng_seed(&rng, 9);
        int drawn_blocked = 0;
        for (int g = 0; g < 50000; g++)
        {
            Game game;
            game_start_random(&game, &list,
                              kind == PRIOR_UNIFORM && g % 2 ? NULL : &prior,
                              &rng);
            drawn_blocked += !word_can_be_secret(
                &list, (size_t)wordlist_find(&list, game.secret));
        }
        CHECK_INT(drawn_blocked, 0);
        prior_free(&prior);
    }
    // Still a perfectly valid guess.
    Game game;
    game_start(&game, &list, "planet");
    CHECK_INT(game_guess(&game, "faggot"), GUESS_OK);
    wordlist_free(&list);
}

void prior_tests(void)
{
    RUN(uniform_prior);
    RUN(common_words_weigh_more);
    RUN(sampling_follows_the_weights);
    RUN(difficulty_levels);
    RUN(never_secret_words_are_never_drawn);
}

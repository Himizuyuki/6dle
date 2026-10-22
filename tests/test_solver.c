#include <math.h>
#include <stdlib.h>

#include "core/game.h"
#include "solver/solver.h"
#include "test.h"

static int small_network(Mlp *mlp, int hidden, uint64_t seed)
{
    int sizes[] = { FEATURE_DIM, hidden, WORD_FEATURE_DIM };
    Rng rng;
    rng_seed(&rng, seed);
    return mlp_init(mlp, sizes, 2, &rng);
}

static int in_pool(const Solver *s, uint32_t word)
{
    for (size_t i = 0; i < s->npool; i++)
    {
        if (s->pool[i] == word)
            return 1;
    }
    return 0;
}

static void rejects_mismatched_model(void)
{
    int sizes[] = { 10, 4, WORD_FEATURE_DIM };
    Mlp mlp;
    Rng rng;
    rng_seed(&rng, 1);
    REQUIRE(mlp_init(&mlp, sizes, 2, &rng) == 0);
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Solver s;
    CHECK(solver_init(&s, &words, &mlp, NULL) == -1);
    wordlist_free(&words);
    mlp_free(&mlp);
}

static void probabilities_and_ranking(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 16, 2) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);

    Suggestion top[5];
    CHECK_INT(solver_top(&s, top, 5), 5);
    double total = 0.0;
    float max = 0.0f;
    for (size_t i = 0; i < s.npool; i++)
    {
        total += s.probs[i];
        max = s.probs[i] > max ? s.probs[i] : max;
    }
    CHECK_NEAR(total, 1.0, 1e-4);
    for (int i = 1; i < 5; i++)
        CHECK(top[i - 1].prob >= top[i].prob);
    CHECK_NEAR(top[0].prob, max, 1e-7);
    CHECK_INT(solver_best(&s), top[0].word);
    CHECK_INT(solver_top(&s, top, 0), 0);
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

static void probe_and_hard_pools(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 16, 3) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);
    uint32_t bullet = (uint32_t)wordlist_find(&words, "bullet");
    uint32_t cocoon = (uint32_t)wordlist_find(&words, "cocoon");
    solver_apply(&s, "bullet", pattern_compute("bullet", "mallet"));
    REQUIRE(solver_candidates(&s) == 4); // mallet pallet sallet wallet

    // Probes: every word except the one already played.
    solver_evaluate(&s);
    CHECK_INT(s.npool, words.count - 1);
    CHECK(!in_pool(&s, bullet));
    CHECK(in_pool(&s, cocoon));
    // Hard mode: the candidates only (the scores are reused).
    solver_set_probes(&s, 0);
    CHECK(s.evaluated);
    CHECK_INT(s.npool, 4);
    for (size_t i = 0; i < s.npool; i++)
        CHECK(solver_is_candidate(&s, s.pool[i]));
    double total = 0.0;
    for (size_t i = 0; i < s.npool; i++)
        total += s.probs[i];
    CHECK_NEAR(total, 1.0, 1e-5);
    // Back to probes: the wider pool needs a new evaluation.
    solver_set_probes(&s, 1);
    CHECK(!s.evaluated);

    // Two words left: a probe can't help, candidates only.
    solver_apply(&s, "wallet", pattern_compute("wallet", "mallet"));
    solver_apply(&s, "sallet", pattern_compute("sallet", "mallet"));
    REQUIRE(solver_candidates(&s) <= 2);
    solver_evaluate(&s);
    CHECK_INT(s.npool, solver_candidates(&s));
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

static void answers_follow_the_prior(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    REQUIRE(wordlist_load_frequencies(&words, FIXTURE("small-freq.txt")) == 0);
    Prior prior;
    REQUIRE(prior_init(&prior, &words, PRIOR_NORMAL) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 8, 4) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);

    // Uniform: the four ..llet words are equally likely.
    solver_apply(&s, "bullet", pattern_compute("bullet", "mallet"));
    Suggestion top[5];
    CHECK_INT(solver_top_answers(&s, top, 5), 4);
    for (int i = 0; i < 4; i++)
        CHECK_NEAR(top[i].prob, 0.25, 1e-6);

    // Common words: "wallet" (Zipf 3.9) beats the three rare ones.
    solver_set_prior(&s, prior.weight);
    CHECK_INT(solver_candidates(&s), words.count);
    solver_apply(&s, "bullet", pattern_compute("bullet", "mallet"));
    CHECK_INT(solver_top_answers(&s, top, 5), 4);
    CHECK_STR(words.items[top[0].word], "wallet");
    CHECK(top[0].prob > 0.9f);
    double total = 0.0;
    for (int i = 0; i < 4; i++)
        total += top[i].prob;
    CHECK_NEAR(total, 1.0, 1e-5);
    solver_free(&s);
    mlp_free(&mlp);
    prior_free(&prior);
    wordlist_free(&words);
}

static void plays_legal_games_to_the_end(void)
{
    // Even an untrained network must make legal, never repeated guesses
    // and keep the secret possible, in both modes.
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 8, 3) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);
    int illegal = 0;
    for (int probes = 0; probes <= 1; probes++)
    {
        solver_set_probes(&s, probes);
        for (size_t secret = 0; secret < words.count; secret++)
        {
            Game game;
            game_start(&game, &words, words.items[secret]);
            solver_reset(&s);
            while (game.status == GAME_PLAYING)
            {
                uint32_t w = solver_best(&s);
                REQUIRE(w != SOLVER_NO_WORD);
                illegal += !probes && !solver_is_candidate(&s, w);
                const char *guess = words.items[w];
                REQUIRE(game_guess(&game, guess) == GUESS_OK);
                solver_apply(&s, guess, game.patterns[game.nguesses - 1]);
                CHECK(solver_is_candidate(&s, (uint32_t)secret));
            }
        }
    }
    CHECK_INT(illegal, 0);
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

static void no_word_fits_a_secret_outside_the_list(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 8, 4) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);
    // Only "cocoon" avoids every letter of "bullet"; then "cocoon" itself
    // comes back all grey, so nothing is left.
    solver_apply(&s, "bullet", pattern_compute("bullet", "zzzzzz"));
    CHECK_INT(solver_candidates(&s), 1);
    solver_apply(&s, "cocoon", pattern_compute("cocoon", "zzzzzz"));
    CHECK_INT(solver_candidates(&s), 0);
    CHECK_INT(solver_best(&s), SOLVER_NO_WORD);
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

static float loss_at(Solver *s, const float *target)
{
    solver_evaluate(s);
    float loss = 0.0f;
    for (size_t i = 0; i < s->npool; i++)
    {
        if (target[i] > 0.0f)
            loss -= target[i] * logf(s->probs[i]);
    }
    return loss;
}

static void backward_matches_numerical_gradient(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    REQUIRE(wordlist_load_frequencies(&words, FIXTURE("small-freq.txt")) == 0);
    Prior prior;
    REQUIRE(prior_init(&prior, &words, PRIOR_NORMAL) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 6, 5) == 0);
    Solver s;
    // A non-uniform prior exercises the posterior input of the scores.
    REQUIRE(solver_init(&s, &words, &mlp, prior.weight) == 0);
    solver_apply(&s, "bullet", pattern_compute("bullet", "mallet"));
    solver_evaluate(&s);
    REQUIRE(s.npool > 10); // probes: candidates and others mixed

    float *target = calloc(s.npool, sizeof(float));
    target[0] = 0.5f;
    target[3] = 0.3f;
    target[s.npool - 1] = 0.2f;
    float *grad = calloc(mlp.nparams, sizeof(float));
    float loss = solver_loss(&s, target);
    solver_backprop(&s, grad);
    CHECK_NEAR(loss, loss_at(&s, target), 1e-5);

    // Check the gradient on a spread of parameters, including the output
    // layer which carries the word scoring.
    int bad = 0;
    int checked = 0;
    const float h = 1e-2f;
    for (size_t i = 0; i < mlp.nparams; i += 31)
    {
        float saved = mlp.params[i];
        mlp.params[i] = saved + h;
        double up = loss_at(&s, target);
        mlp.params[i] = saved - h;
        double down = loss_at(&s, target);
        mlp.params[i] = saved;
        double numeric = (up - down) / (2.0 * h);
        checked++;
        if (fabs(numeric - grad[i]) > 2e-3 + 2e-2 * fabs(numeric))
            bad++;
    }
    CHECK(checked > 100);
    // A handful may straddle a ReLU kink; real bugs break most of them.
    CHECK(bad <= checked / 50);
    free(grad);
    free(target);
    solver_free(&s);
    mlp_free(&mlp);
    prior_free(&prior);
    wordlist_free(&words);
}

static void training_step_lowers_the_loss(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 32, 6) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);
    Adam adam;
    REQUIRE(adam_init(&adam, mlp.nparams, 1e-3f) == 0);
    solver_evaluate(&s);
    float *target = calloc(s.npool, sizeof(float));
    float *grad = calloc(mlp.nparams, sizeof(float));
    target[3] = 1.0f; // pretend the expert likes word 3

    float before = loss_at(&s, target);
    for (int step = 0; step < 50; step++)
    {
        memset(grad, 0, mlp.nparams * sizeof(float));
        solver_evaluate(&s);
        solver_loss(&s, target);
        solver_backprop(&s, grad);
        adam_step(&adam, mlp.params, grad, 1.0f);
    }
    float after = loss_at(&s, target);
    CHECK(after < before * 0.5f);
    CHECK_INT(solver_best(&s), s.pool[3]);

    free(grad);
    free(target);
    adam_free(&adam);
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

// Average guesses and number of wins of the shipped model over `games`
// random secrets.
static int play_games(Solver *s, const WordList *words, const Prior *prior,
                      int games, double *average)
{
    Rng rng;
    rng_seed(&rng, 2024);
    int won = 0;
    int total = 0;
    for (int g = 0; g < games; g++)
    {
        Game game;
        game_start_random(&game, words, prior, &rng);
        solver_reset(s);
        while (game.status == GAME_PLAYING)
        {
            const char *guess = words->items[solver_best(s)];
            if (game_guess(&game, guess) != GUESS_OK)
                return -1;
            solver_apply(s, guess, game.patterns[game.nguesses - 1]);
        }
        if (game.status == GAME_WON)
        {
            won++;
            total += game.nguesses;
        }
    }
    *average = (double)total / (won ? won : 1);
    return won;
}

static void shipped_model_solves_the_game(void)
{
    // End-to-end check of models/solver.nn on the real word list, in the
    // four combinations of secret prior and probing.
    Mlp mlp;
    if (mlp_load(&mlp, SIXDLE_MODEL) != 0)
    {
        printf("    (skipped: no model at %s, run `make train`)\n",
               SIXDLE_MODEL);
        return;
    }
    WordList words;
    REQUIRE(wordlist_load(&words, SIXDLE_WORDS) == 0);
    REQUIRE(wordlist_load_frequencies(&words, SIXDLE_FREQ) == 0);
    Prior priors[2];
    REQUIRE(prior_init(&priors[0], &words, PRIOR_UNIFORM) == 0);
    REQUIRE(prior_init(&priors[1], &words, PRIOR_NORMAL) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp, NULL) == 0);

    const int games = 200;
    for (int kind = 0; kind < 2; kind++)
    {
        solver_set_prior(&s, kind == 0 ? NULL : priors[1].weight);
        for (int probes = 1; probes >= 0; probes--)
        {
            solver_set_probes(&s, probes);
            double average = 0.0;
            int won = play_games(&s, &words, &priors[kind], games, &average);
            printf("    %s, %s: won %d/%d, %.2f guesses on average\n",
                   prior_name(priors[kind].kind), probes ? "probes" : "hard",
                   won, games, average);
            // Training measures 99-100% with probes, ~98% in hard mode.
            CHECK(won >= games * (probes ? 98 : 95) / 100);
            CHECK(average < (probes ? 3.9 : 4.1));
        }
    }

    solver_free(&s);
    prior_free(&priors[0]);
    prior_free(&priors[1]);
    wordlist_free(&words);
    mlp_free(&mlp);
}

void solver_tests(void)
{
    RUN(rejects_mismatched_model);
    RUN(probabilities_and_ranking);
    RUN(probe_and_hard_pools);
    RUN(answers_follow_the_prior);
    RUN(plays_legal_games_to_the_end);
    RUN(no_word_fits_a_secret_outside_the_list);
    RUN(backward_matches_numerical_gradient);
    RUN(training_step_lowers_the_loss);
    RUN(shipped_model_solves_the_game);
}

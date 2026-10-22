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

static int is_candidate(const Solver *s, uint32_t word)
{
    for (size_t i = 0; i < s->k.ncands; i++)
    {
        if (s->k.cands[i] == word)
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
    CHECK(solver_init(&s, &words, &mlp) == -1);
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
    REQUIRE(solver_init(&s, &words, &mlp) == 0);

    Suggestion top[5];
    CHECK_INT(solver_top(&s, top, 5), 5);
    double total = 0.0;
    for (size_t i = 0; i < s.k.ncands; i++)
        total += s.probs[i];
    CHECK_NEAR(total, 1.0, 1e-4);
    for (int i = 1; i < 5; i++)
        CHECK(top[i - 1].prob >= top[i].prob);
    // The best one really is the maximum.
    float max = 0.0f;
    for (size_t i = 0; i < s.k.ncands; i++)
        max = s.probs[i] > max ? s.probs[i] : max;
    CHECK_NEAR(top[0].prob, max, 1e-7);
    CHECK_INT(solver_best(&s), top[0].word);
    CHECK_INT(solver_top(&s, top, 0), 0);

    // After feedback, only consistent words can be suggested.
    solver_apply(&s, "bullet", pattern_compute("bullet", "mallet"));
    size_t n = solver_top(&s, top, 5);
    CHECK_INT(n, solver_candidates(&s) < 5 ? solver_candidates(&s) : 5);
    for (size_t i = 0; i < n; i++)
        CHECK(pattern_compute("bullet", words.items[top[i].word])
              == pattern_compute("bullet", "mallet"));

    solver_reset(&s);
    CHECK_INT(solver_candidates(&s), words.count);
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

static void plays_legal_games_to_the_end(void)
{
    // Even an untrained network must only make legal, never repeated,
    // guesses and always finish: every guess is a remaining candidate.
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 8, 3) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp) == 0);
    int illegal = 0;
    for (size_t secret = 0; secret < words.count; secret++)
    {
        Game game;
        game_start(&game, &words, words.items[secret]);
        solver_reset(&s);
        while (game.status == GAME_PLAYING)
        {
            uint32_t w = solver_best(&s);
            REQUIRE(w != SOLVER_NO_WORD);
            illegal += !is_candidate(&s, w);
            const char *guess = words.items[w];
            REQUIRE(game_guess(&game, guess) == GUESS_OK);
            solver_apply(&s, guess, game.patterns[game.nguesses - 1]);
            CHECK(is_candidate(&s, (uint32_t)secret));
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
    REQUIRE(solver_init(&s, &words, &mlp) == 0);
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
    for (size_t c = 0; c < s->k.ncands; c++)
    {
        if (target[c] > 0.0f)
            loss -= target[c] * logf(s->probs[c]);
    }
    return loss;
}

static void backward_matches_numerical_gradient(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 6, 5) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp) == 0);
    // Leaves mallet, pallet, sallet and wallet.
    solver_apply(&s, "bullet", pattern_compute("bullet", "mallet"));
    REQUIRE(s.k.ncands >= 3);

    float *target = calloc(s.k.ncands, sizeof(float));
    target[0] = 0.7f;
    target[2] = 0.3f;
    float *grad = calloc(mlp.nparams, sizeof(float));
    solver_evaluate(&s);
    float loss = solver_backward(&s, target, grad);
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
    wordlist_free(&words);
}

static void training_step_lowers_the_loss(void)
{
    WordList words;
    REQUIRE(wordlist_load(&words, FIXTURE("small.txt")) == 0);
    Mlp mlp;
    REQUIRE(small_network(&mlp, 32, 6) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp) == 0);
    Adam adam;
    REQUIRE(adam_init(&adam, mlp.nparams, 1e-3f) == 0);
    float *target = calloc(s.k.ncands, sizeof(float));
    float *grad = calloc(mlp.nparams, sizeof(float));
    target[3] = 1.0f; // pretend the expert likes word 3

    float before = loss_at(&s, target);
    for (int step = 0; step < 50; step++)
    {
        memset(grad, 0, mlp.nparams * sizeof(float));
        solver_evaluate(&s);
        solver_backward(&s, target, grad);
        adam_step(&adam, mlp.params, grad, 1.0f);
    }
    float after = loss_at(&s, target);
    CHECK(after < before * 0.5f);
    CHECK_INT(solver_best(&s), s.k.cands[3]);

    free(grad);
    free(target);
    adam_free(&adam);
    solver_free(&s);
    mlp_free(&mlp);
    wordlist_free(&words);
}

static void shipped_model_solves_the_game(void)
{
    // End-to-end check of models/solver.nn on the real word list.
    Mlp mlp;
    if (mlp_load(&mlp, SIXDLE_MODEL) != 0)
    {
        printf("    (skipped: no model at %s, run `make train`)\n",
               SIXDLE_MODEL);
        return;
    }
    WordList words;
    REQUIRE(wordlist_load(&words, SIXDLE_WORDS) == 0);
    Solver s;
    REQUIRE(solver_init(&s, &words, &mlp) == 0);

    Rng rng;
    rng_seed(&rng, 2024);
    const int games = 300;
    int won = 0;
    int total_guesses = 0;
    for (int g = 0; g < games; g++)
    {
        Game game;
        game_start_random(&game, &words, &rng);
        solver_reset(&s);
        while (game.status == GAME_PLAYING)
        {
            const char *guess = words.items[solver_best(&s)];
            REQUIRE(game_guess(&game, guess) == GUESS_OK);
            solver_apply(&s, guess, game.patterns[game.nguesses - 1]);
        }
        if (game.status == GAME_WON)
        {
            won++;
            total_guesses += game.nguesses;
        }
    }
    printf("    shipped model: won %d/%d, %.2f guesses on average\n", won,
           games, (double)total_guesses / (won ? won : 1));
    // Training measures ~98.8% (the expert it imitates: ~99%).
    CHECK(won >= games * 95 / 100);
    CHECK((double)total_guesses / won < 4.2);

    solver_free(&s);
    wordlist_free(&words);
    mlp_free(&mlp);
}

void solver_tests(void)
{
    RUN(rejects_mismatched_model);
    RUN(probabilities_and_ranking);
    RUN(plays_legal_games_to_the_end);
    RUN(no_word_fits_a_secret_outside_the_list);
    RUN(backward_matches_numerical_gradient);
    RUN(training_step_lowers_the_loss);
    RUN(shipped_model_solves_the_game);
}

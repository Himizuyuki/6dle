/*
 * Which first guess is best?
 *
 * For two ways of picking the secret ("Any word": all words equally likely,
 * and "Normal": mostly common words), every word is scored as an opener over
 * all possible secrets:
 *   - bits:  expected information (entropy of the colours it shows)
 *   - left:  expected number of words still possible afterwards
 *   - worst: largest number of words that can remain
 *   - est.:  the expert's estimate of the total number of guesses
 * Then, with --play N, the network solves every secret starting with each of
 * the N best openers (and with its own favourite), which measures what the
 * opener is really worth: win rate and average number of guesses. The same
 * is done for the best familiar openers (Zipf >= 3), the ones a player is
 * likely to know.
 *
 * Usage: see usage() or `make openers`.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/game.h"
#include "core/prior.h"
#include "core/words.h"
#include "parallel.h"
#include "solver/expert.h"
#include "solver/solver.h"

// "Familiar" openers: words most players know.
#define OPENER_FAMILIAR 3.0f

typedef struct Options
{
    const char *words;
    const char *freq;
    const char *model;
    size_t top;
    size_t play;
    int threads;
} Options;

typedef struct StatsJob
{
    const WordList *words;
    const float *weights;
    const uint32_t *all;
    size_t begin;
    size_t end;
    GuessStats *out;
} StatsJob;

static void *stats_worker(void *arg)
{
    StatsJob *job = arg;
    for (size_t i = job->begin; i < job->end; i++)
        expert_stats(job->words, job->words->items[i], job->all, job->weights,
                     job->words->count, &job->out[i]);
    return NULL;
}

typedef struct PlayJob
{
    const WordList *words;
    const Mlp *model;
    const Prior *prior; // to weight the secrets
    uint32_t opener;
    size_t begin;
    size_t end;
    double won; // probability mass of the secrets solved
    double guesses; // sum of probability * guesses over solved secrets
    int worst; // most guesses needed by a solved secret
    size_t lost; // number of secrets not found in MAX_GUESSES
} PlayJob;

static void *play_worker(void *arg)
{
    PlayJob *job = arg;
    Solver net;
    if (solver_init(&net, job->words, job->model, NULL) != 0)
        exit(EXIT_FAILURE);
    for (size_t secret = job->begin; secret < job->end; secret++)
    {
        const char *word = job->words->items[secret];
        solver_reset(&net);
        int solved = 0;
        for (int t = 0; t < MAX_GUESSES && !solved; t++)
        {
            uint32_t guess = t == 0 ? job->opener : solver_best(&net);
            Pattern p = pattern_compute(job->words->items[guess], word);
            solver_apply(&net, job->words->items[guess], p);
            if (p == PATTERN_WIN)
                solved = t + 1;
        }
        double w = job->prior->weight[secret];
        if (solved)
        {
            job->won += w;
            job->guesses += w * solved;
            if (solved > job->worst)
                job->worst = solved;
        }
        else
            job->lost++;
    }
    solver_free(&net);
    return NULL;
}

// Play every secret with the given opener; weighted by the prior.
static void play(const Options *opt, const WordList *words, const Mlp *model,
                 const Prior *prior, uint32_t opener)
{
    PlayJob jobs[MAX_THREADS];
    size_t n = words->count;
    for (int t = 0; t < opt->threads; t++)
    {
        jobs[t] = (PlayJob){
            .words = words,
            .model = model,
            .prior = prior,
            .opener = opener,
            .begin = n * (size_t)t / (size_t)opt->threads,
            .end = n * (size_t)(t + 1) / (size_t)opt->threads,
        };
    }
    run_parallel(play_worker, jobs, sizeof(*jobs), opt->threads);
    double won = 0.0;
    double guesses = 0.0;
    int worst = 0;
    size_t lost = 0;
    for (int t = 0; t < opt->threads; t++)
    {
        won += jobs[t].won;
        guesses += jobs[t].guesses;
        lost += jobs[t].lost;
        if (jobs[t].worst > worst)
            worst = jobs[t].worst;
    }
    printf("  %7.3f%%  %6.3f  ", 100.0 * won, guesses / won);
    if (lost > 0)
        printf("lost %zu", lost);
    else
        printf("max %d", worst);
}

static const GuessStats *g_stats;

static int by_cost(const void *a, const void *b)
{
    double x = g_stats[*(const uint32_t *)a].cost;
    double y = g_stats[*(const uint32_t *)b].cost;
    return (x > y) - (x < y);
}

static void analyse(const Options *opt, const WordList *words, const Mlp *model,
                    const Prior *prior)
{
    size_t n = words->count;
    const float *weights = prior->kind == PRIOR_UNIFORM ? NULL : prior->weight;
    uint32_t *order = malloc(n * sizeof(*order));
    GuessStats *stats = malloc(n * sizeof(*stats));
    if (order == NULL || stats == NULL)
        exit(EXIT_FAILURE);
    for (size_t i = 0; i < n; i++)
        order[i] = (uint32_t)i;

    StatsJob jobs[MAX_THREADS];
    for (int t = 0; t < opt->threads; t++)
        jobs[t] = (StatsJob){ words,
                              weights,
                              order,
                              n * (size_t)t / (size_t)opt->threads,
                              n * (size_t)(t + 1) / (size_t)opt->threads,
                              stats };
    run_parallel(stats_worker, jobs, sizeof(*jobs), opt->threads);
    g_stats = stats;
    qsort(order, n, sizeof(*order), by_cost);

    size_t best_bits = 0;
    size_t best_left = 0;
    size_t best_worst = 0;
    for (size_t i = 1; i < n; i++)
    {
        if (stats[i].entropy > stats[best_bits].entropy)
            best_bits = i;
        if (stats[i].expected_left < stats[best_left].expected_left)
            best_left = i;
        if (stats[i].worst_left < stats[best_worst].worst_left)
            best_worst = i;
    }

    Solver net;
    int have_net = model != NULL && solver_init(&net, words, model, NULL) == 0;
    uint32_t favourite = have_net ? solver_best(&net) : UINT32_MAX;
    if (have_net)
        solver_free(&net);

    printf("\n## Difficulty: %s\n\n", prior_name(prior->kind));
    // Twice: all words, then only familiar ones (that a player would think
    // of).
    for (int familiar = 0; familiar <= 1; familiar++)
    {
        if (familiar && words->zipf == NULL)
            break;
        if (familiar)
            printf("\nFamiliar openers (Zipf >= %.1f):\n",
                   (double)OPENER_FAMILIAR);
        printf("rank  opener   bits  left    worst  est.");
        if (have_net && opt->play > 0)
            printf("   |  played: won     guesses  lost / max guesses");
        printf("\n");
        size_t shown = 0;
        for (size_t r = 0; r < n && shown < opt->top; r++)
        {
            if (familiar && words->zipf[order[r]] < OPENER_FAMILIAR)
                continue;
            const GuessStats *s = &stats[order[r]];
            printf("%3zu.  %s  %5.2f  %6.1f  %5.0f  %.3f", r + 1,
                   words->items[order[r]], s->entropy, s->expected_left,
                   s->worst_left, s->cost);
            if (have_net && shown < (familiar ? opt->play / 2 : opt->play))
            {
                printf("   |");
                play(opt, words, model, prior, order[r]);
            }
            printf("%s\n",
                   order[r] == favourite ? "   <- network's choice" : "");
            fflush(stdout);
            shown++;
        }
    }
    printf("\nmost information:  %s (%.2f bits)\n", words->items[best_bits],
           stats[best_bits].entropy);
    printf("fewest words left: %s (%.1f on average)\n", words->items[best_left],
           stats[best_left].expected_left);
    printf("best worst case:   %s (at most %.0f words left)\n",
           words->items[best_worst], stats[best_worst].worst_left);
    if (have_net && opt->play > 0)
    {
        size_t rank = 0;
        while (order[rank] != favourite)
            rank++;
        printf("network's opener:  %s (rank %zu by estimate)",
               words->items[favourite], rank + 1);
        if (rank >= opt->top || rank >= opt->play)
        {
            printf(", played:");
            play(opt, words, model, prior, favourite);
        }
        printf("\n");
    }
    free(order);
    free(stats);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [options]\n"
            "  --words FILE   word list (default data/wordBank.txt)\n"
            "  --freq FILE    word frequencies (default data/wordFreq.txt)\n"
            "  --model FILE   network used for --play (default "
            "models/solver.nn)\n"
            "  --top N        openers listed (default 15)\n"
            "  --play N       of those, played against every secret "
            "(default 8)\n"
            "  --threads N    worker threads (default: all CPUs)\n",
            prog);
}

int main(int argc, char **argv)
{
    Options opt = { .words = "data/wordBank.txt",
                    .freq = "data/wordFreq.txt",
                    .model = "models/solver.nn",
                    .top = 15,
                    .play = 8,
                    .threads = cpu_count() };
    for (int i = 1; i < argc; i++)
    {
        const char *val = i + 1 < argc ? argv[i + 1] : NULL;
        if (val == NULL)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        if (strcmp(argv[i], "--words") == 0)
            opt.words = val;
        else if (strcmp(argv[i], "--freq") == 0)
            opt.freq = val;
        else if (strcmp(argv[i], "--model") == 0)
            opt.model = val;
        else if (strcmp(argv[i], "--top") == 0)
            opt.top = strtoul(val, NULL, 10);
        else if (strcmp(argv[i], "--play") == 0)
            opt.play = strtoul(val, NULL, 10);
        else if (strcmp(argv[i], "--threads") == 0)
            opt.threads = atoi(val);
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        i++;
    }
    if (opt.threads < 1 || opt.threads > MAX_THREADS)
        opt.threads = cpu_count();

    WordList words;
    if (wordlist_load(&words, opt.words) != 0)
    {
        fprintf(stderr, "cannot load word list %s: %s\n", opt.words,
                strerror(errno));
        return EXIT_FAILURE;
    }
    int have_freq = wordlist_load_frequencies(&words, opt.freq) == 0;
    Mlp model;
    int have_model = mlp_load(&model, opt.model) == 0;
    if (!have_model)
        fprintf(stderr, "no model at %s: skipping the games\n", opt.model);

    printf("# Best opening words (%zu words)\n", words.count);
    printf("\nbits: expected information, left: expected words left, "
           "worst: most words left,\nest.: expert's estimate of the "
           "guesses needed; played: network solving every secret\n"
           "(weighted by how likely it is) after this opener.\n");
    static const PriorKind KINDS[] = { PRIOR_UNIFORM, PRIOR_NORMAL };
    for (size_t i = 0; i < 2; i++)
    {
        if (KINDS[i] != PRIOR_UNIFORM && !have_freq)
            break;
        Prior prior;
        if (prior_init(&prior, &words, KINDS[i]) != 0)
            return EXIT_FAILURE;
        analyse(&opt, &words, have_model ? &model : NULL, &prior);
        prior_free(&prior);
    }

    if (have_model)
        mlp_free(&model);
    wordlist_free(&words);
    return EXIT_SUCCESS;
}

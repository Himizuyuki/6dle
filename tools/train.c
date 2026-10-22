/*
 * Trains the solver network by imitation learning.
 *
 * 1. The entropy expert (src/solver/expert.c) plays thousands of random games.
 *    At every turn we record its preference over all still-possible words
 *    (a softmax of their entropies) - the "soft target".
 * 2. The network is trained to reproduce those targets (cross-entropy).
 * 3. DAgger: the trained network plays its own games, the expert labels the
 *    positions it reaches, and training continues on the union. This teaches
 *    the network to recover from its own mistakes.
 *
 * Usage: see usage() or `make train`.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "core/game.h"
#include "core/pattern.h"
#include "core/rng.h"
#include "core/words.h"
#include "solver/expert.h"
#include "solver/knowledge.h"
#include "solver/nn.h"
#include "solver/solver.h"

#define TOPK 24 // best guesses kept per recorded position
#define MAX_THREADS 64

typedef struct Options
{
    const char *words;
    const char *out;
    const char *eval_only;
    size_t games;
    int epochs;
    int hidden;
    int dagger;
    size_t dagger_games;
    int dagger_epochs;
    int threads;
    uint64_t seed;
    size_t eval;
    int eval_expert;
    float lr;
    float temperature; // of the soft targets, in bits
    size_t max_answers; // entropy is estimated on at most this many answers
    size_t batch; // games per optimiser step
} Options;

typedef struct Target
{
    uint32_t word[TOPK];
    float prob[TOPK];
    unsigned char n; // 0: nothing to learn here (single candidate)
} Target;

typedef struct Record
{
    uint32_t secret;
    unsigned char nguesses;
    uint32_t guess[MAX_GUESSES];
    Target target[MAX_GUESSES];
} Record;

typedef struct Dataset
{
    Record *items;
    size_t count;
} Dataset;

typedef struct Shared
{
    const Options *opt;
    const WordList *words;
    const Mlp *model;
    const float *opener; // expert entropy of every word on the first turn
} Shared;

static double now(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (p == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static size_t sample(const float *probs, size_t n, Rng *rng)
{
    double r = rng_double(rng);
    for (size_t i = 0; i < n; i++)
    {
        r -= probs[i];
        if (r < 0.0)
            return i;
    }
    return n - 1;
}

static size_t argmax(const float *v, size_t n)
{
    size_t best = 0;
    for (size_t i = 1; i < n; i++)
    {
        if (v[i] > v[best])
            best = i;
    }
    return best;
}

// Run fn(job) for every job, one thread each.
static void run_parallel(void *(*fn)(void *), void *jobs, size_t job_size,
                         int njobs)
{
    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < njobs; i++)
    {
        void *job = (char *)jobs + (size_t)i * job_size;
        if (pthread_create(&threads[i], NULL, fn, job) != 0)
        {
            fprintf(stderr, "pthread_create failed\n");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < njobs; i++)
        pthread_join(threads[i], NULL);
}

/* ---------------------------------------------------------------- expert */

typedef struct ExpertBuffers
{
    float *scores;
    float *probs;
    uint32_t *answers;
} ExpertBuffers;

static void buffers_init(ExpertBuffers *b, const Shared *sh)
{
    b->scores = xmalloc(sh->words->count * sizeof(float));
    b->probs = xmalloc(sh->words->count * sizeof(float));
    b->answers = xmalloc(sh->opt->max_answers * sizeof(uint32_t));
}

static void buffers_free(ExpertBuffers *b)
{
    free(b->scores);
    free(b->probs);
    free(b->answers);
}

// Expert entropies of every candidate in b->scores.
static void expert_evaluate(const Shared *sh, const Knowledge *k,
                            ExpertBuffers *b)
{
    if (k->turn == 0)
    {
        memcpy(b->scores, sh->opener, k->ncands * sizeof(float));
        return;
    }
    const uint32_t *answers = k->cands;
    size_t nanswers = k->ncands;
    if (nanswers > sh->opt->max_answers)
    {
        // Evenly spaced subset: an unbiased, deterministic estimate.
        nanswers = sh->opt->max_answers;
        for (size_t i = 0; i < nanswers; i++)
            b->answers[i] = k->cands[i * k->ncands / nanswers];
        answers = b->answers;
    }
    expert_scores(sh->words, k->cands, k->ncands, answers, nanswers, b->scores);
}

typedef struct OpenerJob
{
    const WordList *words;
    uint32_t *all;
    size_t begin;
    size_t end;
    float *out;
} OpenerJob;

static void *opener_worker(void *arg)
{
    OpenerJob *job = arg;
    expert_scores(job->words, job->all + job->begin, job->end - job->begin,
                  job->all, job->words->count, job->out + job->begin);
    return NULL;
}

// The first move is the same in every game, so its (expensive, exact)
// entropies are computed once.
static float *compute_opener(const WordList *words, int nthreads)
{
    size_t n = words->count;
    uint32_t *all = xmalloc(n * sizeof(*all));
    for (size_t i = 0; i < n; i++)
        all[i] = (uint32_t)i;
    float *out = xmalloc(n * sizeof(*out));
    OpenerJob jobs[MAX_THREADS];
    for (int t = 0; t < nthreads; t++)
    {
        jobs[t] = (OpenerJob){ words, all, n * (size_t)t / (size_t)nthreads,
                               n * (size_t)(t + 1) / (size_t)nthreads, out };
    }
    run_parallel(opener_worker, jobs, sizeof(*jobs), nthreads);
    free(all);
    return out;
}

/* ------------------------------------------------------- data generation */

static void store_target(Target *t, const Knowledge *k, const float *probs)
{
    t->n = 0;
    if (k->ncands < 2)
        return;
    // Keep the TOPK most probable entries (insertion into a sorted array).
    for (size_t c = 0; c < k->ncands; c++)
    {
        if (t->n == TOPK && !(probs[c] > t->prob[TOPK - 1]))
            continue;
        size_t pos = t->n < TOPK ? t->n++ : TOPK - 1;
        while (pos > 0 && t->prob[pos - 1] < probs[c])
        {
            t->prob[pos] = t->prob[pos - 1];
            t->word[pos] = t->word[pos - 1];
            pos--;
        }
        t->prob[pos] = probs[c];
        t->word[pos] = k->cands[c];
    }
    float total = 0.0f;
    for (size_t i = 0; i < t->n; i++)
        total += t->prob[i];
    for (size_t i = 0; i < t->n; i++)
        t->prob[i] /= total;
}

typedef struct GenJob
{
    const Shared *sh;
    Record *out;
    size_t count;
    uint64_t seed;
    int use_model; // behaviour policy: network (DAgger) or expert
} GenJob;

static void *gen_worker(void *arg)
{
    GenJob *job = arg;
    const Shared *sh = job->sh;
    const WordList *words = sh->words;
    Rng rng;
    rng_seed(&rng, job->seed);
    ExpertBuffers b;
    buffers_init(&b, sh);
    Solver net;
    Knowledge plain;
    Knowledge *k = &plain;
    if (job->use_model)
    {
        if (solver_init(&net, words, sh->model) != 0)
            exit(EXIT_FAILURE);
        k = &net.k;
    }
    else if (knowledge_init(&plain, words) != 0)
        exit(EXIT_FAILURE);

    for (size_t g = 0; g < job->count; g++)
    {
        Record *rec = &job->out[g];
        rec->secret = (uint32_t)rng_below(&rng, words->count);
        rec->nguesses = 0;
        const char *secret = words->items[rec->secret];
        knowledge_reset(k);
        for (int t = 0; t < MAX_GUESSES; t++)
        {
            expert_evaluate(sh, k, &b);
            expert_soft_targets(b.scores, k->ncands, sh->opt->temperature,
                                b.probs);
            store_target(&rec->target[t], k, b.probs);

            size_t pick;
            if (job->use_model)
            {
                net.evaluated = 0;
                solver_evaluate(&net);
                pick = sample(net.probs, k->ncands, &rng);
            }
            else
                pick = sample(b.probs, k->ncands, &rng);

            const char *guess = words->items[k->cands[pick]];
            rec->guess[t] = k->cands[pick];
            rec->nguesses++;
            Pattern p = pattern_compute(guess, secret);
            knowledge_apply(k, guess, p);
            if (p == PATTERN_WIN)
                break;
        }
    }

    if (job->use_model)
        solver_free(&net);
    else
        knowledge_free(&plain);
    buffers_free(&b);
    return NULL;
}

static void generate(Dataset *data, const Shared *sh, size_t games,
                     uint64_t seed, int use_model)
{
    int nthreads = sh->opt->threads;
    data->items =
        realloc(data->items, (data->count + games) * sizeof(*data->items));
    if (data->items == NULL)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    GenJob jobs[MAX_THREADS];
    for (int t = 0; t < nthreads; t++)
    {
        size_t begin = games * (size_t)t / (size_t)nthreads;
        size_t end = games * (size_t)(t + 1) / (size_t)nthreads;
        jobs[t] = (GenJob){ sh, data->items + data->count + begin, end - begin,
                            seed * 1000003u + (uint64_t)t, use_model };
    }
    run_parallel(gen_worker, jobs, sizeof(*jobs), nthreads);
    data->count += games;
}

/* -------------------------------------------------------------- training */

typedef struct TrainJob
{
    const Shared *sh;
    Solver solver;
    float *grad;
    float *target;
    Record **recs;
    size_t nrecs;
    double loss;
    size_t states;
} TrainJob;

static int compare_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a;
    uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static void *train_worker(void *arg)
{
    TrainJob *job = arg;
    Solver *s = &job->solver;
    const WordList *words = job->sh->words;
    for (size_t r = 0; r < job->nrecs; r++)
    {
        const Record *rec = job->recs[r];
        const char *secret = words->items[rec->secret];
        solver_reset(s);
        for (int t = 0; t < rec->nguesses; t++)
        {
            const Target *tg = &rec->target[t];
            if (tg->n > 0)
            {
                solver_evaluate(s);
                memset(job->target, 0, s->k.ncands * sizeof(float));
                for (int i = 0; i < tg->n; i++)
                {
                    // cands is sorted, so find each target word's slot.
                    uint32_t *slot =
                        bsearch(&tg->word[i], s->k.cands, s->k.ncands,
                                sizeof(uint32_t), compare_u32);
                    if (slot != NULL)
                        job->target[slot - s->k.cands] = tg->prob[i];
                }
                job->loss += solver_backward(s, job->target, job->grad);
                job->states++;
            }
            const char *guess = words->items[rec->guess[t]];
            solver_apply(s, guess, pattern_compute(guess, secret));
        }
    }
    return NULL;
}

static void train(Mlp *model, Adam *adam, const Dataset *data, const Shared *sh,
                  int epochs, Rng *rng)
{
    const Options *opt = sh->opt;
    int nthreads = opt->threads;
    TrainJob *jobs = xmalloc((size_t)nthreads * sizeof(*jobs));
    for (int t = 0; t < nthreads; t++)
    {
        jobs[t].sh = sh;
        jobs[t].grad = xmalloc(model->nparams * sizeof(float));
        jobs[t].target = xmalloc(sh->words->count * sizeof(float));
        if (solver_init(&jobs[t].solver, sh->words, model) != 0)
            exit(EXIT_FAILURE);
    }
    float *grad = xmalloc(model->nparams * sizeof(float));
    Record **order = xmalloc(data->count * sizeof(*order));
    for (size_t i = 0; i < data->count; i++)
        order[i] = &data->items[i];

    float base_lr = adam->lr;
    for (int epoch = 0; epoch < epochs; epoch++)
    {
        double start = now();
        // Linear decay to 10% of the base learning rate.
        adam->lr = base_lr * (1.0f - 0.9f * (float)epoch / (float)epochs);
        for (size_t i = data->count; i > 1; i--)
        {
            size_t j = rng_below(rng, i);
            Record *tmp = order[i - 1];
            order[i - 1] = order[j];
            order[j] = tmp;
        }
        double loss = 0.0;
        size_t states = 0;
        for (size_t first = 0; first < data->count; first += opt->batch)
        {
            size_t n = data->count - first < opt->batch ? data->count - first
                                                        : opt->batch;
            for (int t = 0; t < nthreads; t++)
            {
                size_t begin = n * (size_t)t / (size_t)nthreads;
                size_t end = n * (size_t)(t + 1) / (size_t)nthreads;
                jobs[t].recs = order + first + begin;
                jobs[t].nrecs = end - begin;
                jobs[t].loss = 0.0;
                jobs[t].states = 0;
                memset(jobs[t].grad, 0, model->nparams * sizeof(float));
            }
            run_parallel(train_worker, jobs, sizeof(*jobs), nthreads);

            memset(grad, 0, model->nparams * sizeof(float));
            size_t batch_states = 0;
            for (int t = 0; t < nthreads; t++)
            {
                for (size_t i = 0; i < model->nparams; i++)
                    grad[i] += jobs[t].grad[i];
                loss += jobs[t].loss;
                batch_states += jobs[t].states;
            }
            states += batch_states;
            if (batch_states > 0)
                adam_step(adam, model->params, grad,
                          1.0f / (float)batch_states);
        }
        printf("  epoch %2d/%d  loss %.4f  lr %.2e  (%zu positions, %.1fs)\n",
               epoch + 1, epochs, loss / (double)(states ? states : 1),
               (double)adam->lr, states, now() - start);
        fflush(stdout);
    }
    adam->lr = base_lr;

    for (int t = 0; t < nthreads; t++)
    {
        solver_free(&jobs[t].solver);
        free(jobs[t].grad);
        free(jobs[t].target);
    }
    free(jobs);
    free(grad);
    free(order);
}

/* ------------------------------------------------------------ evaluation */

typedef struct EvalJob
{
    const Shared *sh;
    size_t count;
    uint64_t seed;
    int expert;
    size_t histogram[MAX_GUESSES + 1]; // [0] = lost, [n] = won in n
} EvalJob;

static void *eval_worker(void *arg)
{
    EvalJob *job = arg;
    const Shared *sh = job->sh;
    const WordList *words = sh->words;
    Rng rng;
    rng_seed(&rng, job->seed);
    Solver net;
    Knowledge plain;
    Knowledge *k = &plain;
    ExpertBuffers b;
    if (job->expert)
    {
        buffers_init(&b, sh);
        if (knowledge_init(&plain, words) != 0)
            exit(EXIT_FAILURE);
    }
    else
    {
        if (solver_init(&net, words, sh->model) != 0)
            exit(EXIT_FAILURE);
        k = &net.k;
    }

    for (size_t g = 0; g < job->count; g++)
    {
        const char *secret = words->items[rng_below(&rng, words->count)];
        knowledge_reset(k);
        int solved = 0;
        for (int t = 0; t < MAX_GUESSES && !solved; t++)
        {
            uint32_t guess;
            if (job->expert)
            {
                expert_evaluate(sh, k, &b);
                guess = k->cands[argmax(b.scores, k->ncands)];
            }
            else
            {
                net.evaluated = 0;
                guess = solver_best(&net);
            }
            Pattern p = pattern_compute(words->items[guess], secret);
            knowledge_apply(k, words->items[guess], p);
            if (p == PATTERN_WIN)
                solved = t + 1;
        }
        job->histogram[solved]++;
    }

    if (job->expert)
    {
        buffers_free(&b);
        knowledge_free(&plain);
    }
    else
        solver_free(&net);
    return NULL;
}

static void evaluate(const Shared *sh, size_t games, uint64_t seed, int expert,
                     const char *label)
{
    int nthreads = sh->opt->threads;
    EvalJob jobs[MAX_THREADS];
    for (int t = 0; t < nthreads; t++)
    {
        jobs[t] = (EvalJob){ .sh = sh,
                             .count = games * (size_t)(t + 1) / (size_t)nthreads
                                 - games * (size_t)t / (size_t)nthreads,
                             .seed = seed * 7919u + (uint64_t)t,
                             .expert = expert };
    }
    double start = now();
    run_parallel(eval_worker, jobs, sizeof(*jobs), nthreads);

    size_t histogram[MAX_GUESSES + 1] = { 0 };
    for (int t = 0; t < nthreads; t++)
    {
        for (int i = 0; i <= MAX_GUESSES; i++)
            histogram[i] += jobs[t].histogram[i];
    }
    size_t won = games - histogram[0];
    double total = 0.0;
    for (int i = 1; i <= MAX_GUESSES; i++)
        total += (double)i * (double)histogram[i];
    printf("%s: won %zu/%zu (%.2f%%), %.3f guesses on average when won "
           "(%.1fs)\n  guesses:",
           label, won, games, 100.0 * (double)won / (double)games,
           won ? total / (double)won : 0.0, now() - start);
    for (int i = 1; i <= MAX_GUESSES; i++)
        printf(" %d:%zu", i, histogram[i]);
    printf(" lost:%zu\n", histogram[0]);
    fflush(stdout);
}

/* ------------------------------------------------------------------ main */

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [options]\n"
            "  --words FILE       word list (default data/wordBank.txt)\n"
            "  --out FILE         model to write (default models/solver.nn)\n"
            "  --games N          expert games to imitate (default 20000)\n"
            "  --epochs N         passes over them (default 8)\n"
            "  --dagger N         rounds on the network's own games "
            "(default 2)\n"
            "  --dagger-games N   games per round (default 10000)\n"
            "  --dagger-epochs N  epochs per round (default 4)\n"
            "  --hidden N         width of the two hidden layers "
            "(default 256)\n"
            "  --threads N        worker threads (default: all CPUs)\n"
            "  --seed N           random seed (default 1)\n"
            "  --eval N           games to evaluate on (default 2000)\n"
            "  --eval-only FILE   only evaluate an existing model\n"
            "  --expert           also evaluate the expert itself\n",
            prog);
}

static int parse_options(Options *opt, int argc, char **argv)
{
    *opt = (Options){ .words = "data/wordBank.txt",
                      .out = "models/solver.nn",
                      .games = 20000,
                      .epochs = 8,
                      .hidden = 256,
                      .dagger = 2,
                      .dagger_games = 10000,
                      .dagger_epochs = 4,
                      .threads = (int)sysconf(_SC_NPROCESSORS_ONLN),
                      .seed = 1,
                      .eval = 2000,
                      .lr = 1e-3f,
                      .temperature = 0.1f,
                      .max_answers = 2000,
                      .batch = 32 };
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        const char *val = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(arg, "--expert") == 0)
        {
            opt->eval_expert = 1;
            continue;
        }
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 || !val)
            return -1;
        i++;
        if (strcmp(arg, "--words") == 0)
            opt->words = val;
        else if (strcmp(arg, "--out") == 0)
            opt->out = val;
        else if (strcmp(arg, "--eval-only") == 0)
            opt->eval_only = val;
        else if (strcmp(arg, "--games") == 0)
            opt->games = strtoul(val, NULL, 10);
        else if (strcmp(arg, "--epochs") == 0)
            opt->epochs = atoi(val);
        else if (strcmp(arg, "--dagger") == 0)
            opt->dagger = atoi(val);
        else if (strcmp(arg, "--dagger-games") == 0)
            opt->dagger_games = strtoul(val, NULL, 10);
        else if (strcmp(arg, "--dagger-epochs") == 0)
            opt->dagger_epochs = atoi(val);
        else if (strcmp(arg, "--hidden") == 0)
            opt->hidden = atoi(val);
        else if (strcmp(arg, "--threads") == 0)
            opt->threads = atoi(val);
        else if (strcmp(arg, "--seed") == 0)
            opt->seed = strtoull(val, NULL, 10);
        else if (strcmp(arg, "--eval") == 0)
            opt->eval = strtoul(val, NULL, 10);
        else
            return -1;
    }
    if (opt->threads < 1)
        opt->threads = 1;
    if (opt->threads > MAX_THREADS)
        opt->threads = MAX_THREADS;
    return opt->hidden > 0 && opt->epochs >= 0 ? 0 : -1;
}

int main(int argc, char **argv)
{
    Options opt;
    if (parse_options(&opt, argc, argv) != 0)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    WordList words;
    if (wordlist_load(&words, opt.words) != 0)
    {
        fprintf(stderr, "cannot load word list %s: %s\n", opt.words,
                strerror(errno));
        return EXIT_FAILURE;
    }
    printf("%zu words, %d threads\n", words.count, opt.threads);

    Mlp model;
    Rng rng;
    rng_seed(&rng, opt.seed);
    if (opt.eval_only)
    {
        if (mlp_load(&model, opt.eval_only) != 0)
        {
            fprintf(stderr, "cannot load model %s\n", opt.eval_only);
            return EXIT_FAILURE;
        }
    }
    else
    {
        int sizes[] = { FEATURE_DIM, opt.hidden, opt.hidden, WORD_FEATURE_DIM };
        if (mlp_init(&model, sizes, 3, &rng) != 0)
            return EXIT_FAILURE;
    }
    printf("network: %d -> %d -> %d -> %d (%zu parameters)\n", model.sizes[0],
           model.sizes[1], model.sizes[2], model.sizes[3], model.nparams);

    double start = now();
    printf("computing the expert's first move over all words...\n");
    fflush(stdout);
    float *opener = compute_opener(&words, opt.threads);
    size_t best = argmax(opener, words.count);
    printf("expert opener: %s (%.3f bits, %.1fs)\n", words.items[best],
           (double)opener[best], now() - start);

    Shared sh = { &opt, &words, &model, opener };
    if (!opt.eval_only)
    {
        Adam adam;
        if (adam_init(&adam, model.nparams, opt.lr) != 0)
            return EXIT_FAILURE;
        Dataset data = { NULL, 0 };

        start = now();
        printf("expert plays %zu games...\n", opt.games);
        fflush(stdout);
        generate(&data, &sh, opt.games, opt.seed, 0);
        printf("  done in %.1fs\n", now() - start);
        printf("training (%d epochs)\n", opt.epochs);
        train(&model, &adam, &data, &sh, opt.epochs, &rng);

        for (int round = 1; round <= opt.dagger; round++)
        {
            evaluate(&sh, opt.eval, opt.seed + 100 + (uint64_t)round, 0,
                     "network");
            start = now();
            printf("DAgger round %d/%d: network plays %zu games, expert "
                   "labels them...\n",
                   round, opt.dagger, opt.dagger_games);
            fflush(stdout);
            generate(&data, &sh, opt.dagger_games, opt.seed + (uint64_t)round,
                     1);
            printf("  done in %.1fs\n", now() - start);
            train(&model, &adam, &data, &sh, opt.dagger_epochs, &rng);
        }

        if (mlp_save(&model, opt.out) != 0)
        {
            fprintf(stderr, "cannot write %s: %s\n", opt.out, strerror(errno));
            return EXIT_FAILURE;
        }
        printf("saved %s\n", opt.out);
        free(data.items);
        adam_free(&adam);
    }

    evaluate(&sh, opt.eval, opt.seed + 99991u, 0, "network");
    if (opt.eval_expert)
        evaluate(&sh, opt.eval, opt.seed + 99991u, 1, "expert ");

    free(opener);
    mlp_free(&model);
    wordlist_free(&words);
    return EXIT_SUCCESS;
}

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include "solver/nn.h"
#include "test.h"

static void forward_matches_hand_computation(void)
{
    // 2 -> 2 (ReLU) -> 1. Layout per layer: weights input-major, biases.
    int sizes[] = { 2, 2, 1 };
    Mlp mlp;
    Rng rng;
    rng_seed(&rng, 1);
    REQUIRE(mlp_init(&mlp, sizes, 2, &rng) == 0);
    CHECK_INT(mlp.nparams, (2 * 2 + 2) + (2 * 1 + 1));
    float params[] = {
        1.0f,  -1.0f, // input 0 -> hidden 0, 1
        2.0f,  1.0f, // input 1 -> hidden 0, 1
        0.5f,  -3.0f, // hidden biases
        1.0f,  10.0f, // hidden 0, 1 -> output
        0.25f, // output bias
    };
    memcpy(mlp.params, params, sizeof(params));
    MlpWork work;
    REQUIRE(mlp_work_init(&work, &mlp) == 0);
    float x[] = { 1.0f, 2.0f };
    const float *y = mlp_forward(&mlp, &work, x);
    // h0 = relu(1 + 4 + 0.5) = 5.5, h1 = relu(-1 + 2 - 3) = 0
    CHECK_NEAR(y[0], 5.5 + 0.25, 1e-6);
    mlp_work_free(&work);
    mlp_free(&mlp);
}

// loss = sum_o c_o * y_o, so dloss/dy = c.
static double linear_loss(const Mlp *mlp, MlpWork *work, const float *x,
                          const float *c)
{
    const float *y = mlp_forward(mlp, work, x);
    double loss = 0.0;
    for (int o = 0; o < mlp->sizes[mlp->nlayers]; o++)
        loss += (double)c[o] * y[o];
    return loss;
}

static void backward_matches_numerical_gradient(void)
{
    int sizes[] = { 7, 9, 6, 4 };
    Mlp mlp;
    Rng rng;
    rng_seed(&rng, 5);
    REQUIRE(mlp_init(&mlp, sizes, 3, &rng) == 0);
    // Non-zero biases so some ReLUs are clearly on and others clearly off.
    for (size_t i = 0; i < mlp.nparams; i++)
        mlp.params[i] += (float)(rng_double(&rng) * 0.2 - 0.1);
    MlpWork work;
    REQUIRE(mlp_work_init(&work, &mlp) == 0);
    float x[] = { 0.5f, 0.0f, 1.0f, 0.25f, 0.0f, 0.75f, 1.0f }; // some zeros
    float c[] = { 1.0f, -2.0f, 0.5f, 3.0f };

    float *grad = calloc(mlp.nparams, sizeof(float));
    linear_loss(&mlp, &work, x, c);
    mlp_backward(&mlp, &work, c, grad);

    int bad = 0;
    const float h = 1e-2f;
    for (size_t i = 0; i < mlp.nparams; i++)
    {
        float saved = mlp.params[i];
        mlp.params[i] = saved + h;
        double up = linear_loss(&mlp, &work, x, c);
        mlp.params[i] = saved - h;
        double down = linear_loss(&mlp, &work, x, c);
        mlp.params[i] = saved;
        double numeric = (up - down) / (2.0 * h);
        // Skip parameters whose nudge flips a ReLU (kink in the loss).
        if (fabs(numeric - grad[i]) > 1e-3 * (1.0 + fabs(numeric)))
        {
            mlp.params[i] = saved + h / 10;
            double up2 = linear_loss(&mlp, &work, x, c);
            mlp.params[i] = saved;
            double one_sided =
                (up2 - linear_loss(&mlp, &work, x, c)) / (h / 10.0);
            if (fabs(one_sided - grad[i]) > 1e-2 * (1.0 + fabs(one_sided)))
                bad++;
        }
    }
    CHECK_INT(bad, 0);
    free(grad);
    mlp_work_free(&work);
    mlp_free(&mlp);
}

static void learns_xor(void)
{
    int sizes[] = { 2, 16, 1 };
    Mlp mlp;
    Rng rng;
    rng_seed(&rng, 3);
    REQUIRE(mlp_init(&mlp, sizes, 2, &rng) == 0);
    MlpWork work;
    REQUIRE(mlp_work_init(&work, &mlp) == 0);
    Adam adam;
    REQUIRE(adam_init(&adam, mlp.nparams, 0.01f) == 0);
    float *grad = calloc(mlp.nparams, sizeof(float));
    const float xs[4][2] = { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    const float ys[4] = { 0, 1, 1, 0 };
    double loss = 0.0;
    for (int step = 0; step < 2000; step++)
    {
        memset(grad, 0, mlp.nparams * sizeof(float));
        loss = 0.0;
        for (int i = 0; i < 4; i++)
        {
            const float *y = mlp_forward(&mlp, &work, xs[i]);
            float d = y[0] - ys[i];
            loss += 0.5 * d * d;
            mlp_backward(&mlp, &work, &d, grad);
        }
        adam_step(&adam, mlp.params, grad, 0.25f);
    }
    CHECK(loss < 1e-3);
    for (int i = 0; i < 4; i++)
        CHECK_NEAR(mlp_forward(&mlp, &work, xs[i])[0], ys[i], 0.1);
    free(grad);
    adam_free(&adam);
    mlp_work_free(&work);
    mlp_free(&mlp);
}

static void save_and_load_round_trip(void)
{
    char path[] = "/tmp/6dle-test-model-XXXXXX";
    int fd = mkstemp(path);
    REQUIRE(fd >= 0);
    close(fd);

    int sizes[] = { 5, 8, 3 };
    Mlp a;
    Mlp b;
    Rng rng;
    rng_seed(&rng, 11);
    REQUIRE(mlp_init(&a, sizes, 2, &rng) == 0);
    CHECK(mlp_save(&a, path) == 0);
    REQUIRE(mlp_load(&b, path) == 0);
    CHECK_INT(b.nlayers, 2);
    CHECK_INT(b.sizes[0], 5);
    CHECK_INT(b.sizes[2], 3);
    REQUIRE(b.nparams == a.nparams);
    CHECK(memcmp(a.params, b.params, a.nparams * sizeof(float)) == 0);
    mlp_free(&b);

    // Truncated file.
    FILE *f = fopen(path, "r+b");
    REQUIRE(f != NULL);
    REQUIRE(ftruncate(fileno(f), 40) == 0);
    fclose(f);
    CHECK(mlp_load(&b, path) == -1);
    CHECK(b.params == NULL);

    // Trailing garbage.
    CHECK(mlp_save(&a, path) == 0);
    f = fopen(path, "ab");
    fputc(0, f);
    fclose(f);
    CHECK(mlp_load(&b, path) == -1);

    // Not a model at all.
    CHECK(mlp_load(&b, FIXTURE("small.txt")) == -1);
    CHECK(mlp_load(&b, "/nonexistent/model.nn") == -1);

    unlink(path);
    mlp_free(&a);
}

static void rejects_bad_shapes(void)
{
    Mlp mlp;
    Rng rng;
    rng_seed(&rng, 1);
    int zero[] = { 3, 0, 2 };
    CHECK(mlp_init(&mlp, zero, 2, &rng) == -1);
    int many[] = { 1, 1, 1, 1, 1, 1 };
    CHECK(mlp_init(&mlp, many, 5, &rng) == -1);
}

void nn_tests(void)
{
    RUN(forward_matches_hand_computation);
    RUN(backward_matches_numerical_gradient);
    RUN(learns_xor);
    RUN(save_and_load_round_trip);
    RUN(rejects_bad_shapes);
}

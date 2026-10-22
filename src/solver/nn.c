#include "nn.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Layer l stores its weights input-major (w[i * out + o]) followed by its
// biases. Input-major lets forward and backward skip zero activations, and
// most inputs (and about half of the ReLU outputs) are zero.

static const char MAGIC[8] = { '6', 'D', 'L', 'E', 'M', 'L', 'P', '1' };
#define MAX_LAYER_SIZE 65536

static size_t layer_params(const Mlp *mlp, int l)
{
    return (size_t)mlp->sizes[l] * (size_t)mlp->sizes[l + 1]
        + (size_t)mlp->sizes[l + 1];
}

static int check_sizes(const int *sizes, int nlayers)
{
    if (nlayers < 1 || nlayers > MLP_MAX_LAYERS)
        return -1;
    for (int l = 0; l <= nlayers; l++)
    {
        if (sizes[l] < 1 || sizes[l] > MAX_LAYER_SIZE)
            return -1;
    }
    return 0;
}

static int mlp_alloc(Mlp *mlp, const int *sizes, int nlayers)
{
    memset(mlp, 0, sizeof(*mlp));
    if (check_sizes(sizes, nlayers) != 0)
        return -1;
    mlp->nlayers = nlayers;
    memcpy(mlp->sizes, sizes, (size_t)(nlayers + 1) * sizeof(*sizes));
    for (int l = 0; l < nlayers; l++)
        mlp->nparams += layer_params(mlp, l);
    mlp->params = calloc(mlp->nparams, sizeof(*mlp->params));
    return mlp->params ? 0 : -1;
}

int mlp_init(Mlp *mlp, const int *sizes, int nlayers, Rng *rng)
{
    if (mlp_alloc(mlp, sizes, nlayers) != 0)
        return -1;
    float *p = mlp->params;
    for (int l = 0; l < nlayers; l++)
    {
        size_t in = (size_t)sizes[l];
        size_t out = (size_t)sizes[l + 1];
        // He uniform: variance 2 / fan_in, suited to ReLU.
        float limit = sqrtf(6.0f / (float)in);
        for (size_t i = 0; i < in * out; i++)
            p[i] = (float)(rng_double(rng) * 2.0 - 1.0) * limit;
        p += in * out + out; // biases stay at zero
    }
    return 0;
}

void mlp_free(Mlp *mlp)
{
    free(mlp->params);
    mlp->params = NULL;
}

int mlp_work_init(MlpWork *work, const Mlp *mlp)
{
    memset(work, 0, sizeof(*work));
    for (int l = 0; l <= mlp->nlayers; l++)
    {
        work->act[l] = calloc((size_t)mlp->sizes[l], sizeof(float));
        work->delta[l] = calloc((size_t)mlp->sizes[l], sizeof(float));
        if (work->act[l] == NULL || work->delta[l] == NULL)
        {
            mlp_work_free(work);
            return -1;
        }
    }
    return 0;
}

void mlp_work_free(MlpWork *work)
{
    for (int l = 0; l <= MLP_MAX_LAYERS; l++)
    {
        free(work->act[l]);
        free(work->delta[l]);
        work->act[l] = NULL;
        work->delta[l] = NULL;
    }
}

const float *mlp_forward(const Mlp *mlp, MlpWork *work, const float *x)
{
    memcpy(work->act[0], x, (size_t)mlp->sizes[0] * sizeof(*x));
    const float *p = mlp->params;
    for (int l = 0; l < mlp->nlayers; l++)
    {
        size_t in = (size_t)mlp->sizes[l];
        size_t out = (size_t)mlp->sizes[l + 1];
        const float *w = p;
        const float *b = p + in * out;
        const float *a = work->act[l];
        float *y = work->act[l + 1];
        memcpy(y, b, out * sizeof(*y));
        for (size_t i = 0; i < in; i++)
        {
            float ai = a[i];
            if (ai == 0.0f)
                continue;
            const float *row = w + i * out;
            for (size_t o = 0; o < out; o++)
                y[o] += ai * row[o];
        }
        if (l < mlp->nlayers - 1)
        {
            for (size_t o = 0; o < out; o++)
                y[o] = y[o] > 0.0f ? y[o] : 0.0f;
        }
        p = b + out;
    }
    return work->act[mlp->nlayers];
}

void mlp_backward(const Mlp *mlp, MlpWork *work, const float *dout, float *grad)
{
    size_t offset[MLP_MAX_LAYERS];
    size_t total = 0;
    for (int l = 0; l < mlp->nlayers; l++)
    {
        offset[l] = total;
        total += layer_params(mlp, l);
    }

    memcpy(work->delta[mlp->nlayers], dout,
           (size_t)mlp->sizes[mlp->nlayers] * sizeof(*dout));
    for (int l = mlp->nlayers - 1; l >= 0; l--)
    {
        size_t in = (size_t)mlp->sizes[l];
        size_t out = (size_t)mlp->sizes[l + 1];
        const float *w = mlp->params + offset[l];
        float *gw = grad + offset[l];
        float *gb = gw + in * out;
        const float *a = work->act[l];
        const float *dy = work->delta[l + 1];
        float *dx = work->delta[l];

        for (size_t o = 0; o < out; o++)
            gb[o] += dy[o];
        for (size_t i = 0; i < in; i++)
        {
            float ai = a[i];
            if (ai == 0.0f)
            {
                // Zero input: no weight gradient, and for hidden layers the
                // ReLU was inactive so nothing flows further back either.
                dx[i] = 0.0f;
                continue;
            }
            const float *row = w + i * out;
            float *grow = gw + i * out;
            float sum = 0.0f;
            for (size_t o = 0; o < out; o++)
            {
                grow[o] += ai * dy[o];
                sum += row[o] * dy[o];
            }
            dx[i] = sum;
        }
    }
}

int mlp_save(const Mlp *mlp, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return -1;
    uint32_t header[MLP_MAX_LAYERS + 2];
    header[0] = (uint32_t)mlp->nlayers;
    for (int l = 0; l <= mlp->nlayers; l++)
        header[l + 1] = (uint32_t)mlp->sizes[l];
    int ok = fwrite(MAGIC, sizeof(MAGIC), 1, f) == 1
        && fwrite(header, sizeof(*header), (size_t)mlp->nlayers + 2, f)
            == (size_t)mlp->nlayers + 2
        && fwrite(mlp->params, sizeof(*mlp->params), mlp->nparams, f)
            == mlp->nparams;
    if (fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}

int mlp_load(Mlp *mlp, const char *path)
{
    memset(mlp, 0, sizeof(*mlp));
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return -1;

    char magic[sizeof(MAGIC)];
    uint32_t nlayers;
    int ok = fread(magic, sizeof(magic), 1, f) == 1
        && memcmp(magic, MAGIC, sizeof(MAGIC)) == 0
        && fread(&nlayers, sizeof(nlayers), 1, f) == 1 && nlayers >= 1
        && nlayers <= MLP_MAX_LAYERS;
    uint32_t raw[MLP_MAX_LAYERS + 1];
    int sizes[MLP_MAX_LAYERS + 1];
    if (ok)
        ok = fread(raw, sizeof(*raw), nlayers + 1, f) == nlayers + 1;
    for (uint32_t l = 0; ok && l <= nlayers; l++)
    {
        ok = raw[l] <= MAX_LAYER_SIZE;
        sizes[l] = (int)raw[l];
    }
    ok = ok && mlp_alloc(mlp, sizes, (int)nlayers) == 0;
    ok = ok
        && fread(mlp->params, sizeof(*mlp->params), mlp->nparams, f)
            == mlp->nparams
        && fgetc(f) == EOF;
    fclose(f);
    if (!ok)
    {
        mlp_free(mlp);
        return -1;
    }
    return 0;
}

int adam_init(Adam *adam, size_t n, float lr)
{
    adam->m = calloc(n, sizeof(float));
    adam->v = calloc(n, sizeof(float));
    adam->n = n;
    adam->step = 0;
    adam->lr = lr;
    if (adam->m == NULL || adam->v == NULL)
    {
        adam_free(adam);
        return -1;
    }
    return 0;
}

void adam_free(Adam *adam)
{
    free(adam->m);
    free(adam->v);
    adam->m = NULL;
    adam->v = NULL;
}

void adam_step(Adam *adam, float *params, const float *grad, float scale)
{
    const float b1 = 0.9f;
    const float b2 = 0.999f;
    const float eps = 1e-8f;
    adam->step++;
    float c1 = 1.0f - powf(b1, (float)adam->step);
    float c2 = 1.0f - powf(b2, (float)adam->step);
    float lr = adam->lr * sqrtf(c2) / c1;
    for (size_t i = 0; i < adam->n; i++)
    {
        float g = grad[i] * scale;
        adam->m[i] = b1 * adam->m[i] + (1.0f - b1) * g;
        adam->v[i] = b2 * adam->v[i] + (1.0f - b2) * g * g;
        params[i] -= lr * adam->m[i] / (sqrtf(adam->v[i]) + eps);
    }
}

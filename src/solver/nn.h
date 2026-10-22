#ifndef NN_H
#define NN_H

#include <stddef.h>

#include "core/rng.h"

// A plain feed-forward network (multi-layer perceptron): ReLU hidden layers,
// linear output. All weights and biases live in one flat array so the
// optimiser, gradient buffers and the file format stay trivial.
#define MLP_MAX_LAYERS 4

typedef struct Mlp
{
    int nlayers; // number of weight layers
    int sizes[MLP_MAX_LAYERS + 1]; // sizes[0] inputs ... sizes[nlayers] outputs
    size_t nparams;
    float *params;
} Mlp;

// Per-thread scratch space holding the activations of the last forward pass
// (needed by the backward pass).
typedef struct MlpWork
{
    float *act[MLP_MAX_LAYERS + 1];
    float *delta[MLP_MAX_LAYERS + 1];
} MlpWork;

// sizes has nlayers + 1 entries. Weights get He initialisation.
int mlp_init(Mlp *mlp, const int *sizes, int nlayers, Rng *rng);
void mlp_free(Mlp *mlp);

int mlp_work_init(MlpWork *work, const Mlp *mlp);
void mlp_work_free(MlpWork *work);

// Returns a pointer to the outputs, owned by work.
const float *mlp_forward(const Mlp *mlp, MlpWork *work, const float *x);

// Back-propagate dloss/doutput through the last forward pass of work and
// add the parameter gradients to grad (nparams floats).
void mlp_backward(const Mlp *mlp, MlpWork *work, const float *dout,
                  float *grad);

// Binary format: "6DLEMLP1", nlayers, sizes (uint32), params (float32),
// host byte order. Return 0 on success.
int mlp_save(const Mlp *mlp, const char *path);
int mlp_load(Mlp *mlp, const char *path);

typedef struct Adam
{
    float *m;
    float *v;
    size_t n;
    long step;
    float lr;
} Adam;

int adam_init(Adam *adam, size_t n, float lr);
void adam_free(Adam *adam);
// One update of params using grad * scale (scale = 1 / batch size).
void adam_step(Adam *adam, float *params, const float *grad, float scale);

#endif // NN_H

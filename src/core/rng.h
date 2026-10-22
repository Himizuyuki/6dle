#ifndef RNG_H
#define RNG_H

#include <stddef.h>
#include <stdint.h>

// Small, seedable PRNG (splitmix64). Seeding with a fixed value makes games
// and training runs reproducible, which the tests rely on.
typedef struct Rng
{
    uint64_t state;
} Rng;

void rng_seed(Rng *rng, uint64_t seed);

// Seed from /dev/urandom, falling back to the clock.
void rng_seed_random(Rng *rng);

uint64_t rng_next(Rng *rng);

// Uniform integer in [0, n) without modulo bias. n must be > 0.
size_t rng_below(Rng *rng, size_t n);

// Uniform double in [0, 1).
double rng_double(Rng *rng);

#endif // RNG_H

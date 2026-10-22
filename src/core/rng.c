#include "rng.h"

#include <stdio.h>
#include <time.h>

void rng_seed(Rng *rng, uint64_t seed)
{
    rng->state = seed;
}

void rng_seed_random(Rng *rng)
{
    uint64_t seed = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f == NULL || fread(&seed, sizeof(seed), 1, f) != 1)
    {
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);
        seed = (uint64_t)ts.tv_sec * 1000000007u + (uint64_t)ts.tv_nsec;
    }
    if (f != NULL)
        fclose(f);
    rng_seed(rng, seed);
}

uint64_t rng_next(Rng *rng)
{
    uint64_t z = (rng->state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

size_t rng_below(Rng *rng, size_t n)
{
    // Reject the top partial bucket so every value is equally likely.
    uint64_t limit = UINT64_MAX - UINT64_MAX % n;
    uint64_t r;
    do
        r = rng_next(rng);
    while (r >= limit);
    return (size_t)(r % n);
}

double rng_double(Rng *rng)
{
    return (double)(rng_next(rng) >> 11) * (1.0 / 9007199254740992.0);
}

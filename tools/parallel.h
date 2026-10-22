#ifndef PARALLEL_H
#define PARALLEL_H

#include <stddef.h>

#define MAX_THREADS 64

// Number of CPUs, clamped to [1, MAX_THREADS].
int cpu_count(void);

// Run fn(job) for each of the njobs jobs (an array of job_size-byte
// elements), one thread each, and wait for all of them.
void run_parallel(void *(*fn)(void *), void *jobs, size_t job_size, int njobs);

#endif // PARALLEL_H

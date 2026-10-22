#include "parallel.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int cpu_count(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1)
        return 1;
    return n > MAX_THREADS ? MAX_THREADS : (int)n;
}

void run_parallel(void *(*fn)(void *), void *jobs, size_t job_size, int njobs)
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

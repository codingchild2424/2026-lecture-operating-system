/*
 * race_demo.c - Race Condition Demonstration
 *
 * Multiple threads increment a shared counter without synchronization.
 * The final result will be less than expected due to race conditions.
 *
 * Compile: gcc -Wall -pthread -o race_demo race_demo.c
 * Run:     ./race_demo [num_threads] [increments_per_thread]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define DEFAULT_THREADS     4
#define DEFAULT_INCREMENTS  1000000

/* Shared counter - no protection */
volatile int counter = 0;

int increments_per_thread;

void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /*
         * This is NOT atomic. It compiles to roughly:
         *   1. LOAD counter -> register
         *   2. ADD 1 -> register
         *   3. STORE register -> counter
         *
         * Two threads can both read the same value,
         * increment it, and write back the same result,
         * effectively losing one increment.
         */
        counter++;
    }
    printf("[Thread %d] finished %d increments\n", tid, increments_per_thread);
    return NULL;
}

int main(int argc, char *argv[])
{
    int nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;
    increments_per_thread = (argc > 2) ? atoi(argv[2]) : DEFAULT_INCREMENTS;

    if (nthreads <= 0 || increments_per_thread <= 0) {
        fprintf(stderr, "Usage: %s [num_threads] [increments_per_thread]\n", argv[0]);
        return 1;
    }

    int expected = nthreads * increments_per_thread;

    printf("=== Race Condition Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    for (int i = 0; i < nthreads; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, increment, &tids[i]);
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nExpected: %d\n", expected);
    printf("Actual:   %d\n", counter);

    if (counter != expected) {
        printf("MISMATCH! Lost %d increments due to race condition.\n",
               expected - counter);
    } else {
        printf("No race detected this run. Try increasing increments or threads.\n");
    }

    free(threads);
    free(tids);
    return 0;
}

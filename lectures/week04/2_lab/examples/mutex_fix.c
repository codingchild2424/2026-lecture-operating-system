/*
 * mutex_fix.c - Race Condition Fixed with Mutex
 *
 * Same shared counter scenario as race_demo.c,
 * but protected with a pthread_mutex.
 * The final result will always be correct.
 *
 * Compile: gcc -Wall -pthread -o mutex_fix mutex_fix.c
 * Run:     ./mutex_fix [num_threads] [increments_per_thread]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define DEFAULT_THREADS     4
#define DEFAULT_INCREMENTS  1000000

/* Shared counter - protected by mutex */
int counter = 0;

/* Mutex protecting the counter */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int increments_per_thread;

void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        /*
         * pthread_mutex_lock ensures only ONE thread
         * at a time can execute the code between
         * lock and unlock (the critical section).
         */
        pthread_mutex_lock(&lock);
        counter++;  /* Critical section: safe now */
        pthread_mutex_unlock(&lock);
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

    printf("=== Mutex Fix Demo ===\n");
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

    if (counter == expected) {
        printf("SUCCESS! Mutex prevented the race condition.\n");
    } else {
        printf("ERROR: This should not happen with correct mutex usage.\n");
    }

    pthread_mutex_destroy(&lock);
    free(threads);
    free(tids);
    return 0;
}

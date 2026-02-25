/*
 * deadlock_demo.c - Deadlock Demonstration
 *
 * Two threads acquire two locks in opposite order,
 * causing a classic deadlock (circular wait).
 *
 * Thread A: lock1 -> lock2
 * Thread B: lock2 -> lock1
 *
 * Compile: gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
 * Run:     ./deadlock_demo
 *
 * NOTE: The program will hang (deadlock). Use Ctrl+C to kill it.
 *       Deadlock is probabilistic; if it doesn't occur, run again.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

/*
 * Thread A acquires lock1 first, then lock2.
 */
void *thread_a(void *arg)
{
    (void)arg;

    printf("[Thread A] Trying to acquire lock1...\n");
    pthread_mutex_lock(&lock1);
    printf("[Thread A] Acquired lock1.\n");

    /*
     * Small sleep to increase the chance of deadlock.
     * Without this, one thread might finish before the other starts,
     * avoiding deadlock by pure timing luck.
     */
    usleep(100000);  /* 100ms */

    printf("[Thread A] Trying to acquire lock2...\n");
    pthread_mutex_lock(&lock2);
    printf("[Thread A] Acquired lock2.\n");

    /* Critical section */
    printf("[Thread A] In critical section with both locks.\n");

    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);
    printf("[Thread A] Released both locks.\n");

    return NULL;
}

/*
 * Thread B acquires lock2 first, then lock1.
 * Opposite order from Thread A -> deadlock!
 */
void *thread_b(void *arg)
{
    (void)arg;

    printf("[Thread B] Trying to acquire lock2...\n");
    pthread_mutex_lock(&lock2);
    printf("[Thread B] Acquired lock2.\n");

    usleep(100000);  /* 100ms */

    printf("[Thread B] Trying to acquire lock1...\n");
    pthread_mutex_lock(&lock1);
    printf("[Thread B] Acquired lock1.\n");

    /* Critical section */
    printf("[Thread B] In critical section with both locks.\n");

    pthread_mutex_unlock(&lock1);
    pthread_mutex_unlock(&lock2);
    printf("[Thread B] Released both locks.\n");

    return NULL;
}

int main(void)
{
    printf("=== Deadlock Demo ===\n");
    printf("Thread A: lock1 -> lock2\n");
    printf("Thread B: lock2 -> lock1\n");
    printf("If deadlock occurs, the program will hang. Use Ctrl+C to exit.\n\n");

    pthread_t ta, tb;

    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    printf("\nNo deadlock occurred this time!\n");
    printf("Tip: Run multiple times to observe the deadlock.\n");

    pthread_mutex_destroy(&lock1);
    pthread_mutex_destroy(&lock2);
    return 0;
}

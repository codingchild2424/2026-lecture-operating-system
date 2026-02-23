/*
 * spinlock_impl.c - Simple Spinlock Implementation
 *
 * Implements a spinlock using __sync_lock_test_and_set,
 * the same atomic primitive used in xv6's acquire() function.
 *
 * xv6 kernel/spinlock.c:
 *   while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
 *     ;
 *
 * Compile: gcc -Wall -pthread -o spinlock_impl spinlock_impl.c
 * Run:     ./spinlock_impl [num_threads] [increments_per_thread]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define DEFAULT_THREADS     4
#define DEFAULT_INCREMENTS  1000000

/* ---- Spinlock implementation ---- */

struct spinlock {
    volatile int locked;  /* 0 = unlocked, 1 = locked */
};

/*
 * Initialize the spinlock.
 */
void spinlock_init(struct spinlock *lk)
{
    lk->locked = 0;
}

/*
 * Acquire the spinlock.
 *
 * __sync_lock_test_and_set atomically:
 *   1. Reads the old value of lk->locked
 *   2. Sets lk->locked to 1
 *   3. Returns the old value
 *
 * If the old value was 0, we acquired the lock (it was free).
 * If the old value was 1, someone else holds it; keep spinning.
 *
 * Compare with xv6 acquire() in kernel/spinlock.c
 */
void spinlock_acquire(struct spinlock *lk)
{
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        /* Spin: busy-wait until the lock is released.
         *
         * In a real OS kernel (like xv6), interrupts would be
         * disabled before spinning. Here we just spin in userspace.
         */
    }
    /*
     * Memory barrier: ensure all subsequent reads/writes
     * happen after the lock is acquired.
     * __sync_lock_test_and_set already implies an acquire barrier,
     * but __sync_synchronize makes it explicit.
     */
    __sync_synchronize();
}

/*
 * Release the spinlock.
 *
 * __sync_lock_release atomically sets lk->locked to 0
 * with a release memory barrier.
 *
 * Compare with xv6 release() in kernel/spinlock.c
 */
void spinlock_release(struct spinlock *lk)
{
    __sync_synchronize();
    __sync_lock_release(&lk->locked);
}

/* ---- Demo using our spinlock ---- */

int counter = 0;
struct spinlock lock;
int increments_per_thread;

void *increment(void *arg)
{
    int tid = *(int *)arg;
    for (int i = 0; i < increments_per_thread; i++) {
        spinlock_acquire(&lock);
        counter++;
        spinlock_release(&lock);
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

    printf("=== Spinlock Implementation Demo ===\n");
    printf("Threads: %d, Increments per thread: %d\n", nthreads, increments_per_thread);
    printf("Expected final counter: %d\n\n", expected);

    spinlock_init(&lock);

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
        printf("SUCCESS! Our spinlock works correctly.\n");
    } else {
        printf("ERROR: Spinlock implementation has a bug.\n");
    }

    free(threads);
    free(tids);
    return 0;
}

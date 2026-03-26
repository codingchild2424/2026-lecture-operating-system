/*
 * mergesort_solution.c - Fork-Join Parallel Merge Sort (Solution)
 *
 * Compile: gcc -Wall -pthread -O2 -o mergesort mergesort_solution.c
 * Run:     ./mergesort [array_size] [max_depth]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define DEFAULT_SIZE  10000000
#define DEFAULT_DEPTH 3

struct sort_arg {
    int *array;
    int *temp;
    int left;
    int right;
    int depth;
};

void merge(int *array, int *temp, int left, int mid, int right)
{
    memcpy(temp + left, array + left, (right - left + 1) * sizeof(int));

    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) {
        if (temp[i] <= temp[j])
            array[k++] = temp[i++];
        else
            array[k++] = temp[j++];
    }
    while (i <= mid)
        array[k++] = temp[i++];
    while (j <= right)
        array[k++] = temp[j++];
}

void merge_sort_sequential(int *array, int *temp, int left, int right)
{
    if (left >= right)
        return;

    int mid = left + (right - left) / 2;
    merge_sort_sequential(array, temp, left, mid);
    merge_sort_sequential(array, temp, mid + 1, right);
    merge(array, temp, left, mid, right);
}

int max_depth;

void *merge_sort_parallel(void *arg)
{
    struct sort_arg *sa = (struct sort_arg *)arg;
    int left  = sa->left;
    int right = sa->right;
    int depth = sa->depth;

    if (left >= right)
        return NULL;

    /* Fall back to sequential when depth threshold reached */
    if (depth >= max_depth) {
        merge_sort_sequential(sa->array, sa->temp, left, right);
        return NULL;
    }

    int mid = left + (right - left) / 2;

    /* FORK: create a thread for the left half */
    struct sort_arg left_arg = { sa->array, sa->temp, left, mid, depth + 1 };
    pthread_t tid;
    pthread_create(&tid, NULL, merge_sort_parallel, &left_arg);

    /* Sort the right half in the current thread */
    struct sort_arg right_arg = { sa->array, sa->temp, mid + 1, right, depth + 1 };
    merge_sort_parallel(&right_arg);

    /* JOIN: wait for the left-half thread */
    pthread_join(tid, NULL);

    /* MERGE: combine the two sorted halves */
    merge(sa->array, sa->temp, left, mid, right);

    return NULL;
}

double get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int is_sorted(int *array, int n)
{
    for (int i = 1; i < n; i++)
        if (array[i] < array[i - 1])
            return 0;
    return 1;
}

int main(int argc, char *argv[])
{
    int n = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    max_depth = (argc > 2) ? atoi(argv[2]) : DEFAULT_DEPTH;

    printf("=== Fork-Join Parallel Merge Sort ===\n");
    printf("Array size: %d, Max thread depth: %d (up to %d threads)\n\n",
           n, max_depth, 1 << max_depth);

    int *original = malloc(n * sizeof(int));
    int *array    = malloc(n * sizeof(int));
    int *temp     = malloc(n * sizeof(int));

    srand(42);
    for (int i = 0; i < n; i++)
        original[i] = rand() % (n * 10);

    /* Sequential */
    memcpy(array, original, n * sizeof(int));
    double t0 = get_time();
    merge_sort_sequential(array, temp, 0, n - 1);
    double t1 = get_time();
    double seq_time = t1 - t0;
    printf("Sequential: %.4f sec\n", seq_time);

    int *expected = malloc(n * sizeof(int));
    memcpy(expected, array, n * sizeof(int));

    /* Parallel */
    memcpy(array, original, n * sizeof(int));
    struct sort_arg sa = { array, temp, 0, n - 1, 0 };
    t0 = get_time();
    merge_sort_parallel(&sa);
    t1 = get_time();
    double par_time = t1 - t0;
    printf("Parallel:   %.4f sec\n", par_time);
    printf("Speedup:    %.2fx\n", seq_time / par_time);

    int correct = (memcmp(array, expected, n * sizeof(int)) == 0);
    printf("Verified:   %s\n", correct ? "CORRECT" : "MISMATCH!");

    free(original);
    free(array);
    free(temp);
    free(expected);
    return correct ? 0 : 1;
}

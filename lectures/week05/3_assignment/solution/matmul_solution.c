/*
 * matmul_solution.c - OpenMP Matrix Multiplication (Solution)
 *
 * Compile: gcc -Wall -fopenmp -O2 -o matmul matmul_solution.c
 * Run:     ./matmul [matrix_size]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define DEFAULT_N 1024

double *A, *B, *C;
int N;

#define IDX(i, j) ((i) * N + (j))

void init_matrices(void)
{
    srand(42);
    for (int i = 0; i < N * N; i++) {
        A[i] = (double)(rand() % 100) / 10.0;
        B[i] = (double)(rand() % 100) / 10.0;
    }
}

void zero_C(void)
{
    memset(C, 0, N * N * sizeof(double));
}

void matmul_sequential(void)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[IDX(i, j)] += A[IDX(i, k)] * B[IDX(k, j)];
}

void matmul_openmp_basic(void)
{
    #pragma omp parallel for
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[IDX(i, j)] += A[IDX(i, k)] * B[IDX(k, j)];
}

void matmul_openmp_optimized(void)
{
    #pragma omp parallel for
    for (int i = 0; i < N; i++)
        for (int k = 0; k < N; k++)
            for (int j = 0; j < N; j++)
                C[IDX(i, j)] += A[IDX(i, k)] * B[IDX(k, j)];
}

int verify(double *ref, double *test, int size)
{
    for (int i = 0; i < size; i++) {
        double diff = ref[i] - test[i];
        if (diff > 1e-6 || diff < -1e-6)
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    N = (argc > 1) ? atoi(argv[1]) : DEFAULT_N;

    printf("=== OpenMP Matrix Multiplication ===\n");
    printf("Matrix size: %d x %d\n", N, N);
    printf("Threads: %d\n\n", omp_get_max_threads());

    A = malloc(N * N * sizeof(double));
    B = malloc(N * N * sizeof(double));
    C = malloc(N * N * sizeof(double));
    double *C_ref = malloc(N * N * sizeof(double));

    init_matrices();
    double t0, t1;

    zero_C();
    t0 = omp_get_wtime();
    matmul_sequential();
    t1 = omp_get_wtime();
    double seq_time = t1 - t0;
    printf("Sequential (i-j-k):       %.4f sec\n", seq_time);
    memcpy(C_ref, C, N * N * sizeof(double));

    zero_C();
    t0 = omp_get_wtime();
    matmul_openmp_basic();
    t1 = omp_get_wtime();
    double omp_basic_time = t1 - t0;
    int ok1 = verify(C_ref, C, N * N);
    printf("OpenMP basic:             %.4f sec  (%.2fx) %s\n",
           omp_basic_time, seq_time / omp_basic_time,
           ok1 ? "" : "MISMATCH!");

    zero_C();
    t0 = omp_get_wtime();
    matmul_openmp_optimized();
    t1 = omp_get_wtime();
    double omp_opt_time = t1 - t0;
    int ok2 = verify(C_ref, C, N * N);
    printf("OpenMP optimized (i-k-j): %.4f sec  (%.2fx) %s\n",
           omp_opt_time, seq_time / omp_opt_time,
           ok2 ? "" : "MISMATCH!");

    printf("\nVerified: %s\n", (ok1 && ok2) ? "CORRECT" : "ERRORS FOUND");

    free(A);
    free(B);
    free(C);
    free(C_ref);
    return (ok1 && ok2) ? 0 : 1;
}

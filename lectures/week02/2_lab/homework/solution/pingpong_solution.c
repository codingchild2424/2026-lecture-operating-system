/*
 * pingpong_solution.c - pingpong 과제 모범 답안 (교수용)
 *
 * Compile: gcc -Wall -o pingpong pingpong_solution.c
 * Run:     ./pingpong
 *
 * Expected output format:
 *   <pid>: received ping
 *   <pid>: received pong
 *   Round-trip time: X.XXX us
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

/* Get current time in microseconds */
static long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

int main(void)
{
    int parent_to_child[2];  /* parent writes, child reads */
    int child_to_parent[2];  /* child writes, parent reads */

    if (pipe(parent_to_child) < 0) {
        perror("pipe");
        exit(1);
    }
    if (pipe(child_to_parent) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        /* Child process */
        close(parent_to_child[1]);  /* close write end (child only reads here) */
        close(child_to_parent[0]);  /* close read end (child only writes here) */

        char buf;
        if (read(parent_to_child[0], &buf, 1) != 1) {
            perror("child read");
            exit(1);
        }
        printf("%d: received ping\n", getpid());

        if (write(child_to_parent[1], &buf, 1) != 1) {
            perror("child write");
            exit(1);
        }

        close(parent_to_child[0]);
        close(child_to_parent[1]);
        exit(0);
    } else {
        /* Parent process */
        close(parent_to_child[0]);  /* close read end (parent only writes here) */
        close(child_to_parent[1]);  /* close write end (parent only reads here) */

        long start_time = get_time_us();

        char buf = 'p';
        if (write(parent_to_child[1], &buf, 1) != 1) {
            perror("parent write");
            exit(1);
        }

        if (read(child_to_parent[0], &buf, 1) != 1) {
            perror("parent read");
            exit(1);
        }
        printf("%d: received pong\n", getpid());

        long end_time = get_time_us();
        double elapsed = (double)(end_time - start_time);
        printf("Round-trip time: %.3f us\n", elapsed);

        close(parent_to_child[1]);
        close(child_to_parent[0]);
        wait(NULL);
    }

    return 0;
}

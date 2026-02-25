/*
 * fork_basic.c - fork()와 wait()의 기본 동작을 보여주는 예제
 *
 * Compile: gcc -Wall -o fork_basic fork_basic.c
 * Run:     ./fork_basic
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    printf("=== fork() + wait() 기본 예제 ===\n\n");

    /*
     * fork()는 현재 프로세스를 복제하여 자식 프로세스를 만든다.
     * - 부모 프로세스에서는 자식의 PID를 반환
     * - 자식 프로세스에서는 0을 반환
     * - 실패하면 -1을 반환
     */
    printf("[부모 PID=%d] fork() 호출 전\n", getpid());

    pid_t pid = fork();

    if (pid < 0) {
        /* fork 실패 */
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        /* 자식 프로세스 */
        printf("[자식 PID=%d] 나는 자식 프로세스! 부모 PID=%d\n",
               getpid(), getppid());
        printf("[자식 PID=%d] 작업 수행 중...\n", getpid());
        sleep(1);  /* 1초 대기 (작업을 시뮬레이션) */
        printf("[자식 PID=%d] 작업 완료, exit(42)로 종료\n", getpid());
        exit(42);
    } else {
        /* 부모 프로세스 */
        printf("[부모 PID=%d] fork() 성공! 자식 PID=%d\n", getpid(), pid);

        /*
         * wait()는 자식 프로세스가 종료될 때까지 부모를 대기시킨다.
         * status에 자식의 종료 상태가 저장된다.
         */
        int status;
        pid_t terminated = wait(&status);

        if (terminated < 0) {
            perror("wait");
            exit(1);
        }

        printf("[부모 PID=%d] 자식(PID=%d)이 종료됨\n", getpid(), terminated);

        if (WIFEXITED(status)) {
            printf("[부모 PID=%d] 자식의 종료 코드: %d\n",
                   getpid(), WEXITSTATUS(status));
        }
    }

    printf("\n=== 추가 실험: fork()로 여러 자식 만들기 ===\n\n");

    int num_children = 3;
    for (int i = 0; i < num_children; i++) {
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            exit(1);
        } else if (child == 0) {
            /* 각 자식은 서로 다른 시간만큼 대기 */
            printf("  자식 #%d (PID=%d) 시작\n", i, getpid());
            usleep((unsigned int)(100000 * (num_children - i)));  /* 역순으로 종료 */
            printf("  자식 #%d (PID=%d) 종료\n", i, getpid());
            exit(i);
        }
    }

    /* 부모: 모든 자식이 종료될 때까지 대기 */
    for (int i = 0; i < num_children; i++) {
        int status;
        pid_t done = wait(&status);
        if (WIFEXITED(status)) {
            printf("[부모] 자식 PID=%d 종료 (exit code=%d)\n",
                   done, WEXITSTATUS(status));
        }
    }

    printf("\n주목: 자식들의 종료 순서가 생성 순서와 다를 수 있습니다!\n");

    return 0;
}

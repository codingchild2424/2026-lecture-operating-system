/*
 * deadlock_fix_ordering.c - Lock ordering으로 Deadlock 해결
 *
 * 모든 스레드가 동일한 순서(A -> B)로 lock을 잡도록 하여 deadlock을 방지합니다.
 * 이것은 xv6 커널이 사용하는 것과 동일한 전략입니다.
 *
 * 컴파일:
 *   gcc -Wall -pthread -o deadlock_fix_ordering deadlock_fix_ordering.c
 *
 * 실행:
 *   ./deadlock_fix_ordering
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: 항상 mutex_A -> mutex_B 순서로 잠금
 */
void *thread1_func(void *arg)
{
    (void)arg;

    for (int i = 0; i < 5; i++) {
        printf("[Thread 1] 반복 %d: mutex_A 잠금 시도...\n", i + 1);
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 1] 반복 %d: mutex_A 잠금 성공\n", i + 1);

        /* 경쟁 조건을 유도하기 위한 짧은 대기 */
        usleep(10000);  /* 10ms */

        printf("[Thread 1] 반복 %d: mutex_B 잠금 시도...\n", i + 1);
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 1] 반복 %d: mutex_B 잠금 성공\n", i + 1);

        /* 임계 영역 */
        printf("[Thread 1] 반복 %d: === 임계 영역 진입 (A, B 모두 보유) ===\n", i + 1);
        usleep(5000);  /* 작업 시뮬레이션 */

        pthread_mutex_unlock(&mutex_B);
        printf("[Thread 1] 반복 %d: mutex_B 해제\n", i + 1);
        pthread_mutex_unlock(&mutex_A);
        printf("[Thread 1] 반복 %d: mutex_A 해제\n", i + 1);
        printf("\n");
    }

    return NULL;
}

/*
 * thread2_func: 역시 mutex_A -> mutex_B 순서로 잠금 (thread1과 동일한 순서!)
 *
 * 핵심: 원래는 mutex_B -> mutex_A 순서였지만,
 *       lock ordering 규칙에 따라 mutex_A -> mutex_B로 변경함.
 */
void *thread2_func(void *arg)
{
    (void)arg;

    for (int i = 0; i < 5; i++) {
        /*
         * [수정 전 - deadlock 발생]
         *   pthread_mutex_lock(&mutex_B);  // B를 먼저 잠금
         *   pthread_mutex_lock(&mutex_A);  // A를 나중에 잠금
         *
         * [수정 후 - lock ordering 적용]
         *   pthread_mutex_lock(&mutex_A);  // A를 먼저 잠금 (순서 통일!)
         *   pthread_mutex_lock(&mutex_B);  // B를 나중에 잠금
         */
        printf("[Thread 2] 반복 %d: mutex_A 잠금 시도...\n", i + 1);
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 2] 반복 %d: mutex_A 잠금 성공\n", i + 1);

        usleep(10000);  /* 10ms */

        printf("[Thread 2] 반복 %d: mutex_B 잠금 시도...\n", i + 1);
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 2] 반복 %d: mutex_B 잠금 성공\n", i + 1);

        /* 임계 영역 */
        printf("[Thread 2] 반복 %d: === 임계 영역 진입 (A, B 모두 보유) ===\n", i + 1);
        usleep(5000);  /* 작업 시뮬레이션 */

        pthread_mutex_unlock(&mutex_B);
        printf("[Thread 2] 반복 %d: mutex_B 해제\n", i + 1);
        pthread_mutex_unlock(&mutex_A);
        printf("[Thread 2] 반복 %d: mutex_A 해제\n", i + 1);
        printf("\n");
    }

    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    printf("==============================================\n");
    printf("  Lock Ordering으로 Deadlock 해결 데모\n");
    printf("==============================================\n");
    printf("\n");
    printf("해결 방법: 모든 스레드가 동일한 순서로 lock 획득\n");
    printf("  Lock 순서 규칙: 항상 mutex_A -> mutex_B\n");
    printf("\n");
    printf("이것은 xv6 커널에서 사용하는 방식과 동일합니다.\n");
    printf("예: wait_lock은 항상 p->lock보다 먼저 획득\n");
    printf("\n");
    printf("----------------------------------------------\n\n");

    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("----------------------------------------------\n");
    printf("두 스레드 모두 정상 종료! Deadlock이 발생하지 않았습니다.\n");
    printf("\n");
    printf("[핵심 원리]\n");
    printf("  모든 스레드가 동일한 순서(A -> B)로 lock을 획득하면\n");
    printf("  circular wait 조건이 깨져서 deadlock이 발생하지 않습니다.\n");

    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}

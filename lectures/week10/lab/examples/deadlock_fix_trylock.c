/*
 * deadlock_fix_trylock.c - pthread_mutex_trylock으로 Deadlock 회피
 *
 * trylock을 사용하여 lock 획득에 실패하면 보유 중인 lock을 해제하고
 * 다시 시도하는 back-off 전략으로 deadlock을 회피합니다.
 *
 * 컴파일:
 *   gcc -Wall -pthread -o deadlock_fix_trylock deadlock_fix_trylock.c
 *
 * 실행:
 *   ./deadlock_fix_trylock
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: mutex_A를 먼저 잡고, mutex_B를 trylock으로 시도
 *               실패하면 mutex_A를 해제하고 다시 시도 (back-off)
 */
void *thread1_func(void *arg)
{
    (void)arg;
    int success = 0;
    int attempts = 0;

    while (!success) {
        attempts++;

        printf("[Thread 1] 시도 %d: mutex_A 잠금...\n", attempts);
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 1] 시도 %d: mutex_A 잠금 성공\n", attempts);

        usleep(10000);  /* 10ms - 경쟁 상황 유도 */

        printf("[Thread 1] 시도 %d: mutex_B trylock 시도...\n", attempts);
        if (pthread_mutex_trylock(&mutex_B) == 0) {
            /* trylock 성공 */
            printf("[Thread 1] 시도 %d: mutex_B trylock 성공!\n", attempts);
            success = 1;

            /* 임계 영역 */
            printf("[Thread 1] === 임계 영역 진입 (A, B 모두 보유) ===\n");
            usleep(5000);

            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 1] mutex_B 해제\n");
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 1] mutex_A 해제\n");
        } else {
            /* trylock 실패 -> back-off: 보유 중인 lock 해제 후 재시도 */
            printf("[Thread 1] 시도 %d: mutex_B trylock 실패! -> back-off\n", attempts);
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 1] 시도 %d: mutex_A 해제 (back-off)\n", attempts);

            /* 약간의 랜덤 대기 후 재시도 (경쟁 완화) */
            usleep((unsigned)(rand() % 50000));  /* 0~50ms */
        }
    }

    printf("[Thread 1] 완료 (총 %d회 시도)\n\n", attempts);
    return NULL;
}

/*
 * thread2_func: mutex_B를 먼저 잡고, mutex_A를 trylock으로 시도
 *               (thread1과 반대 순서이지만 trylock으로 deadlock 회피)
 *               실패하면 mutex_B를 해제하고 다시 시도 (back-off)
 */
void *thread2_func(void *arg)
{
    (void)arg;
    int success = 0;
    int attempts = 0;

    while (!success) {
        attempts++;

        printf("[Thread 2] 시도 %d: mutex_B 잠금...\n", attempts);
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 2] 시도 %d: mutex_B 잠금 성공\n", attempts);

        usleep(10000);  /* 10ms - 경쟁 상황 유도 */

        printf("[Thread 2] 시도 %d: mutex_A trylock 시도...\n", attempts);
        if (pthread_mutex_trylock(&mutex_A) == 0) {
            /* trylock 성공 */
            printf("[Thread 2] 시도 %d: mutex_A trylock 성공!\n", attempts);
            success = 1;

            /* 임계 영역 */
            printf("[Thread 2] === 임계 영역 진입 (A, B 모두 보유) ===\n");
            usleep(5000);

            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 2] mutex_A 해제\n");
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 2] mutex_B 해제\n");
        } else {
            /* trylock 실패 -> back-off: 보유 중인 lock 해제 후 재시도 */
            printf("[Thread 2] 시도 %d: mutex_A trylock 실패! -> back-off\n", attempts);
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 2] 시도 %d: mutex_B 해제 (back-off)\n", attempts);

            /* 약간의 랜덤 대기 후 재시도 (경쟁 완화) */
            usleep((unsigned)(rand() % 50000));  /* 0~50ms */
        }
    }

    printf("[Thread 2] 완료 (총 %d회 시도)\n\n", attempts);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    /* 랜덤 시드 초기화 (back-off 대기 시간용) */
    srand(42);

    printf("==============================================\n");
    printf("  trylock + Back-off으로 Deadlock 회피 데모\n");
    printf("==============================================\n");
    printf("\n");
    printf("Thread 1: mutex_A -> trylock(mutex_B)\n");
    printf("Thread 2: mutex_B -> trylock(mutex_A)\n");
    printf("\n");
    printf("두 스레드가 반대 순서로 lock을 시도하지만,\n");
    printf("trylock이 실패하면 보유 중인 lock을 해제하고\n");
    printf("잠시 후 다시 시도합니다 (back-off 전략).\n");
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
    printf("  trylock은 lock을 즉시 획득하지 못하면 차단하지 않고 실패를 반환합니다.\n");
    printf("  실패 시 이미 보유한 lock을 해제(back-off)하면\n");
    printf("  Hold & Wait 조건이 깨져서 deadlock이 방지됩니다.\n");
    printf("\n");
    printf("[주의]\n");
    printf("  trylock + back-off 방식은 livelock(계속 재시도만 반복)이\n");
    printf("  발생할 수 있으므로, 랜덤 대기 시간을 두는 것이 중요합니다.\n");

    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}

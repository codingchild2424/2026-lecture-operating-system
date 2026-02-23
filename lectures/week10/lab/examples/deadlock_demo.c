/*
 * deadlock_demo.c - Deadlock 발생 데모 프로그램
 *
 * 두 스레드가 두 mutex를 반대 순서로 잡아 deadlock이 발생하는 것을 보여줍니다.
 * 이 프로그램은 실행하면 멈춥니다 (deadlock). Ctrl+C로 종료하세요.
 *
 * 컴파일:
 *   gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
 *
 * 실행:
 *   ./deadlock_demo
 *   (프로그램이 멈추면 Ctrl+C로 종료)
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: mutex_A를 먼저 잡고, 그 다음 mutex_B를 잡으려고 시도
 */
void *thread1_func(void *arg)
{
    (void)arg;

    printf("[Thread 1] mutex_A 잠금 시도...\n");
    pthread_mutex_lock(&mutex_A);
    printf("[Thread 1] mutex_A 잠금 성공!\n");

    /* 다른 스레드가 mutex_B를 먼저 잡을 시간을 줌 */
    printf("[Thread 1] 0.1초 대기 (다른 스레드가 mutex_B를 잡도록)...\n");
    usleep(100000);  /* 100ms */

    printf("[Thread 1] mutex_B 잠금 시도... (여기서 멈출 수 있음)\n");
    pthread_mutex_lock(&mutex_B);
    printf("[Thread 1] mutex_B 잠금 성공!\n");

    /* 임계 영역 (critical section) */
    printf("[Thread 1] 두 lock 모두 획득 - 작업 수행 중\n");

    pthread_mutex_unlock(&mutex_B);
    pthread_mutex_unlock(&mutex_A);
    printf("[Thread 1] 완료\n");

    return NULL;
}

/*
 * thread2_func: mutex_B를 먼저 잡고, 그 다음 mutex_A를 잡으려고 시도
 *               (thread1과 반대 순서 -> deadlock 발생!)
 */
void *thread2_func(void *arg)
{
    (void)arg;

    printf("[Thread 2] mutex_B 잠금 시도...\n");
    pthread_mutex_lock(&mutex_B);
    printf("[Thread 2] mutex_B 잠금 성공!\n");

    /* 다른 스레드가 mutex_A를 먼저 잡을 시간을 줌 */
    printf("[Thread 2] 0.1초 대기 (다른 스레드가 mutex_A를 잡도록)...\n");
    usleep(100000);  /* 100ms */

    printf("[Thread 2] mutex_A 잠금 시도... (여기서 멈출 수 있음)\n");
    pthread_mutex_lock(&mutex_A);
    printf("[Thread 2] mutex_A 잠금 성공!\n");

    /* 임계 영역 (critical section) */
    printf("[Thread 2] 두 lock 모두 획득 - 작업 수행 중\n");

    pthread_mutex_unlock(&mutex_A);
    pthread_mutex_unlock(&mutex_B);
    printf("[Thread 2] 완료\n");

    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    printf("==============================================\n");
    printf("  Deadlock 발생 데모\n");
    printf("==============================================\n");
    printf("\n");
    printf("Thread 1: mutex_A -> mutex_B 순서로 잠금\n");
    printf("Thread 2: mutex_B -> mutex_A 순서로 잠금 (반대!)\n");
    printf("\n");
    printf("Deadlock이 발생하면 프로그램이 멈춥니다.\n");
    printf("Ctrl+C로 종료하세요.\n");
    printf("\n");
    printf("----------------------------------------------\n");

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

    /* 이 메시지는 deadlock 발생 시 출력되지 않음 */
    printf("\n----------------------------------------------\n");
    printf("두 스레드 모두 정상 종료 (deadlock 미발생)\n");

    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}

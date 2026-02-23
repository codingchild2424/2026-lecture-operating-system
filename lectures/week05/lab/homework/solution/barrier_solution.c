/*
 * barrier_solution.c - Barrier 구현 솔루션 (보너스 과제)
 *
 * pthread_barrier를 mutex + condition variable로 직접 구현
 * Barrier: 모든 스레드가 도착할 때까지 대기, 모두 도착하면 한꺼번에 진행
 *
 * 컴파일: gcc -Wall -pthread -o barrier_solution barrier_solution.c
 * 실행:   ./barrier_solution
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* ============================================================
 * Barrier 구조체
 * ============================================================ */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             count;       // 현재 barrier에 도착한 스레드 수
    int             num_threads; // barrier를 통과하는 데 필요한 스레드 수
    int             round;       // 현재 라운드 (재사용 가능한 barrier를 위해)
} my_barrier_t;

/* ============================================================
 * barrier_init
 * ============================================================ */
void barrier_init(my_barrier_t *b, int num_threads) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->count = 0;
    b->num_threads = num_threads;
    b->round = 0;
}

/* ============================================================
 * barrier_wait
 *
 * 핵심 아이디어: round 변수로 "세대(generation)"를 구분한다.
 *
 * 문제 상황 (round 없이):
 *   1. 스레드 A, B, C가 barrier에 도착 (count=3)
 *   2. 마지막 스레드 C가 count=0으로 리셋하고 broadcast
 *   3. A가 깨어나서 barrier를 빠져나감
 *   4. A가 빠르게 다음 barrier_wait에 도착 (count=1)
 *   5. B가 이제야 깨어남 -> count가 1인데 이것이 이전 라운드인지
 *      다음 라운드인지 구분할 수 없음!
 *
 * 해결: round 번호를 저장하고, 자신의 라운드에서만 대기
 * ============================================================ */
void barrier_wait(my_barrier_t *b) {
    pthread_mutex_lock(&b->mutex);

    int my_round = b->round;    // 현재 라운드 기억

    b->count++;

    if (b->count == b->num_threads) {
        // 마지막 스레드: 모두를 깨우고 다음 라운드 준비
        b->count = 0;
        b->round++;             // 라운드 전환
        pthread_cond_broadcast(&b->cond);
    } else {
        // 아직 모두 도착하지 않음: 라운드가 바뀔 때까지 대기
        while (b->round == my_round) {
            pthread_cond_wait(&b->cond, &b->mutex);
        }
    }

    pthread_mutex_unlock(&b->mutex);
}

/* ============================================================
 * barrier_destroy
 * ============================================================ */
void barrier_destroy(my_barrier_t *b) {
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->cond);
}

/* ============================================================
 * 테스트 코드
 * ============================================================ */
#define NUM_THREADS 5
#define NUM_ROUNDS  3

static my_barrier_t barrier;
static int shared_counter = 0;
static pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    int id = *(int *)arg;

    for (int round = 0; round < NUM_ROUNDS; round++) {
        /* Phase 1: 각 스레드가 작업 수행 */
        printf("  스레드 %d: 라운드 %d 작업 시작\n", id, round);
        usleep((unsigned)(rand() % 100000));

        pthread_mutex_lock(&counter_lock);
        shared_counter++;
        pthread_mutex_unlock(&counter_lock);

        /* Barrier: 모든 스레드가 Phase 1을 완료할 때까지 대기 */
        barrier_wait(&barrier);

        /* Phase 2: 검증 - 모든 스레드의 작업이 반영되었는지 확인 */
        if (id == 0) {
            int expected = NUM_THREADS * (round + 1);
            printf("  [검증] 라운드 %d: counter = %d (기대값: %d) %s\n",
                   round, shared_counter, expected,
                   shared_counter == expected ? "OK" : "FAIL");
            assert(shared_counter == expected);
        }

        /* 다음 라운드 전에 모든 스레드가 검증을 마칠 때까지 대기 */
        barrier_wait(&barrier);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int       ids[NUM_THREADS];

    printf("=== Barrier 테스트 ===\n");
    printf("스레드 수: %d, 라운드 수: %d\n\n", NUM_THREADS, NUM_ROUNDS);

    barrier_init(&barrier, NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    barrier_destroy(&barrier);

    printf("\n=== 모든 테스트 통과 ===\n");
    return 0;
}

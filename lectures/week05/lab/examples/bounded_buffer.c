/*
 * bounded_buffer.c
 *
 * 다수의 Producer/Consumer 스레드를 사용하는 Bounded Buffer 구현
 * 올바른 동기화를 통해 race condition을 방지
 *
 * 컴파일: gcc -Wall -pthread -o bounded_buffer bounded_buffer.c
 * 실행:   ./bounded_buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

/* ============================================================
 * 설정값
 * ============================================================ */
#define BUFFER_SIZE     4       // 의도적으로 작은 버퍼 (동기화 이슈 관찰)
#define NUM_PRODUCERS   3       // Producer 스레드 수
#define NUM_CONSUMERS   3       // Consumer 스레드 수
#define ITEMS_PER_PROD  10      // 각 Producer가 생산할 아이템 수

/* ============================================================
 * Bounded Buffer 구조체
 * ============================================================ */
typedef struct {
    int     data[BUFFER_SIZE];
    int     in;
    int     out;
    int     count;

    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;

    /* 통계 */
    int     total_produced;
    int     total_consumed;
    int     done;               // 모든 생산 완료 플래그
} bbuf_t;

static bbuf_t bbuf;

/* ============================================================
 * 초기화 / 정리
 * ============================================================ */
void bbuf_init(bbuf_t *b) {
    b->in = 0;
    b->out = 0;
    b->count = 0;
    b->total_produced = 0;
    b->total_consumed = 0;
    b->done = 0;

    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->not_full, NULL);
    pthread_cond_init(&b->not_empty, NULL);
}

void bbuf_destroy(bbuf_t *b) {
    pthread_mutex_destroy(&b->lock);
    pthread_cond_destroy(&b->not_full);
    pthread_cond_destroy(&b->not_empty);
}

/* ============================================================
 * put: 버퍼에 아이템 삽입
 * ============================================================ */
void bbuf_put(bbuf_t *b, int item) {
    pthread_mutex_lock(&b->lock);

    /* 조건 변수 대기 패턴 (Mesa semantics):
     * - while 루프로 조건 재확인 필수
     * - pthread_cond_wait는 atomically:
     *   (1) mutex 해제 + (2) 대기 상태 진입
     * - 깨어나면 자동으로 mutex 재획득 */
    while (b->count == BUFFER_SIZE) {
        pthread_cond_wait(&b->not_full, &b->lock);
    }

    /* 불변식(invariant) 확인: 삽입 전 버퍼가 가득 차 있지 않아야 함 */
    assert(b->count < BUFFER_SIZE);

    b->data[b->in] = item;
    b->in = (b->in + 1) % BUFFER_SIZE;
    b->count++;
    b->total_produced++;

    /* 하나라도 들어왔으므로 대기 중인 consumer 깨움 */
    pthread_cond_signal(&b->not_empty);

    pthread_mutex_unlock(&b->lock);
}

/* ============================================================
 * get: 버퍼에서 아이템 제거
 * 반환값: 아이템 값, 또는 모든 생산이 끝나고 버퍼가 빈 경우 -1
 * ============================================================ */
int bbuf_get(bbuf_t *b) {
    int item;

    pthread_mutex_lock(&b->lock);

    /* 버퍼가 비어 있고 아직 생산이 끝나지 않았으면 대기 */
    while (b->count == 0 && !b->done) {
        pthread_cond_wait(&b->not_empty, &b->lock);
    }

    /* 생산이 모두 끝나고 버퍼도 비었으면 종료 신호 */
    if (b->count == 0 && b->done) {
        pthread_mutex_unlock(&b->lock);
        return -1;
    }

    assert(b->count > 0);

    item = b->data[b->out];
    b->out = (b->out + 1) % BUFFER_SIZE;
    b->count--;
    b->total_consumed++;

    /* 공간이 생겼으므로 대기 중인 producer 깨움 */
    pthread_cond_signal(&b->not_full);

    pthread_mutex_unlock(&b->lock);

    return item;
}

/* ============================================================
 * Producer 스레드
 * ============================================================ */
void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < ITEMS_PER_PROD; i++) {
        int item = id * 100 + i;
        bbuf_put(&bbuf, item);

        printf("  P%d: 생산 [%d] (버퍼 사용량 ~%d/%d)\n",
               id, item, bbuf.count, BUFFER_SIZE);

        /* 생산 속도 시뮬레이션 */
        usleep((unsigned)(rand() % 50000));
    }

    printf("  P%d: === 생산 완료 ===\n", id);
    return NULL;
}

/* ============================================================
 * Consumer 스레드
 * ============================================================ */
void *consumer(void *arg) {
    int id = *(int *)arg;
    int consumed = 0;

    while (1) {
        int item = bbuf_get(&bbuf);
        if (item == -1) {
            break;  // 종료 신호
        }

        consumed++;
        printf("  C%d: 소비 [%d] (버퍼 사용량 ~%d/%d)\n",
               id, item, bbuf.count, BUFFER_SIZE);

        /* 소비 속도 시뮬레이션 */
        usleep((unsigned)(rand() % 80000));
    }

    printf("  C%d: === 소비 완료 (총 %d개) ===\n", id, consumed);
    return NULL;
}

/* ============================================================
 * main
 * ============================================================ */
int main(void) {
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int       prod_ids[NUM_PRODUCERS];
    int       cons_ids[NUM_CONSUMERS];

    int total_items = NUM_PRODUCERS * ITEMS_PER_PROD;

    printf("=== Bounded Buffer: 다중 Producer/Consumer ===\n");
    printf("버퍼 크기: %d\n", BUFFER_SIZE);
    printf("Producer %d개 x %d아이템 = 총 %d아이템\n",
           NUM_PRODUCERS, ITEMS_PER_PROD, total_items);
    printf("Consumer %d개\n\n", NUM_CONSUMERS);

    srand((unsigned)time(NULL));
    bbuf_init(&bbuf);

    /* Producer 스레드 생성 */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i;
        pthread_create(&producers[i], NULL, producer, &prod_ids[i]);
    }

    /* Consumer 스레드 생성 */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cons_ids[i] = i;
        pthread_create(&consumers[i], NULL, consumer, &cons_ids[i]);
    }

    /* 모든 Producer 종료 대기 */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    /* 모든 생산이 끝났음을 알림 */
    pthread_mutex_lock(&bbuf.lock);
    bbuf.done = 1;
    pthread_cond_broadcast(&bbuf.not_empty);  // 모든 대기 중 consumer 깨움
    pthread_mutex_unlock(&bbuf.lock);

    /* 모든 Consumer 종료 대기 */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    /* 결과 검증 */
    printf("\n=== 결과 ===\n");
    printf("총 생산: %d\n", bbuf.total_produced);
    printf("총 소비: %d\n", bbuf.total_consumed);

    if (bbuf.total_produced == total_items &&
        bbuf.total_consumed == total_items) {
        printf("검증 성공: 모든 아이템이 정확히 생산/소비되었습니다.\n");
    } else {
        printf("검증 실패: 아이템 수가 일치하지 않습니다!\n");
        return 1;
    }

    bbuf_destroy(&bbuf);

    printf("\n=== 프로그램 정상 종료 ===\n");
    return 0;
}

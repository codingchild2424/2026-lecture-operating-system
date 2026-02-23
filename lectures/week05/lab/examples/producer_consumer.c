/*
 * producer_consumer.c
 *
 * Bounded buffer를 이용한 Producer-Consumer 문제 구현
 * pthread_mutex와 pthread_cond를 사용한 동기화
 *
 * 컴파일: gcc -Wall -pthread -o producer_consumer producer_consumer.c
 * 실행:   ./producer_consumer
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* ============================================================
 * Bounded Buffer 정의
 * ============================================================ */
#define BUFFER_SIZE 5       // 버퍼 크기
#define NUM_ITEMS   20      // 생산/소비할 아이템 총 수

typedef struct {
    int     data[BUFFER_SIZE];  // 원형 버퍼
    int     in;                 // 다음 삽입 위치
    int     out;                // 다음 제거 위치
    int     count;              // 현재 아이템 수

    pthread_mutex_t mutex;      // 상호 배제
    pthread_cond_t  not_full;   // 버퍼가 가득 차지 않음을 알림
    pthread_cond_t  not_empty;  // 버퍼가 비어있지 않음을 알림
} bounded_buffer_t;

/* 전역 bounded buffer */
bounded_buffer_t buffer;

/* ============================================================
 * 버퍼 초기화
 * ============================================================ */
void buffer_init(bounded_buffer_t *buf) {
    buf->in    = 0;
    buf->out   = 0;
    buf->count = 0;

    pthread_mutex_init(&buf->mutex, NULL);
    pthread_cond_init(&buf->not_full, NULL);
    pthread_cond_init(&buf->not_empty, NULL);
}

/* ============================================================
 * 버퍼 삽입 (producer가 호출)
 *
 * 핵심 패턴:
 *   1. mutex 획득
 *   2. while 조건 확인 (가득 찼는지) -> wait
 *   3. 데이터 삽입
 *   4. signal로 consumer 깨움
 *   5. mutex 해제
 * ============================================================ */
void buffer_put(bounded_buffer_t *buf, int item) {
    pthread_mutex_lock(&buf->mutex);

    /* 버퍼가 가득 차 있으면 대기
     * 주의: if가 아닌 while을 사용해야 함 (spurious wakeup 방지) */
    while (buf->count == BUFFER_SIZE) {
        printf("  [Producer] 버퍼 가득 참, 대기 중...\n");
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }

    /* 원형 버퍼에 데이터 삽입 */
    buf->data[buf->in] = item;
    buf->in = (buf->in + 1) % BUFFER_SIZE;
    buf->count++;

    printf("  [Producer] 생산: %d (버퍼: %d/%d)\n",
           item, buf->count, BUFFER_SIZE);

    /* consumer에게 "버퍼가 비어있지 않다"고 알림 */
    pthread_cond_signal(&buf->not_empty);

    pthread_mutex_unlock(&buf->mutex);
}

/* ============================================================
 * 버퍼에서 제거 (consumer가 호출)
 *
 * 핵심 패턴:
 *   1. mutex 획득
 *   2. while 조건 확인 (비어있는지) -> wait
 *   3. 데이터 제거
 *   4. signal로 producer 깨움
 *   5. mutex 해제
 * ============================================================ */
int buffer_get(bounded_buffer_t *buf) {
    int item;

    pthread_mutex_lock(&buf->mutex);

    /* 버퍼가 비어있으면 대기 */
    while (buf->count == 0) {
        printf("  [Consumer] 버퍼 비어있음, 대기 중...\n");
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }

    /* 원형 버퍼에서 데이터 제거 */
    item = buf->data[buf->out];
    buf->out = (buf->out + 1) % BUFFER_SIZE;
    buf->count--;

    printf("  [Consumer] 소비: %d (버퍼: %d/%d)\n",
           item, buf->count, BUFFER_SIZE);

    /* producer에게 "버퍼가 가득 차지 않았다"고 알림 */
    pthread_cond_signal(&buf->not_full);

    pthread_mutex_unlock(&buf->mutex);

    return item;
}

/* ============================================================
 * 버퍼 정리
 * ============================================================ */
void buffer_destroy(bounded_buffer_t *buf) {
    pthread_mutex_destroy(&buf->mutex);
    pthread_cond_destroy(&buf->not_full);
    pthread_cond_destroy(&buf->not_empty);
}

/* ============================================================
 * Producer 스레드 함수
 * ============================================================ */
void *producer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        int item = id * 1000 + i;   // 고유한 아이템 번호 생성
        buffer_put(&buffer, item);
        usleep(rand() % 100000);     // 0~100ms 랜덤 대기 (생산 시뮬레이션)
    }

    printf("  [Producer %d] 생산 완료\n", id);
    return NULL;
}

/* ============================================================
 * Consumer 스레드 함수
 * ============================================================ */
void *consumer(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < NUM_ITEMS; i++) {
        int item = buffer_get(&buffer);
        (void)item;                  // 사용하지 않는 변수 경고 제거
        usleep(rand() % 150000);     // 0~150ms 랜덤 대기 (소비 시뮬레이션)
    }

    printf("  [Consumer %d] 소비 완료\n", id);
    return NULL;
}

/* ============================================================
 * main
 * ============================================================ */
int main(void) {
    pthread_t prod_thread, cons_thread;
    int prod_id = 1, cons_id = 1;

    printf("=== Producer-Consumer 문제 ===\n");
    printf("버퍼 크기: %d, 생산/소비 아이템 수: %d\n\n", BUFFER_SIZE, NUM_ITEMS);

    buffer_init(&buffer);

    /* 스레드 생성 */
    pthread_create(&prod_thread, NULL, producer, &prod_id);
    pthread_create(&cons_thread, NULL, consumer, &cons_id);

    /* 스레드 종료 대기 */
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    buffer_destroy(&buffer);

    printf("\n=== 프로그램 정상 종료 ===\n");
    return 0;
}

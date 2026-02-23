/*
 * cow_concept.c - Copy-On-Write (COW) 동작을 관찰하는 프로그램
 *
 * COW fork의 핵심 개념을 Linux/macOS에서 시연한다.
 * - 부모가 mmap으로 메모리 영역을 생성하고 데이터를 채운다
 * - fork() 후 부모와 자식이 같은 물리 페이지를 공유한다
 * - 한쪽이 쓰기를 하면 COW로 인해 페이지가 복사된다
 *
 * 컴파일: gcc -Wall -o cow_concept cow_concept.c
 * 실행:   ./cow_concept
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

#define NUM_PAGES    1024       /* 할당할 페이지 수 */
#define PAGE_SIZE    4096       /* 페이지 크기 (일반적인 경우) */
#define REGION_SIZE  (NUM_PAGES * PAGE_SIZE)  /* 총 4MB */

/*
 * get_minor_faults - 현재 프로세스의 minor page fault 횟수를 반환한다.
 *
 * minor page fault: 디스크 I/O 없이 처리되는 page fault.
 * COW로 인한 페이지 복사는 minor page fault로 기록된다.
 */
static long get_minor_faults(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

/*
 * get_time_us - 현재 시간을 마이크로초 단위로 반환한다.
 */
static long long get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

/*
 * demo_cow_basic - COW의 기본 동작을 시연한다.
 *
 * fork() 후 자식이 읽기만 할 때와 쓰기를 할 때의
 * minor page fault 횟수 차이를 관찰한다.
 */
static void demo_cow_basic(void)
{
    char *region;
    int i;

    printf("=== 1. COW Fork 기본 동작 ===\n\n");

    /* 메모리 영역 할당 (MAP_ANONYMOUS: 파일이 아닌 익명 메모리) */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* 모든 페이지에 데이터 기록 (물리 페이지 확보) */
    for (i = 0; i < NUM_PAGES; i++) {
        region[i * PAGE_SIZE] = (char)('A' + (i % 26));
    }
    printf("[부모] %d 페이지에 데이터 기록 완료 (%d KB)\n",
           NUM_PAGES, REGION_SIZE / 1024);

    /* fork 전 minor fault 횟수 기록 */
    long faults_before_fork = get_minor_faults();
    printf("[부모] fork 전 minor page faults: %ld\n\n", faults_before_fork);

    pid_t pid = fork();

    if (pid == 0) {
        /* ---- 자식 프로세스 ---- */
        long faults_after_fork = get_minor_faults();
        printf("[자식] fork 직후 minor page faults: %ld\n", faults_after_fork);

        /* 읽기만 수행: COW 복사가 발생하지 않아야 함 */
        long faults_before_read = get_minor_faults();
        volatile char sum = 0;
        for (i = 0; i < NUM_PAGES; i++) {
            sum += region[i * PAGE_SIZE];
        }
        (void)sum;
        long faults_after_read = get_minor_faults();
        printf("[자식] 읽기 후 minor page faults: %ld (읽기로 인한 추가 faults: %ld)\n",
               faults_after_read, faults_after_read - faults_before_read);

        /* 쓰기 수행: COW 복사가 발생해야 함 */
        long faults_before_write = get_minor_faults();
        for (i = 0; i < NUM_PAGES; i++) {
            region[i * PAGE_SIZE] = 'Z';  /* 각 페이지에 쓰기 -> COW 발생 */
        }
        long faults_after_write = get_minor_faults();
        printf("[자식] 쓰기 후 minor page faults: %ld (쓰기로 인한 추가 faults: %ld)\n",
               faults_after_write, faults_after_write - faults_before_write);
        long sys_pgsize = sysconf(_SC_PAGESIZE);
        long expected = (long)NUM_PAGES * PAGE_SIZE / sys_pgsize;
        printf("[자식] -> 시스템 페이지 크기 기준 약 %ld번의 COW fault 예상 (실제: %ld)\n",
               expected, faults_after_write - faults_before_write);
        printf("         (시스템 페이지 크기 = %ld bytes, 프로그램 PAGE_SIZE = %d bytes)\n\n",
               sys_pgsize, PAGE_SIZE);

        munmap(region, REGION_SIZE);
        exit(0);
    } else if (pid > 0) {
        /* ---- 부모 프로세스 ---- */
        wait(NULL);

        /* 부모의 데이터가 변경되지 않았는지 확인 */
        int ok = 1;
        for (i = 0; i < NUM_PAGES; i++) {
            if (region[i * PAGE_SIZE] != (char)('A' + (i % 26))) {
                ok = 0;
                break;
            }
        }
        printf("[부모] 자식의 쓰기 후 부모 데이터 검증: %s\n",
               ok ? "변경 없음 (PASS)" : "변경됨! (FAIL)");
        printf("       -> COW 덕분에 자식의 쓰기가 부모에 영향을 주지 않음\n\n");

        munmap(region, REGION_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

/*
 * demo_cow_performance - COW fork의 성능 이점을 시연한다.
 *
 * 큰 메모리를 할당한 후 fork하고, exec 없이 즉시 종료하는 시나리오에서
 * COW가 불필요한 메모리 복사를 피하는 것을 관찰한다.
 */
static void demo_cow_performance(void)
{
    char *region;
    int i;

    printf("=== 2. COW Fork 성능 비교 ===\n\n");

    /* 큰 메모리 영역 할당 */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* 모든 페이지에 데이터 기록 */
    for (i = 0; i < NUM_PAGES; i++) {
        memset(region + i * PAGE_SIZE, 'X', PAGE_SIZE);
    }

    /* fork 시간 측정 */
    long long t1 = get_time_us();
    pid_t pid = fork();
    long long t2 = get_time_us();

    if (pid == 0) {
        /* 자식: fork만 하고 즉시 종료 (exec 시뮬레이션) */
        printf("[자식] fork 소요 시간: %lld us\n", t2 - t1);
        printf("[자식] -> COW 덕분에 %d KB를 복사하지 않고 즉시 fork 완료\n",
               REGION_SIZE / 1024);
        printf("[자식] -> 만약 eager copy였다면 %d 페이지를 모두 복사해야 했음\n\n",
               NUM_PAGES);
        munmap(region, REGION_SIZE);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
        munmap(region, REGION_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

/*
 * demo_cow_isolation - COW에서 프로세스 격리가 보장됨을 확인한다.
 */
static void demo_cow_isolation(void)
{
    int *shared_val;

    printf("=== 3. COW 프로세스 격리 확인 ===\n\n");

    /* 정수값을 저장할 메모리 할당 */
    shared_val = mmap(NULL, PAGE_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (shared_val == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    *shared_val = 42;
    printf("[부모] fork 전 값: %d\n", *shared_val);

    pid_t pid = fork();

    if (pid == 0) {
        /* 자식 프로세스 */
        printf("[자식] fork 직후 값: %d (부모와 같은 물리 페이지를 공유 중)\n",
               *shared_val);

        /* MAP_PRIVATE이므로 쓰기 시 COW 발생 */
        *shared_val = 100;
        printf("[자식] 값 변경 후: %d (COW로 인해 별도의 물리 페이지에 기록됨)\n",
               *shared_val);

        munmap(shared_val, PAGE_SIZE);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
        printf("[부모] 자식 종료 후 부모의 값: %d (변경되지 않음!)\n", *shared_val);
        printf("       -> COW가 프로세스 간 격리를 보장함\n\n");

        munmap(shared_val, PAGE_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

int main(void)
{
    printf("=========================================\n");
    printf("  COW (Copy-On-Write) 동작 시연 프로그램\n");
    printf("=========================================\n\n");
    printf("시스템 페이지 크기: %d bytes\n", (int)sysconf(_SC_PAGESIZE));
    printf("할당 영역 크기: %d pages (%d KB)\n\n", NUM_PAGES, REGION_SIZE / 1024);

    demo_cow_basic();
    demo_cow_performance();
    demo_cow_isolation();

    printf("=========================================\n");
    printf("  요약\n");
    printf("=========================================\n");
    printf("1. fork() 후 읽기만 하면 COW 복사가 발생하지 않는다\n");
    printf("2. 쓰기를 하면 minor page fault가 발생하고 페이지가 복사된다\n");
    printf("3. COW 덕분에 부모와 자식의 데이터는 완전히 격리된다\n");
    printf("4. fork + exec 패턴에서 불필요한 메모리 복사를 피할 수 있다\n");

    return 0;
}

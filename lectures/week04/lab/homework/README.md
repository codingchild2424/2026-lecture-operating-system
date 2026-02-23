# Week 4 과제: Parallel Hash Table

> 교재: xv6 Chapter 6 (Locking) - Exercise 4
> 제출 파일: `hashtable.c` (수정된 파일)

## 개요

이 과제에서는 pthread 기반의 **병렬 해시 테이블**을 구현합니다.
멀티스레드 환경에서 안전하게 동작하도록 두 가지 locking 전략을 구현하고 성능을 비교합니다.

- **Phase 1**: Coarse-grained locking (단일 global mutex)
- **Phase 2**: Fine-grained locking (버킷별 mutex)

## 학습 목표

- Critical section과 mutual exclusion의 개념 이해
- Coarse-grained vs. fine-grained locking의 trade-off 분석
- Lock granularity가 병렬 성능에 미치는 영향 실험
- xv6 커널의 locking 전략과의 비교 (예: `kalloc.c`의 freelist lock)

## 파일 구조

```
homework/
  skeleton/          <- 작업 디렉터리 (여기서 코딩)
    hashtable.h      - 구조체 정의 (수정 불필요)
    hashtable.c      - TODO가 있는 구현 파일 (이 파일을 수정)
    main.c           - 벤치마크 코드 (수정 불필요)
    Makefile         - 빌드 설정
  solution/          - 참고 답안 (제출 후 공개)
  tests/             - 테스트 스크립트
    test_correctness.sh  - 정확성 테스트
    test_performance.sh  - 성능 측정
```

## 빌드 및 실행

```bash
cd skeleton/
make

# 실행: ./hashtable_bench [none|coarse|fine] [threads] [keys]
./hashtable_bench none   4 100000   # locking 없음 (race condition 발생)
./hashtable_bench coarse 4 100000   # coarse-grained
./hashtable_bench fine   4 100000   # fine-grained
```

## 과제 상세

### Phase 1: Coarse-grained Locking

`hashtable.c`를 수정하여 **단일 global mutex**로 해시 테이블을 보호하세요.

구현 위치: `hashtable.c`의 `TODO` 주석을 찾아 수정

**구현 내용:**

1. `hashtable_init()`: `ht->global_lock` 초기화 (`pthread_mutex_init`)
2. `hashtable_destroy()`: `ht->global_lock` 파괴 (`pthread_mutex_destroy`)
3. `hashtable_insert()`: 함수 시작에서 global lock 획득, 반환 전 해제
4. `hashtable_lookup()`: 동일
5. `hashtable_delete()`: 동일

**핵심 아이디어:**
```c
void hashtable_insert(struct hashtable *ht, int key, int value)
{
    // strategy가 LOCK_COARSE일 때만 lock
    if (ht->strategy == LOCK_COARSE)
        pthread_mutex_lock(&ht->global_lock);

    // ... 기존 코드 ...

    if (ht->strategy == LOCK_COARSE)
        pthread_mutex_unlock(&ht->global_lock);
}
```

**검증:**
```bash
make
./hashtable_bench coarse 4 100000
# "All keys found correctly." 가 출력되어야 함
```

### Phase 2: Fine-grained Locking

**버킷별 mutex**를 사용하여 서로 다른 버킷에 접근하는 스레드가 동시에 실행되도록 하세요.

**구현 내용:**

1. `hashtable_init()`: 각 `ht->buckets[i].lock` 초기화
2. `hashtable_destroy()`: 각 `ht->buckets[i].lock` 파괴
3. `hashtable_insert()`: 해당 버킷의 lock만 획득/해제
4. `hashtable_lookup()`: 동일
5. `hashtable_delete()`: 동일

**핵심 아이디어:**
```c
void hashtable_insert(struct hashtable *ht, int key, int value)
{
    int idx = hash(key);

    if (ht->strategy == LOCK_FINE)
        pthread_mutex_lock(&ht->buckets[idx].lock);

    // ... 기존 코드 (idx 버킷에만 접근) ...

    if (ht->strategy == LOCK_FINE)
        pthread_mutex_unlock(&ht->buckets[idx].lock);
}
```

**검증:**
```bash
./hashtable_bench fine 4 100000
# "All keys found correctly." 가 출력되어야 함
```

### 성능 비교 보고

1, 2, 4 threads로 coarse-grained과 fine-grained의 성능을 비교하세요.

```bash
# 테스트 스크립트 사용
cd ../tests/
./test_performance.sh ../skeleton/hashtable_bench
```

또는 직접 실행:
```bash
# Coarse-grained
./hashtable_bench coarse 1 200000
./hashtable_bench coarse 2 200000
./hashtable_bench coarse 4 200000

# Fine-grained
./hashtable_bench fine 1 200000
./hashtable_bench fine 2 200000
./hashtable_bench fine 4 200000
```

## 테스트

```bash
# 정확성 테스트 (coarse, fine 모두 테스트)
cd tests/
./test_correctness.sh ../skeleton/hashtable_bench

# 성능 테스트
./test_performance.sh ../skeleton/hashtable_bench
```

## 제출물

1. **`hashtable.c`**: locking이 구현된 소스 코드
2. **보고서** (짧은 텍스트 또는 주석으로 작성):
   - 성능 측정 결과 표 (strategy x thread 수)
   - 다음 질문에 대한 답:

### 보고서 질문

1. `none` 모드에서 thread를 4개 사용하면 어떤 문제가 발생하는가?
   실제로 `./hashtable_bench none 4 100000`을 실행하고 결과를 설명하세요.

2. Coarse-grained locking에서 thread 수를 1에서 4로 늘리면 성능이 어떻게 변하는가?
   왜 그런가?

3. Fine-grained locking이 coarse-grained보다 빠른가? 그 이유는?

4. 버킷 수(`NBUCKETS`)를 13에서 101로 변경하면 fine-grained의 성능이 어떻게 변하는가?
   (힌트: `hashtable.h`에서 `NBUCKETS`를 수정하고 다시 빌드)

5. xv6의 `kalloc.c`에서는 어떤 locking 전략을 사용하는가?
   Fine-grained locking으로 바꾸면 어떤 장점이 있는가?

## 채점 기준

| 항목 | 배점 |
|------|------|
| Phase 1: Coarse-grained 정확성 | 30% |
| Phase 2: Fine-grained 정확성 | 30% |
| 성능 측정 및 비교 | 20% |
| 보고서 질문 답변 | 20% |

## 힌트

- `hashtable.c`의 `TODO` 주석을 모두 찾아서 구현하세요.
- `ht->strategy`를 확인하여 적절한 lock을 사용하세요.
- Lock을 획득했으면 **모든 return 경로에서** 반드시 해제해야 합니다. (특히 `hashtable_insert`의 중간 return)
- `pthread_mutex_init`, `pthread_mutex_lock`, `pthread_mutex_unlock`, `pthread_mutex_destroy`만 사용하면 됩니다.
- Helper 함수를 만들면 코드가 깔끔해집니다 (solution 참고).

## 참고 자료

- xv6 교재 Chapter 6: Locking
- `kernel/spinlock.c`: xv6의 spinlock 구현
- `kernel/kalloc.c`: xv6 메모리 할당기 (coarse-grained lock 사용 예)
- `pthread_mutex(3)` man page

# Week 5 Lab: Thread & Concurrency 2 - 동기화 심화

> 수업 중 실습 (~50분)
>
> 교재 참고: xv6 textbook Ch 6.6-6.10 (Locking 심화), Ch 9 (Concurrency Revisited)

---

## 학습 목표

- Condition variable을 활용한 스레드 간 통신 패턴을 이해한다
- Producer-Consumer 문제를 올바르게 구현할 수 있다
- xv6 커널의 spinlock 구현을 분석하고 각 함수의 역할을 설명할 수 있다
- xv6 kalloc.c에서 lock이 보호하는 대상과 그 필요성을 설명할 수 있다

---

## 실습 1: Producer-Consumer 문제 구현 (15분)

### 배경

Producer-Consumer는 동기화의 대표적인 문제이다. 공유 버퍼를 사이에 두고:
- **Producer**: 데이터를 생성하여 버퍼에 넣는다
- **Consumer**: 버퍼에서 데이터를 꺼내 처리한다

핵심 조건:
- 버퍼가 가득 차면 producer는 **대기**해야 한다
- 버퍼가 비어 있으면 consumer는 **대기**해야 한다
- 버퍼 접근 시 **상호 배제**가 보장되어야 한다

### 사용할 동기화 도구

| 도구 | 역할 |
|------|------|
| `pthread_mutex_t` | 공유 데이터(버퍼)에 대한 상호 배제 |
| `pthread_cond_t not_full` | 버퍼에 빈 공간이 생겼음을 알림 |
| `pthread_cond_t not_empty` | 버퍼에 데이터가 들어왔음을 알림 |

### 실습 진행

1. 예제 코드를 컴파일하고 실행한다:

```bash
cd examples/
gcc -Wall -pthread -o producer_consumer producer_consumer.c
./producer_consumer
```

2. `producer_consumer.c`를 열고 다음 부분을 중점적으로 확인한다:

#### (a) `buffer_put` 함수 - Producer 동작

```c
void buffer_put(bounded_buffer_t *buf, int item) {
    pthread_mutex_lock(&buf->mutex);

    // 왜 if가 아니라 while인가?
    while (buf->count == BUFFER_SIZE) {
        pthread_cond_wait(&buf->not_full, &buf->mutex);
    }

    buf->data[buf->in] = item;
    buf->in = (buf->in + 1) % BUFFER_SIZE;
    buf->count++;

    pthread_cond_signal(&buf->not_empty);
    pthread_mutex_unlock(&buf->mutex);
}
```

#### (b) `buffer_get` 함수 - Consumer 동작

```c
int buffer_get(bounded_buffer_t *buf) {
    int item;
    pthread_mutex_lock(&buf->mutex);

    while (buf->count == 0) {
        pthread_cond_wait(&buf->not_empty, &buf->mutex);
    }

    item = buf->data[buf->out];
    buf->out = (buf->out + 1) % BUFFER_SIZE;
    buf->count--;

    pthread_cond_signal(&buf->not_full);
    pthread_mutex_unlock(&buf->mutex);
    return item;
}
```

### 생각해볼 점

- `pthread_cond_wait`에서 조건을 확인할 때 `if`가 아닌 `while`을 사용하는 이유는?
  - 힌트: **spurious wakeup**, Mesa semantics
- `pthread_cond_wait`가 호출되면 mutex에 어떤 일이 발생하는가?
  - 힌트: atomically unlock + sleep, 깨어나면 자동 relock
- `signal`과 `broadcast`의 차이는?

---

## 실습 2: 다중 Producer/Consumer Bounded Buffer (10분)

### 실습 진행

1. 컴파일 및 실행:

```bash
gcc -Wall -pthread -o bounded_buffer bounded_buffer.c
./bounded_buffer
```

2. `bounded_buffer.c`는 **3개의 Producer + 3개의 Consumer**가 동시에 동작한다. 다음을 관찰한다:

- 버퍼 크기가 4로 작기 때문에 대기가 빈번하게 발생
- 모든 아이템이 정확히 한 번씩 생산/소비됨을 검증

3. **실험**: 코드를 수정해서 다음을 시도해 본다:

| 변경 | 예상 결과 |
|------|-----------|
| `while`을 `if`로 바꾸기 | race condition 발생 가능 (assert 실패) |
| `signal`을 `broadcast`로 바꾸기 | 정상 동작, 불필요한 wakeup 증가 |
| `BUFFER_SIZE`를 1로 변경 | 교대 실행, 동시성 감소 |
| Consumer 수를 1로 줄이기 | 소비 병목, 생산자 대기 증가 |

---

## 실습 3: xv6 kernel/spinlock.c 코드 분석 (15분)

> 파일 위치: `xv6-riscv/kernel/spinlock.c`

xv6의 spinlock 구현을 함께 읽으며 각 함수의 역할을 분석한다.

### 3-1. `acquire` 함수

```c
void acquire(struct spinlock *lk)
{
    push_off();   // (1) 인터럽트 비활성화
    if(holding(lk))
        panic("acquire");

    // (2) atomic swap으로 lock 획득 시도
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;

    // (3) memory fence
    __sync_synchronize();

    // (4) 디버깅 정보 기록
    lk->cpu = mycpu();
}
```

#### 분석 포인트

| 단계 | 코드 | 질문 |
|------|------|------|
| (1) | `push_off()` | 왜 lock 획득 전에 인터럽트를 꺼야 하는가? |
| (2) | `__sync_lock_test_and_set` | 이 atomic 연산이 test-and-set과 어떻게 다른가? |
| (3) | `__sync_synchronize` | memory fence가 없으면 어떤 문제가 발생하는가? |
| (4) | `lk->cpu = mycpu()` | 이 정보는 어디에서 사용되는가? |

**인터럽트를 꺼야 하는 이유 (교재 Ch 6.6)**:
- CPU가 lock을 보유한 상태에서 인터럽트 발생
- 인터럽트 핸들러가 같은 lock을 acquire 시도
- 자기 자신이 보유한 lock을 기다리며 영원히 spinning -> **deadlock**

### 3-2. `release` 함수

```c
void release(struct spinlock *lk)
{
    if(!holding(lk))
        panic("release");

    lk->cpu = 0;

    __sync_synchronize();              // (1) memory fence
    __sync_lock_release(&lk->locked);  // (2) atomic: locked = 0

    pop_off();                         // (3) 인터럽트 복원
}
```

**질문**: `__sync_synchronize()`가 `__sync_lock_release` **앞에** 있어야 하는 이유는?
- 힌트: critical section 내부의 store가 lock 해제 이후로 재배치되면 안 된다

### 3-3. `push_off` / `pop_off`

```c
void push_off(void) {
    int old = intr_get();
    intr_off();
    if(mycpu()->noff == 0)
        mycpu()->intena = old;    // 최초 push 시 이전 상태 저장
    mycpu()->noff += 1;           // 중첩 횟수 증가
}

void pop_off(void) {
    struct cpu *c = mycpu();
    if(intr_get())
        panic("pop_off - interruptible");
    if(c->noff < 1)
        panic("pop_off");
    c->noff -= 1;
    if(c->noff == 0 && c->intena)
        intr_on();                // 모든 lock 해제 후 복원
}
```

**핵심 개념: 중첩 가능한 인터럽트 비활성화**

- lock을 중첩 획득하면 (`push_off` 여러 번) 인터럽트를 바로 켜면 안 됨
- `noff` 카운터로 추적하여 마지막 lock 해제 시에만 인터럽트 복원
- `intena`는 최초 `push_off` 호출 시점의 인터럽트 상태를 기억

**질문**: lock A를 acquire한 후 lock B를 acquire하면 `noff`는 몇인가? lock B를 release할 때 인터럽트가 켜지는가?

---

## 실습 4: xv6 kernel/kalloc.c에서 Lock 역할 분석 (10분)

> 파일 위치: `xv6-riscv/kernel/kalloc.c`

### 구조 분석

```c
struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;
```

- `kmem`은 전체 시스템에 **하나만** 존재하는 전역 변수
- `freelist`는 사용 가능한 물리 메모리 페이지의 연결 리스트
- `lock`은 이 연결 리스트를 보호

### kfree 분석

```c
void kfree(void *pa) {
    struct run *r;

    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    memset(pa, 1, PGSIZE);       // dangling reference 감지용 쓰레기값 채우기

    r = (struct run*)pa;

    acquire(&kmem.lock);          // --- critical section 시작 ---
    r->next = kmem.freelist;      // 리스트 head에 삽입
    kmem.freelist = r;
    release(&kmem.lock);          // --- critical section 끝 ---
}
```

### kalloc 분석

```c
void *kalloc(void) {
    struct run *r;

    acquire(&kmem.lock);          // --- critical section 시작 ---
    r = kmem.freelist;
    if(r)
        kmem.freelist = r->next;  // 리스트 head에서 제거
    release(&kmem.lock);          // --- critical section 끝 ---

    if(r)
        memset((char*)r, 5, PGSIZE);
    return (void*)r;
}
```

### 생각해볼 점

1. **lock이 없다면 무슨 일이 발생하는가?**
   - 두 CPU가 동시에 `kalloc`을 호출하면?
   - 같은 페이지가 두 번 할당될 수 있다 (double allocation)

2. **성능 문제**:
   - 모든 CPU가 **하나의 lock**을 공유한다
   - CPU 수가 많아지면 `kalloc`/`kfree` 호출 시 **contention** 발생
   - 이것이 이번 주 과제의 동기: **per-CPU free list**로 개선!

3. **memset의 위치**:
   - `kfree`에서 `memset(pa, 1, PGSIZE)` - lock 밖에서 실행 (아직 freelist에 없음)
   - `kalloc`에서 `memset((char*)r, 5, PGSIZE)` - lock 밖에서 실행 (이미 freelist에서 제거됨)
   - lock의 범위를 최소화하여 성능 향상

---

## 정리

| 주제 | 핵심 내용 |
|------|-----------|
| Condition Variable | `while` 루프 + `wait`/`signal` 패턴으로 스레드 간 조건 동기화 |
| Bounded Buffer | mutex + 2개의 cond var (`not_full`, `not_empty`) |
| xv6 spinlock | atomic swap + memory fence + interrupt disable |
| push_off/pop_off | 중첩 가능한 인터럽트 관리 (`noff` 카운터) |
| kalloc lock | 단일 lock이 free list를 보호, per-CPU로 개선 가능 |

---

## 참고 자료

- xv6 textbook Chapter 6: Locking (특히 6.6-6.10)
- xv6 textbook Chapter 9: Concurrency Revisited
- OSTEP Chapter 30: Condition Variables
- `man pthread_cond_wait`, `man pthread_mutex_lock`

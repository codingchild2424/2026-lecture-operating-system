# Week 5 과제: 동기화 심화

> 교재 참고: xv6 textbook Ch 6 (Locking), Ch 9 (Concurrency Revisited)
>
> 제출 기한: 다음 수업 전까지

---

## 과제 1: xv6 kalloc Per-CPU Free List 개선 (필수)

> 교재 Ch 6, Exercise 3 기반

### 배경

현재 xv6의 물리 메모리 할당기(`kernel/kalloc.c`)는 다음과 같이 구현되어 있다:

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
```

- 전체 시스템에 **단일 free list** + **단일 lock**
- 모든 CPU가 `kalloc()`/`kfree()` 호출 시 같은 lock을 경쟁
- CPU 수가 증가하면 **lock contention** 심화 -> 성능 저하

### 목표

`kmem`을 **CPU별 free list 배열**로 변경하여 병렬 메모리 할당 성능을 향상시킨다.

```
변경 전:                        변경 후:
┌──────────────────┐            ┌──────────────────┐
│  kmem             │            │  kmem[0]          │  CPU 0 전용
│  ├─ lock          │            │  ├─ lock          │
│  └─ freelist ─→...│            │  └─ freelist ─→...│
└──────────────────┘            ├──────────────────┤
    모든 CPU가 공유              │  kmem[1]          │  CPU 1 전용
                                │  ├─ lock          │
                                │  └─ freelist ─→...│
                                ├──────────────────┤
                                │  ...              │
                                ├──────────────────┤
                                │  kmem[NCPU-1]     │  CPU 7 전용
                                │  ├─ lock          │
                                │  └─ freelist ─→...│
                                └──────────────────┘
```

### 수정 대상 파일

`kernel/kalloc.c` 하나만 수정하면 된다.

### 구현 요구사항

#### 1. 자료구조 변경

```c
// 기존
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 변경 후
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];   // NCPU는 param.h에 8로 정의
```

#### 2. `kinit()` 수정

- 모든 CPU의 lock을 초기화한다 (`initlock`)
- `freerange`는 기존과 동일하게 호출한다
- `kinit`은 CPU 0에서만 실행되므로, 초기 free page는 모두 CPU 0의 freelist에 들어간다

#### 3. `kfree()` 수정

- 현재 CPU의 freelist에 페이지를 반환한다
- `cpuid()`를 안전하게 호출하려면 인터럽트가 꺼져 있어야 한다
- `push_off()` / `pop_off()`를 사용하여 인터럽트를 관리한다

```
kfree 흐름:
  push_off()
  id = cpuid()
  acquire(&kmem[id].lock)
  freelist에 페이지 추가
  release(&kmem[id].lock)
  pop_off()
```

#### 4. `kalloc()` 수정

- **먼저** 현재 CPU의 freelist에서 할당을 시도한다
- **자신의 freelist가 비었으면** 다른 CPU의 freelist에서 **steal**한다
- 모든 CPU의 freelist가 비었으면 0을 반환한다 (메모리 부족)

```
kalloc 흐름:
  push_off()
  id = cpuid()

  // 1. 자기 CPU에서 시도
  acquire(&kmem[id].lock)
  r = kmem[id].freelist에서 하나 꺼냄
  release(&kmem[id].lock)

  // 2. 실패 시 다른 CPU에서 steal
  if(!r) {
    for each other CPU i:
      acquire(&kmem[i].lock)
      r = kmem[i].freelist에서 하나 꺼냄
      release(&kmem[i].lock)
      if(r) break
  }

  pop_off()
  return r
```

### 주의사항

1. **Deadlock 방지**: steal 시 한 번에 하나의 lock만 보유한다. 두 개의 lock을 동시에 잡으면 deadlock 위험이 있다.

2. **cpuid()와 인터럽트**: `cpuid()`는 `tp` 레지스터를 읽어 현재 CPU 번호를 반환한다. 인터럽트가 켜진 상태에서 호출하면 context switch로 인해 잘못된 CPU 번호를 얻을 수 있다. 반드시 인터럽트를 끈 상태에서 호출해야 한다.

3. **acquire 내부의 push_off**: `acquire()`는 내부적으로 `push_off()`를 호출한다. 그러나 `cpuid()`를 `acquire()` 전에 호출해야 하므로, 별도로 `push_off()`를 먼저 호출해야 한다.

### 시작하기

skeleton 파일이 제공된다:

```bash
# skeleton 파일을 xv6 소스에 복사
cp skeleton/kalloc_percpu.c ../../xv6-riscv/kernel/kalloc.c
```

skeleton 파일에서 `TODO` 주석을 찾아 코드를 완성하면 된다.

### 테스트

```bash
cd ../../xv6-riscv
make clean
make qemu
```

xv6 셸에서:
```
$ usertests
```

모든 테스트가 `OK`로 통과해야 한다. 자세한 테스트 방법은 `tests/test_kalloc.sh`를 참고한다.

### 평가 기준

| 항목 | 배점 |
|------|------|
| `kmem`을 per-CPU 배열로 올바르게 변경 | 20% |
| `kinit()`에서 모든 CPU lock 초기화 | 10% |
| `kfree()`에서 현재 CPU freelist에 반환 | 20% |
| `kalloc()`에서 자기 CPU 우선 할당 | 20% |
| `kalloc()`에서 steal 구현 | 20% |
| `usertests` 전체 통과 | 10% |

---

## 과제 2: Barrier 구현 (보너스, +20점)

### 배경

**Barrier**는 다수의 스레드가 특정 지점에서 모두 만날 때까지 대기하는 동기화 기법이다.

```
스레드 0: ─── 작업 ───|   대기   |─── 다음 작업 ───
스레드 1: ─── 작업 ────|  대기  |─── 다음 작업 ───
스레드 2: ─── 작업 ──────| 대기 |─── 다음 작업 ───
                          ↑
                     barrier point
                  (마지막 스레드 도착 시
                   모두 동시에 진행)
```

### 목표

`pthread_mutex`와 `pthread_cond`만을 사용하여 barrier를 직접 구현한다.

### 요구사항

1. `barrier_init(b, n)`: barrier를 n개 스레드용으로 초기화
2. `barrier_wait(b)`: barrier에서 대기. 마지막 스레드 도착 시 모두 진행
3. `barrier_destroy(b)`: 자원 해제
4. **재사용 가능(reusable)**: barrier를 여러 라운드에 걸쳐 사용할 수 있어야 한다

### 핵심 포인트: 재사용 가능한 Barrier

단순 구현은 한 번만 작동하고, 재사용 시 버그가 발생한다:

```
문제 시나리오:
1. 라운드 0: 5개 스레드 모두 도착 -> count 리셋, broadcast
2. 스레드 A가 빠르게 라운드 1의 barrier_wait에 진입 (count=1)
3. 스레드 B가 아직 라운드 0의 wait에서 깨어나는 중
   -> B가 count를 보면 1인데, 이것이 라운드 0인지 1인지 구분 불가!
```

해결: **round(세대) 번호**를 사용하여 라운드를 구분한다.

### 시작하기

```bash
cd skeleton/
gcc -Wall -pthread -o barrier barrier.c
./barrier
```

skeleton 파일의 `TODO` 주석을 찾아 코드를 완성한다. 테스트 코드는 이미 포함되어 있으며, 3라운드에 걸쳐 5개 스레드로 검증한다.

### 평가 기준

| 항목 | 배점 |
|------|------|
| `barrier_init` 올바른 초기화 | 20% |
| `barrier_wait` 기본 동작 (1라운드) | 30% |
| `barrier_wait` 재사용 가능 (다중 라운드) | 40% |
| 자원 해제 및 코드 품질 | 10% |

---

## 제출물

```
week5/homework/
├── skeleton/
│   ├── kalloc_percpu.c     <- TODO를 완성하여 제출 (필수)
│   └── barrier.c           <- TODO를 완성하여 제출 (보너스)
└── report.md               <- 간단한 구현 설명 (선택)
```

### report.md 작성 가이드 (선택)

다음 질문에 대한 답을 작성하면 가산점이 있다:

1. per-CPU free list로 변경했을 때, 어떤 상황에서 성능이 향상되는가?
2. steal 과정에서 deadlock이 발생할 수 있는 경우는? 어떻게 방지했는가?
3. (보너스) barrier에서 round 변수가 없으면 어떤 버그가 발생하는가?

---

## 참고 자료

- xv6 textbook Chapter 6: Locking
- xv6 소스 코드: `kernel/kalloc.c`, `kernel/spinlock.c`, `kernel/param.h`
- OSTEP Chapter 28: Locks
- OSTEP Chapter 30: Condition Variables
- OSTEP Chapter 31: Semaphores (Barrier 개념)

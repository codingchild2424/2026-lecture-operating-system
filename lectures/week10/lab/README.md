# Week 10 Lab: Deadlocks

## 학습 목표

이번 실습을 마치면 다음을 할 수 있습니다:

1. Deadlock의 4가지 필요 조건을 설명하고, 실제 코드에서 각 조건이 어떻게 충족되는지 식별할 수 있다
2. pthread mutex를 사용하여 deadlock이 발생하는 상황을 직접 관찰하고 원인을 분석할 수 있다
3. Lock ordering과 trylock을 사용하여 deadlock을 해결/회피하는 방법을 구현할 수 있다
4. xv6 커널 소스 코드에서 lock ordering이 어떻게 적용되어 있는지 분석할 수 있다
5. xv6의 프로세스 kill과 locking 사이의 관계를 이해할 수 있다

**교재 범위**: xv6 book Ch 6.4 (Lock ordering), Ch 7.9 (Code: Kill)

---

## 배경 지식: Deadlock이란?

**Deadlock**(교착 상태)이란 두 개 이상의 프로세스(또는 스레드)가 서로가 보유한 자원을 기다리며 영원히 진행하지 못하는 상태입니다.

### Deadlock의 4가지 필요 조건 (Coffman Conditions)

Deadlock이 발생하려면 다음 4가지 조건이 **동시에** 성립해야 합니다:

| 조건 | 영문 | 설명 |
|------|------|------|
| **상호 배제** | Mutual Exclusion | 자원은 한 번에 하나의 프로세스만 사용할 수 있다 |
| **점유 대기** | Hold & Wait | 자원을 보유한 상태에서 다른 자원을 기다린다 |
| **비선점** | No Preemption | 다른 프로세스가 보유한 자원을 강제로 뺏을 수 없다 |
| **순환 대기** | Circular Wait | 프로세스들이 순환적으로 서로의 자원을 기다린다 |

이 중 하나라도 깨뜨리면 deadlock을 방지할 수 있습니다.

### Deadlock 예시 (개념도)

```
Thread 1                    Thread 2
   |                           |
   | lock(A) -- 성공            | lock(B) -- 성공
   |                           |
   | lock(B) -- 대기...         | lock(A) -- 대기...
   |     ^                     |     ^
   |     |_____________________|_____|
   |                           |
   +-- Thread 2가 B를 놓기를    +-- Thread 1이 A를 놓기를
       기다림                       기다림

   ==> 영원히 진행 불가 (Deadlock!)
```

---

## Exercise 1: Deadlock 발생 관찰 (~10분)

### 목표
실제로 deadlock이 발생하여 프로그램이 멈추는 것을 직접 관찰합니다.

### 코드 분석

`examples/deadlock_demo.c` 파일을 열어 코드를 읽어보세요.

```c
/* Thread 1: A를 먼저 잡고, B를 잡으려고 시도 */
void *thread1_func(void *arg)
{
    pthread_mutex_lock(&mutex_A);    // (1) A 잠금
    usleep(100000);                  // (2) 잠시 대기
    pthread_mutex_lock(&mutex_B);    // (3) B 잠금 시도 -> 대기!
    ...
}

/* Thread 2: B를 먼저 잡고, A를 잡으려고 시도 (반대 순서!) */
void *thread2_func(void *arg)
{
    pthread_mutex_lock(&mutex_B);    // (1) B 잠금
    usleep(100000);                  // (2) 잠시 대기
    pthread_mutex_lock(&mutex_A);    // (3) A 잠금 시도 -> 대기!
    ...
}
```

### 실습 단계

**1단계: 컴파일**

```bash
cd practice/week10/lab/examples
gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
```

**2단계: 실행**

```bash
./deadlock_demo
```

프로그램이 멈추는 것을 확인하세요. 약 10초 정도 기다린 후 `Ctrl+C`로 종료합니다.

**3단계: 분석**

> **질문 1**: 프로그램 출력에서 마지막으로 출력된 메시지는 무엇인가요? 그 이후에 왜 더 이상 진행되지 않았나요?
>
> **질문 2**: 이 상황에서 Deadlock의 4가지 조건이 어떻게 충족되는지 하나씩 설명해 보세요.

<details>
<summary>정답 보기</summary>

**질문 1 정답**:
- Thread 1은 "mutex_B 잠금 시도..." 에서 멈춤
- Thread 2는 "mutex_A 잠금 시도..." 에서 멈춤
- Thread 1이 mutex_B를 기다리고, Thread 2가 mutex_A를 기다리는데, 각각 상대가 보유한 lock을 기다리므로 영원히 진행할 수 없습니다.

**질문 2 정답**:
1. **Mutual Exclusion**: mutex는 한 번에 하나의 스레드만 잠글 수 있습니다
2. **Hold & Wait**: Thread 1은 mutex_A를 보유한 채 mutex_B를 기다리고, Thread 2는 mutex_B를 보유한 채 mutex_A를 기다립니다
3. **No Preemption**: `pthread_mutex_lock`은 lock을 강제로 뺏지 못하고, 상대가 풀어줄 때까지 기다립니다
4. **Circular Wait**: Thread 1 -> mutex_B(Thread 2 보유) -> Thread 2 -> mutex_A(Thread 1 보유) -> Thread 1... 순환 발생

</details>

---

## Exercise 2: Lock Ordering으로 Deadlock 해결 (~10분)

### 목표
**순환 대기(Circular Wait)** 조건을 깨뜨려 deadlock을 방지합니다.

### 핵심 아이디어

모든 스레드가 lock을 **동일한 순서**로 획득하면 순환 대기가 발생할 수 없습니다:

```
규칙: 항상 mutex_A를 먼저, mutex_B를 나중에 잠금

Thread 1: lock(A) -> lock(B)    (원래와 동일)
Thread 2: lock(A) -> lock(B)    (순서 변경!)
```

이렇게 하면 Thread 2가 A를 잡지 못하고 기다리는 동안, Thread 1이 A와 B를 모두 사용하고 해제한 후 Thread 2가 진행할 수 있습니다.

### 실습 단계

**1단계: 코드 분석**

`examples/deadlock_fix_ordering.c`를 열어 두 스레드 함수를 비교해 보세요.

핵심 변경점 (`thread2_func`에서):
```c
/* [수정 전 - deadlock 발생] */
pthread_mutex_lock(&mutex_B);  // B를 먼저
pthread_mutex_lock(&mutex_A);  // A를 나중에

/* [수정 후 - lock ordering 적용] */
pthread_mutex_lock(&mutex_A);  // A를 먼저 (순서 통일!)
pthread_mutex_lock(&mutex_B);  // B를 나중에
```

**2단계: 컴파일 및 실행**

```bash
gcc -Wall -pthread -o deadlock_fix_ordering deadlock_fix_ordering.c
./deadlock_fix_ordering
```

**3단계: 확인**

프로그램이 정상적으로 종료되는 것을 확인합니다. 두 스레드가 각각 5번 반복하여 총 10번의 임계 영역 진입이 모두 성공합니다.

> **질문 3**: Lock ordering이 4가지 deadlock 조건 중 어떤 조건을 깨뜨리나요?

<details>
<summary>정답 보기</summary>

**Circular Wait (순환 대기)** 를 깨뜨립니다.

모든 스레드가 동일한 순서(A -> B)로 lock을 잡으면, 한 스레드가 A를 잡고 B를 기다리는 동안 다른 스레드는 A조차 잡지 못하므로 순환이 형성될 수 없습니다.

이것이 xv6 커널이 채택한 전략이며, 교재 Ch 6.4에서 설명하는 내용입니다.

</details>

---

## Exercise 3: pthread_mutex_trylock으로 Deadlock 회피 (~10분)

### 목표
**점유 대기(Hold & Wait)** 조건을 깨뜨려 deadlock을 회피합니다.

### 핵심 아이디어

`pthread_mutex_trylock`은 lock을 즉시 획득하지 못하면 **차단하지 않고 실패를 반환**합니다. 실패 시 이미 보유한 lock을 해제(back-off)하면 Hold & Wait 조건이 깨집니다:

```
Thread 1:
  lock(A)
  if trylock(B) 실패:
      unlock(A)        <- 보유한 것을 해제! (Hold & Wait 파괴)
      잠시 대기
      처음부터 재시도
```

### 실습 단계

**1단계: 코드 분석**

`examples/deadlock_fix_trylock.c`에서 `thread1_func`의 핵심 로직을 읽어보세요:

```c
while (!success) {
    pthread_mutex_lock(&mutex_A);           // A 잠금

    if (pthread_mutex_trylock(&mutex_B) == 0) {
        /* 성공: 임계 영역 진입 */
        success = 1;
        ...
    } else {
        /* 실패: back-off */
        pthread_mutex_unlock(&mutex_A);     // A 해제!
        usleep(rand() % 50000);             // 랜덤 대기
    }
}
```

**2단계: 컴파일 및 실행**

```bash
gcc -Wall -pthread -o deadlock_fix_trylock deadlock_fix_trylock.c
./deadlock_fix_trylock
```

**3단계: 출력 분석**

출력에서 "trylock 실패! -> back-off" 메시지를 찾아보세요. 일부 시도에서 trylock이 실패하고, back-off 후 재시도하여 성공하는 패턴을 확인합니다.

> **질문 4**: trylock 방식이 4가지 deadlock 조건 중 어떤 조건을 깨뜨리나요?

> **질문 5**: trylock + back-off 방식의 단점은 무엇인가요?

<details>
<summary>정답 보기</summary>

**질문 4 정답**: **Hold & Wait (점유 대기)** 를 깨뜨립니다. trylock이 실패하면 이미 보유한 lock을 해제하므로, "자원을 보유한 채 다른 자원을 기다리는" 상황이 발생하지 않습니다.

**질문 5 정답**:
- **Livelock** 위험: 두 스레드가 계속 trylock 실패 -> back-off -> 재시도를 반복하며 실질적인 진전이 없을 수 있습니다. 이를 완화하기 위해 랜덤 대기 시간을 사용합니다.
- **성능 오버헤드**: 불필요한 lock/unlock 반복과 대기 시간이 발생합니다.
- **복잡성**: lock ordering에 비해 코드가 더 복잡합니다.

따라서 가능하면 lock ordering이 더 선호되는 방법입니다.

</details>

---

## Exercise 4: xv6 커널의 Lock Ordering 분석 (~15분)

### 목표
xv6 커널 소스 코드에서 deadlock 방지를 위한 lock ordering이 어떻게 적용되어 있는지 분석합니다.

### 4-1. xv6의 주요 Lock들

xv6 커널에는 다음과 같은 주요 lock들이 있습니다:

| Lock | 위치 | 용도 |
|------|------|------|
| `p->lock` | `kernel/proc.h` | 프로세스별 lock (state, killed, pid 등 보호) |
| `wait_lock` | `kernel/proc.c` | 부모-자식 관계(`p->parent`) 보호 |
| `bcache.lock` | `kernel/bio.c` | 버퍼 캐시 관리 |
| `ftable.lock` | `kernel/file.c` | 파일 테이블 관리 |
| `pi->lock` | `kernel/pipe.c` | 파이프별 lock |
| `cons.lock` | `kernel/console.c` | 콘솔 입출력 |
| `tickslock` | `kernel/trap.c` | 시스템 타이머 |

### 4-2. 핵심 Lock Ordering 규칙: `wait_lock` -> `p->lock`

`kernel/proc.c`의 26~27행 주석을 읽어보세요:

```c
// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;
```

**규칙: `wait_lock`은 항상 `p->lock`보다 먼저 획득해야 합니다.**

이 규칙이 실제로 지켜지는 코드를 확인해 봅시다.

### 실습: `kexit()` 함수 분석

`kernel/proc.c`의 `kexit()` 함수 (327행~)를 읽어보세요:

```c
void
kexit(int status)
{
    struct proc *p = myproc();
    ...
    acquire(&wait_lock);       // (1) wait_lock을 먼저 획득

    reparent(p);               // 자식 프로세스를 init에게 넘김
    wakeup(p->parent);         // 부모 깨우기

    acquire(&p->lock);         // (2) 그 다음 p->lock 획득

    p->xstate = status;
    p->state = ZOMBIE;

    release(&wait_lock);       // wait_lock 해제

    sched();                   // 스케줄러로 전환
    ...
}
```

> **질문 6**: `kexit()`에서 왜 `wait_lock`을 `p->lock`보다 먼저 잡아야 할까요? 순서를 바꾸면 어떤 문제가 생길 수 있나요?

<details>
<summary>정답 보기</summary>

`kwait()` 함수를 보면:
```c
int kwait(uint64 addr)
{
    acquire(&wait_lock);        // (1) wait_lock 먼저
    ...
    acquire(&pp->lock);         // (2) 자식의 p->lock 나중에
    ...
}
```

`kwait()`도 `wait_lock` -> `p->lock` 순서로 잡습니다. 만약 `kexit()`에서 `p->lock` -> `wait_lock` 순서로 잡으면:

- `kexit()`: `p->lock` 보유 -> `wait_lock` 대기
- `kwait()`: `wait_lock` 보유 -> `pp->lock` 대기

이 경우 순환 대기(Circular Wait)가 형성되어 deadlock이 발생할 수 있습니다.

</details>

### 4-3. `sleep()` 함수의 Lock Ordering

`kernel/proc.c`의 `sleep()` 함수 (543행~)를 읽어보세요:

```c
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    acquire(&p->lock);    // (1) p->lock 획득
    release(lk);          // (2) 조건 lock 해제

    p->chan = chan;
    p->state = SLEEPING;

    sched();              // 스케줄러로 전환

    p->chan = 0;

    release(&p->lock);    // p->lock 해제
    acquire(lk);          // 조건 lock 재획득
}
```

> **질문 7**: `sleep()`에서 왜 `p->lock`을 먼저 잡고, 그 다음에 조건 lock(`lk`)을 해제할까요? 순서를 바꾸면 (먼저 `lk`를 해제하고, 그 다음 `p->lock`을 잡으면) 어떤 문제가 생길까요?

<details>
<summary>정답 보기</summary>

만약 `release(lk)` 후에 `acquire(&p->lock)`을 하면, 두 동작 사이에 다른 프로세스가 `wakeup()`을 호출할 수 있습니다.

```
sleep():                           wakeup():
  release(lk)                        acquire(&p->lock)
  // ---- 이 사이에 wakeup 호출! ---- // p->state == SLEEPING? 아직 아님!
  acquire(&p->lock)                  release(&p->lock)
  p->state = SLEEPING
  sched()    // <- wakeup을 놓침! 영원히 잠듦
```

`p->lock`을 먼저 잡으면, `wakeup()`이 `p->lock`을 잡지 못해 대기하게 되고, `sleep()`이 `SLEEPING` 상태로 전환한 후에야 `wakeup()`이 상태를 확인하므로 wakeup 신호를 놓치지 않습니다. 이것은 "lost wakeup" 문제를 방지하기 위한 것입니다.

</details>

### 4-4. 프로세스 kill과 Locking 문제 (Ch 7.9)

xv6의 `kkill()` 함수(`kernel/proc.c` 593행~)는 매우 신중하게 설계되어 있습니다:

```c
int
kkill(int pid)
{
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->pid == pid){
            p->killed = 1;
            if(p->state == SLEEPING){
                p->state = RUNNABLE;     // 잠든 프로세스를 깨움
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return -1;
}
```

`kkill()`은 대상 프로세스를 **즉시 종료하지 않습니다**. 대신 `p->killed = 1`로 플래그만 설정합니다.

> **질문 8**: `kkill()`이 대상 프로세스를 즉시 종료하지 않는 이유를 locking 관점에서 설명해 보세요.

<details>
<summary>정답 보기</summary>

대상 프로세스가 커널 코드를 실행 중이라면 여러 lock을 보유하고 있을 수 있습니다. 예를 들어:
- 파일 시스템 lock을 보유한 채 디스크 I/O 대기 중
- 파이프 lock을 보유한 채 데이터 대기 중
- `wait_lock`을 보유한 채 작업 수행 중

이 상태에서 즉시 종료하면 **해당 lock들이 영원히 해제되지 않아** 다른 프로세스들이 deadlock에 빠질 수 있습니다.

따라서 `kkill()`은 플래그만 설정하고, 대상 프로세스가 **사용자 공간으로 돌아가는 시점**(trap 처리 후)에 스스로 안전하게 종료하도록 합니다. `usertrap()` 함수(`kernel/trap.c`)에서 확인할 수 있습니다:

```c
uint64 usertrap(void)
{
    ...
    if(killed(p))        // 시스템 콜 전 확인
        kexit(-1);
    ...
    if(killed(p))        // 시스템 콜 후 확인
        kexit(-1);
    ...
}
```

또한 `piperead()`나 `consoleread()` 같은 함수에서도 `killed()` 검사를 수행합니다:

```c
// pipe.c - piperead()
while(pi->nread == pi->nwrite && pi->writeopen){
    if(killed(pr)){
        release(&pi->lock);    // pipe lock을 안전하게 해제
        return -1;
    }
    sleep(&pi->nread, &pi->lock);
}
```

이렇게 하면 프로세스가 보유한 lock을 모두 정리한 후에 종료하므로 deadlock이 발생하지 않습니다.

</details>

### 4-5. acquire()의 Deadlock 방지 기능

`kernel/spinlock.c`의 `acquire()` 함수를 보면:

```c
void
acquire(struct spinlock *lk)
{
    push_off();             // 인터럽트 비활성화!
    if(holding(lk))
        panic("acquire");   // 같은 lock을 두 번 잡으면 panic!
    ...
}
```

> **질문 9**: `acquire()`에서 인터럽트를 비활성화하는 이유는 무엇인가요? 인터럽트를 비활성화하지 않으면 어떤 deadlock이 발생할 수 있나요?

<details>
<summary>정답 보기</summary>

만약 인터럽트가 활성화된 상태에서 spinlock을 잡으면:

1. CPU가 spinlock A를 잡음
2. 타이머 인터럽트 발생 -> 인터럽트 핸들러 실행
3. 인터럽트 핸들러가 spinlock A를 잡으려고 시도
4. **같은 CPU**에서 이미 A를 잡고 있으므로 영원히 spin -> Deadlock!

이것은 단일 CPU에서도 발생하는 deadlock입니다. `push_off()`로 인터럽트를 비활성화하면 lock 보유 중에 인터럽트 핸들러가 실행되지 않아 이 문제를 방지합니다.

또한 `holding(lk)` 검사는 같은 lock을 재귀적으로 잡는 프로그래밍 실수를 감지합니다.

</details>

---

## 요약 및 핵심 정리

### Deadlock 4가지 조건과 해결 방법

| 조건 | 해결 방법 | 이번 실습에서의 예시 |
|------|----------|---------------------|
| Mutual Exclusion | (일반적으로 제거 불가) | - |
| Hold & Wait | trylock + back-off | Exercise 3: `deadlock_fix_trylock.c` |
| No Preemption | (일반적으로 제거 불가) | - |
| Circular Wait | **Lock ordering** | Exercise 2: `deadlock_fix_ordering.c` |

### xv6의 Deadlock 방지 전략

1. **Lock ordering**: 전역적 lock 순서를 정의하고 항상 준수
   - 예: `wait_lock` -> `p->lock`
   - 예: `bcache.lock` -> `buf->lock` (spinlock -> sleeplock)

2. **인터럽트 비활성화**: spinlock 보유 중 인터럽트를 끔
   - `acquire()` -> `push_off()` -> `intr_off()`

3. **지연된 kill**: 프로세스를 즉시 종료하지 않고 플래그만 설정
   - lock을 안전하게 정리할 수 있는 시점에서 종료

4. **Holding 검사**: 같은 lock을 중복 획득하려 하면 panic
   - `if(holding(lk)) panic("acquire");`

### 핵심 교훈

- **Lock ordering은 가장 실용적이고 널리 사용되는 deadlock 방지 기법**입니다.
- xv6 커널은 코드 주석과 컨벤션으로 lock 순서를 문서화합니다.
- 프로세스 kill처럼 복잡한 상황에서는 **lock을 안전하게 정리할 수 있는 시점까지 동작을 지연**시키는 것이 중요합니다.
- 실제 운영체제(Linux 등)에서도 이러한 원리가 동일하게 적용됩니다.

# Week 4 Lab: Thread & Concurrency 1 - Race Conditions and Locks

> 수업 중 실습 (~50분)
> 교재: xv6 Chapter 5-6 (Interrupts, Locking)

## 학습 목표

이번 실습을 통해 다음을 이해합니다:

- 여러 스레드가 공유 자원에 동시 접근할 때 발생하는 **race condition**
- **mutex**를 사용하여 critical section을 보호하는 방법
- xv6 커널에서 사용하는 **spinlock**의 원리와 구현
- **Deadlock**이 발생하는 조건과 재현

## 사전 준비

```bash
# 실습 디렉터리로 이동
cd examples/

# 모든 예제 한 번에 컴파일
gcc -Wall -pthread -o race_demo race_demo.c
gcc -Wall -pthread -o mutex_fix mutex_fix.c
gcc -Wall -pthread -o spinlock_impl spinlock_impl.c
gcc -Wall -pthread -o deadlock_demo deadlock_demo.c
```

---

## 실습 1: Race Condition 관찰 (~12분)

### 배경

여러 스레드가 하나의 공유 변수(`counter`)를 동시에 증가시킬 때, 동기화 없이 실행하면 결과가 예상과 다릅니다.

`counter++`는 한 줄이지만 CPU 입장에서는 세 단계입니다:
```
1. LOAD  counter → 레지스터    (메모리에서 값 읽기)
2. ADD   1 → 레지스터          (레지스터에서 +1)
3. STORE 레지스터 → counter    (결과를 메모리에 쓰기)
```

두 스레드가 동시에 LOAD하면, 같은 값을 읽고 각각 +1 한 뒤 저장합니다.
결과적으로 2가 증가해야 할 것이 1만 증가합니다.

### 실행

```bash
# 기본 실행: 4 threads, 각 1,000,000 increments
./race_demo

# thread 수를 바꿔보기
./race_demo 2 1000000
./race_demo 8 1000000

# 여러 번 실행하여 결과가 매번 다른지 확인
for i in 1 2 3 4 5; do ./race_demo 4 1000000; echo "---"; done
```

### 확인 사항

- [ ] Expected와 Actual 값의 차이를 확인했는가?
- [ ] 실행할 때마다 결과가 달라지는가? (비결정적 동작)
- [ ] thread 수를 늘리면 손실되는 increment가 증가하는가?

### 질문

1. `counter++`가 한 줄인데 왜 race condition이 발생하는가?
2. 매 실행마다 결과가 다른 이유는 무엇인가?
3. thread 수가 1일 때는 race condition이 발생하는가?

---

## 실습 2: Mutex로 보호하기 (~10분)

### 배경

`pthread_mutex_t`로 critical section을 보호하면 한 번에 하나의 스레드만 `counter++`를 실행합니다. 나머지 스레드는 `pthread_mutex_lock()`에서 대기(sleep)합니다.

### 실행

```bash
# mutex 버전 실행
./mutex_fix

# race_demo와 동일한 파라미터로 비교
./mutex_fix 4 1000000
./mutex_fix 8 1000000
```

### 코드 비교

`race_demo.c`와 `mutex_fix.c`를 나란히 열어 차이점을 확인하세요:

```bash
diff race_demo.c mutex_fix.c
```

핵심 차이:
```c
// race_demo.c (보호 없음)
counter++;

// mutex_fix.c (mutex로 보호)
pthread_mutex_lock(&lock);
counter++;
pthread_mutex_unlock(&lock);
```

### 확인 사항

- [ ] 결과가 항상 Expected와 일치하는가?
- [ ] 여러 번 실행해도 항상 동일한 결과인가?
- [ ] 실행 시간이 `race_demo`보다 느린가? 왜 그런가?

### 질문

1. mutex를 사용하면 왜 느려지는가?
2. critical section을 최대한 작게 만들어야 하는 이유는?
3. `pthread_mutex_lock()`에서 대기 중인 스레드는 CPU를 사용하는가?

---

## 실습 3: Spinlock 구현 (~15분)

### 배경

xv6 커널은 `pthread_mutex` 대신 **spinlock**을 사용합니다.
Spinlock은 lock을 얻을 때까지 CPU에서 busy-wait(반복 확인)합니다.

xv6의 `kernel/spinlock.c` acquire 함수:
```c
void acquire(struct spinlock *lk)
{
    push_off(); // disable interrupts
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;
    __sync_synchronize();
}
```

`__sync_lock_test_and_set(&lk->locked, 1)`는 원자적으로:
1. `lk->locked`의 현재 값을 읽고
2. `lk->locked`를 1로 설정하고
3. 이전 값을 반환합니다

이전 값이 0이면 lock을 획득한 것이고, 1이면 다른 스레드가 보유 중이므로 계속 spin합니다.

### 실행

```bash
# spinlock 버전 실행
./spinlock_impl

# mutex 버전과 비교
time ./mutex_fix 4 1000000
time ./spinlock_impl 4 1000000
```

### 코드 분석

`spinlock_impl.c`를 열어 다음 함수를 분석하세요:

- `spinlock_init()`: lock 초기화
- `spinlock_acquire()`: `__sync_lock_test_and_set` 사용
- `spinlock_release()`: `__sync_lock_release` 사용

### 확인 사항

- [ ] spinlock으로도 결과가 올바른가?
- [ ] mutex와 spinlock의 성능 차이를 측정했는가?
- [ ] `__sync_lock_test_and_set`의 동작을 설명할 수 있는가?

### 질문

1. Spinlock과 mutex의 차이점은 무엇인가? (대기 방식, CPU 사용량)
2. xv6가 mutex 대신 spinlock을 사용하는 이유는?
3. `__sync_synchronize()`(memory barrier)는 왜 필요한가?
4. 유저 프로그램에서 spinlock을 사용하면 어떤 문제가 있는가?

---

## 실습 4: Deadlock 시나리오 (~13분)

### 배경

**Deadlock**은 두 개 이상의 스레드가 서로가 보유한 lock을 기다리며 영원히 멈추는 상태입니다.

Deadlock의 4가지 조건 (Coffman conditions):
1. **Mutual exclusion**: 자원은 한 번에 하나의 스레드만 사용
2. **Hold and wait**: lock을 보유한 채 다른 lock을 대기
3. **No preemption**: 다른 스레드의 lock을 강제로 빼앗을 수 없음
4. **Circular wait**: A→B→A 형태의 순환 대기

이 예제에서는:
- Thread A: `lock1` 획득 → `lock2` 대기
- Thread B: `lock2` 획득 → `lock1` 대기

### 실행

```bash
# deadlock 데모 (프로그램이 멈출 수 있음, Ctrl+C로 종료)
./deadlock_demo

# 여러 번 실행하여 deadlock 발생을 관찰
# (매번 발생하지 않을 수 있음 - 타이밍 의존)
```

### 분석

프로그램이 멈추면 deadlock이 발생한 것입니다. 다른 터미널에서 확인:
```bash
# macOS
ps aux | grep deadlock_demo

# 강제 종료
kill -9 $(pgrep deadlock_demo)
```

### 해결 방법

Deadlock을 방지하는 가장 간단한 방법: **모든 스레드가 lock을 같은 순서로 획득**

```c
// Thread A: lock1 -> lock2 (현재)
// Thread B: lock2 -> lock1 (현재 - deadlock 가능!)

// 수정: 두 스레드 모두 lock1 -> lock2 순서로 획득
// Thread A: lock1 -> lock2
// Thread B: lock1 -> lock2  ← 순서를 통일!
```

### 도전 과제 (선택)

`deadlock_demo.c`를 수정하여:
1. Thread B의 lock 획득 순서를 Thread A와 동일하게 변경
2. 수정 후 deadlock이 발생하지 않는지 확인

### 확인 사항

- [ ] Deadlock이 발생하여 프로그램이 멈추는 것을 관찰했는가?
- [ ] Coffman conditions 4가지를 이 예제에서 식별할 수 있는가?
- [ ] Lock 순서를 통일하면 deadlock이 해결되는지 확인했는가?

### 질문

1. xv6에서는 deadlock을 어떻게 방지하는가? (힌트: lock ordering)
2. Deadlock이 항상 발생하지 않는 이유는?
3. `usleep(100000)`을 제거하면 deadlock 발생 확률이 어떻게 변하는가?

---

## 정리 및 핵심 요약

| 개념 | 설명 | xv6 관련 |
|------|------|----------|
| Race condition | 동기화 없는 공유 자원 접근 시 비결정적 결과 | 모든 공유 자료구조에 lock 필요 |
| Mutex | Sleep 기반 lock, 대기 시 CPU 양보 | 유저 프로그램용 |
| Spinlock | Busy-wait 기반 lock, 대기 시 CPU 점유 | xv6 커널의 기본 lock |
| Deadlock | 순환 대기로 모든 스레드가 영원히 멈춤 | Lock ordering으로 방지 |

### xv6 코드에서 확인할 곳

- `kernel/spinlock.h`: spinlock 구조체 정의
- `kernel/spinlock.c`: `acquire()`, `release()` 구현
- `kernel/kalloc.c`: 메모리 할당기의 spinlock 사용 예시
- `kernel/bio.c`: 버퍼 캐시의 lock 사용 예시

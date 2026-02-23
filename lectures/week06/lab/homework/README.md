# Week 6 과제: User-Level Thread Library (uthread)

> 마감: 다음 수업 시작 전
> 난이도: ★★★☆☆
> 유형: 필수 과제

## 개요

이 과제에서는 **사용자 수준 스레드 라이브러리**를 구현한다. xv6 커널의 `swtch`/`scheduler`가 커널 수준에서 프로세스를 전환하는 것처럼, 여러분은 사용자 공간에서 협력적(cooperative) 스레드 전환을 구현할 것이다.

MIT 6.S081의 "Multithreading" Lab에서 영감을 받은 과제로, xv6가 아닌 **macOS/Linux** 환경에서 동작하는 스레드 라이브러리를 만든다.

## 학습 목표

- context switching의 핵심 원리를 직접 구현하며 이해한다.
- `ucontext` API (`getcontext`, `makecontext`, `swapcontext`, `setcontext`)를 활용한 context 관리를 학습한다.
- 프로세스/스레드의 상태 전이(state transition)를 코드로 표현한다.
- round-robin 스케줄링을 직접 구현한다.

## 배경 지식

### Context Switch란?

CPU가 현재 실행 중인 스레드의 **레지스터 상태**(PC, SP, 범용 레지스터 등)를 저장하고, 다음에 실행할 스레드의 저장된 상태를 복원하는 과정이다. xv6에서는 `swtch.S`가 이 역할을 하며, 이 과제에서는 POSIX `ucontext` API가 동일한 역할을 한다.

### ucontext API

```c
#include <ucontext.h>

int  getcontext(ucontext_t *ucp);         // 현재 context를 저장
void makecontext(ucontext_t *ucp,         // context의 시작 함수 설정
                 void (*func)(), int argc, ...);
int  swapcontext(ucontext_t *oucp,        // 현재 context 저장 + 새 context로 전환
                 const ucontext_t *ucp);
int  setcontext(const ucontext_t *ucp);   // 새 context로 전환 (저장 없이)
```

**핵심 함수 설명:**

| 함수 | 역할 | xv6 대응 |
|------|------|----------|
| `getcontext` | 현재 실행 상태를 `ucontext_t`에 저장 | `swtch`의 `sd` (save) 부분 |
| `makecontext` | context의 시작점(함수)과 스택을 설정 | `allocproc`에서 `context.ra = forkret` 설정 |
| `swapcontext` | 현재 상태 저장 + 다른 context로 전환 | `swtch(old, new)` |
| `setcontext` | 다른 context로 전환 (저장 없이) | 프로세스 종료 후 스케줄러로 점프 |

### 스레드 상태 전이

```
            thread_create
  T_FREE ─────────────────> T_RUNNABLE
                              │    ▲
                   scheduler  │    │  thread_yield
                   dispatch   │    │
                              ▼    │
                            T_RUNNING
                              │
                   thread_exit│
                              ▼
                            T_EXITED
```

## 파일 구조

```
homework/
├── README.md           ← 이 파일
├── skeleton/
│   ├── uthread.h       ← 스레드 구조체, 상수 정의 (수정 불필요)
│   ├── uthread.c       ← TODO 부분을 구현하세요
│   └── main.c          ← 테스트 프로그램 (수정 불필요)
├── solution/
│   └── uthread_solution.c  ← 정답 (과제 제출 후 공개)
└── tests/
    └── test_uthread.sh ← 자동 채점 스크립트
```

## 구현 요구사항

`skeleton/uthread.c`의 `TODO` 부분을 구현하라. 구현해야 할 함수는 4개이다:

### 1. `thread_create()` -- context 초기화

```c
int thread_create(void (*func)(void *), void *arg)
```

새 스레드를 생성한다. 내부적으로:
- 빈 슬롯을 찾아 상태를 `T_RUNNABLE`로 설정 (이미 구현됨)
- **TODO**: `getcontext`로 context 초기화
- **TODO**: 스레드의 전용 스택(`t->stack`, 크기 `STACK_SIZE`)을 context에 설정
- **TODO**: `uc_link`를 `_sched_context` (스케줄러 context)로 설정
- **TODO**: `makecontext`로 시작 함수를 `_thread_wrapper(i)`로 설정

**힌트**: xv6의 `allocproc()`에서 `context.ra = forkret`, `context.sp = kstack + PGSIZE`를 설정하는 것과 유사하다.

### 2. `thread_yield()` -- CPU 양보

```c
void thread_yield(void)
```

현재 스레드가 자발적으로 CPU를 양보한다:
- **TODO**: 상태를 `T_RUNNABLE`로 변경
- **TODO**: `swapcontext`로 현재 상태를 저장하고 스케줄러로 전환

**힌트**: xv6의 `yield()`가 `state = RUNNABLE`로 바꾸고 `sched()` -> `swtch()`를 호출하는 것과 동일한 패턴이다.

### 3. `thread_schedule()` -- Round-Robin 스케줄러

```c
void thread_schedule(void)
```

실행할 다음 스레드를 선택하고 전환한다:
- **TODO**: `(current_thread + 1)`부터 순환하며 `T_RUNNABLE` 스레드를 찾는다
- **TODO**: 찾으면 `T_RUNNING`으로 변경하고 `swapcontext`로 전환
- **TODO**: 스레드가 yield/exit하여 돌아오면, 다시 다음 스레드를 찾는다
- **TODO**: 실행 가능한 스레드가 없으면 루프를 종료하고 반환

**힌트**: xv6의 `scheduler()` 함수와 거의 동일한 로직이다. `proc[]` 배열 대신 `threads[]` 배열을, `swtch()` 대신 `swapcontext()`를 사용한다.

### 4. `thread_exit()` -- 스레드 종료

```c
void thread_exit(void)
```

현재 스레드를 종료하고 스케줄러로 돌아간다:
- 상태를 `T_EXITED`로 변경 (이미 구현됨)
- **TODO**: `setcontext`로 스케줄러 context로 점프 (현재 상태 저장 불필요)

**힌트**: xv6에서 `exit()`가 상태를 `ZOMBIE`로 바꾸고 `sched()`를 호출하는 것과 유사하다.

## 빌드 및 실행

```bash
cd skeleton/
gcc -Wall -o uthread uthread.c main.c
./uthread
```

## 예상 출력

올바르게 구현하면 3개의 스레드가 **교대로(interleaved)** 실행된다:

```
=== User-Level Thread Test ===

Created threads: 1, 2, 3

[Alpha] iteration 0
[Beta] count = 0
[Gamma] step 1 of 5
[Alpha] iteration 1
[Beta] count = 10
[Gamma] step 2 of 5
[Alpha] iteration 2
[Beta] count = 20
[Gamma] step 3 of 5
[Alpha] iteration 3
[Beta] count = 30
[Gamma] step 4 of 5
[Alpha] iteration 4
[Beta] count = 40
[Gamma] step 5 of 5
[Alpha] done
[uthread] thread 1 (thread_1) exited
[Beta] done
[uthread] thread 2 (thread_2) exited
[Gamma] done
[uthread] thread 3 (thread_3) exited

=== All threads finished ===
```

핵심: Alpha, Beta, Gamma가 **순서대로 번갈아** 출력된다. 만약 Alpha가 5번 연속 출력된 후 Beta가 출력된다면, `thread_yield` 또는 `thread_schedule`의 구현에 문제가 있는 것이다.

## 자동 테스트

```bash
cd tests/
./test_uthread.sh ../skeleton
```

테스트 항목:
1. 컴파일 성공 여부 (`-Wall -Werror`)
2. 프로그램 정상 종료 여부 (무한 루프/segfault 확인)
3. 3개 스레드 모두 출력 생성 여부
4. 3개 스레드 모두 완료 여부
5. 교대 실행(interleaving) 여부
6. 스레드 종료 메시지 출력 여부

## 채점 기준

| 항목 | 배점 |
|------|------|
| `thread_create` 구현 (context 초기화) | 25% |
| `thread_yield` 구현 (context 저장 + 스케줄러 전환) | 20% |
| `thread_schedule` 구현 (round-robin + swapcontext) | 35% |
| `thread_exit` 구현 (setcontext로 스케줄러 복귀) | 10% |
| 코드 스타일 및 주석 | 10% |

## xv6와의 대응 관계

이 과제의 구조는 xv6 커널 스케줄러와 1:1로 대응된다:

| uthread (이 과제) | xv6 | 역할 |
|---|---|---|
| `thread_create` | `allocproc` | 새 스레드/프로세스 생성, context 초기화 |
| `thread_yield` | `yield` | 자발적 CPU 양보 |
| `thread_schedule` | `scheduler` | 다음 실행 대상 선택 및 전환 |
| `thread_exit` | `exit` | 스레드/프로세스 종료 |
| `swapcontext` | `swtch` | context 저장 + 복원 |
| `struct thread` | `struct proc` | 스레드/프로세스 상태 관리 |
| `threads[]` | `proc[]` | 전역 스레드/프로세스 테이블 |

## 주의사항

- `uthread.h`를 다른 헤더보다 **먼저** include 해야 한다 (`_XOPEN_SOURCE` 정의 때문).
- macOS에서 ucontext는 deprecated 경고가 나지만, 헤더에서 자동으로 억제된다.
- `makecontext`에 전달하는 인자는 반드시 `int` 타입이어야 한다 (포인터 전달 불가).
- 스택은 아래로 자라지만, `uc_stack.ss_sp`에는 스택의 **바닥**(가장 낮은 주소)을 전달한다. `makecontext`가 자동으로 스택 포인터를 올바르게 설정한다.

## 참고 자료

- xv6 textbook Chapter 7: Scheduling
- `man getcontext`, `man makecontext`, `man swapcontext`
- MIT 6.S081 Lab: Multithreading (https://pdos.csail.mit.edu/6.S081/)

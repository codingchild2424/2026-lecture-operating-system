# Week 6 Lab: CPU Scheduling - Context Switching 이해

> 소요 시간: ~50분
> 선수 지식: 프로세스 상태, 레지스터, 스택의 개념
> 준비물: xv6-riscv 빌드 환경 (QEMU + RISC-V toolchain)

## 학습 목표

- xv6의 context switch 메커니즘을 어셈블리 수준에서 이해한다.
- `scheduler()` 함수의 동작 원리를 파악하고, round-robin 스케줄링을 관찰한다.
- `sleep`/`wakeup` 메커니즘이 실제로 어떻게 프로세스를 전환하는지 추적한다.

---

## 실습 1: swtch.S 코드 분석 (~10분)

### 배경

xv6에서 context switch는 `swtch.S`라는 어셈블리 파일 하나로 구현된다. 이 함수는 현재 실행 중인 커널 스레드의 레지스터를 저장하고, 다음에 실행할 커널 스레드의 레지스터를 복원한다.

### context 구조체 확인

`kernel/proc.h`를 열어 `struct context`를 확인하자:

```c
// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
```

**질문**: 왜 `ra`(return address)와 `sp`(stack pointer), 그리고 callee-saved 레지스터(s0-s11)만 저장할까? caller-saved 레지스터(t0-t6, a0-a7)는 왜 저장하지 않을까?

**답**: `swtch`는 C 함수 호출 규약(calling convention)에 따라 호출된다. caller-saved 레지스터는 호출자(caller)가 이미 스택에 저장했으므로, `swtch`는 callee-saved 레지스터만 저장하면 된다. 이것은 RISC-V calling convention을 활용한 최적화이다.

### swtch.S 분석

`kernel/swtch.S`를 열어보자:

```asm
# Context switch
#
#   void swtch(struct context *old, struct context *new);
#
# Save current registers in old. Load from new.

.globl swtch
swtch:
        sd ra, 0(a0)        # old->ra = ra
        sd sp, 8(a0)        # old->sp = sp
        sd s0, 16(a0)       # old->s0 = s0
        sd s1, 24(a0)       # old->s1 = s1
        sd s2, 32(a0)       # old->s2 = s2
        sd s3, 40(a0)       # old->s3 = s3
        sd s4, 48(a0)       # old->s4 = s4
        sd s5, 56(a0)       # old->s5 = s5
        sd s6, 64(a0)       # old->s6 = s6
        sd s7, 72(a0)       # old->s7 = s7
        sd s8, 80(a0)       # old->s8 = s8
        sd s9, 88(a0)       # old->s9 = s9
        sd s10, 96(a0)      # old->s10 = s10
        sd s11, 104(a0)     # old->s11 = s11

        ld ra, 0(a1)        # ra = new->ra
        ld sp, 8(a1)        # sp = new->sp
        ld s0, 16(a1)       # s0 = new->s0
        ld s1, 24(a1)       # s1 = new->s1
        ld s2, 32(a1)       # s2 = new->s2
        ld s3, 40(a1)       # s3 = new->s3
        ld s4, 48(a1)       # s4 = new->s4
        ld s5, 56(a1)       # s5 = new->s5
        ld s6, 64(a1)       # s6 = new->s6
        ld s7, 72(a1)       # s7 = new->s7
        ld s8, 80(a1)       # s8 = new->s8
        ld s9, 88(a1)       # s9 = new->s9
        ld s10, 96(a1)      # s10 = new->s10
        ld s11, 104(a1)     # s11 = new->s11

        ret                  # pc = ra (new->ra로 점프)
```

### 핵심 이해 포인트

1. **`a0`** = 첫 번째 인자 = `old` context 포인터 (현재 프로세스)
2. **`a1`** = 두 번째 인자 = `new` context 포인터 (다음 프로세스)
3. **`sd`** (store doubleword): 레지스터 값을 메모리에 저장
4. **`ld`** (load doubleword): 메모리에서 레지스터로 값 복원
5. **`ret`**: `ra` 레지스터의 주소로 점프 -- 이때 `ra`는 이미 `new->ra`로 교체된 상태!

### 실습 과제

아래 다이어그램을 종이에 그려보자. 프로세스 A에서 프로세스 B로 전환되는 과정을 단계별로 표시하라.

```
프로세스 A (running)          scheduler              프로세스 B (runnable)
     |                          |                         |
     | yield()                  |                         |
     |   sched()                |                         |
     |     swtch(A->ctx,        |                         |
     |           cpu->ctx) ---->|                         |
     |                          | (A의 레지스터 저장됨)    |
     |                          |                         |
     |                          | scheduler loop...       |
     |                          | B를 발견 (RUNNABLE)     |
     |                          |                         |
     |                          | swtch(cpu->ctx,         |
     |                          |       B->ctx) --------->|
     |                          |                         | (B의 레지스터 복원됨)
     |                          |                         | ret -> B가 마지막으로
     |                          |                         |        sched()를 호출한
     |                          |                         |        지점으로 복귀
```

**질문**: `swtch`의 `ret` 명령어가 실행되면 어디로 점프하는가?

**답**: `new->ra`에 저장된 주소로 점프한다. 이전에 해당 프로세스가 `swtch`를 호출했던 지점 바로 다음, 즉 `sched()` 함수 내의 `swtch` 호출 다음 줄로 돌아간다. 새로 생성된 프로세스의 경우 `forkret()` 함수의 시작 주소로 점프한다 (`allocproc`에서 `p->context.ra = (uint64)forkret`으로 설정하기 때문).

---

## 실습 2: scheduler()에 printf 추가 (~15분)

### 목표

스케줄러가 어떤 순서로 프로세스를 선택하는지 직접 눈으로 확인한다.

### 패치 적용

제공된 `scheduler_trace.patch` 파일을 xv6-riscv 디렉토리에 적용한다:

```bash
cd /path/to/xv6-riscv
git apply /path/to/materials/week6/lab/scheduler_trace.patch
```

이 패치는 `scheduler()` 함수에 다음과 같은 코드를 추가한다:

```c
// kernel/proc.c - scheduler() 함수 내부

if(p->state == RUNNABLE) {
  // === 추가된 트레이스 코드 ===
  printf("[sched] cpu%d: switch to pid=%d name=%s\n",
         cpuid(), p->pid, p->name);
  // ===========================

  p->state = RUNNING;
  c->proc = p;
  swtch(&c->context, &p->context);
  c->proc = 0;
  found = 1;
}
```

### 패치를 직접 적용하지 않고 수동으로 수정하는 방법

패치 대신 직접 `kernel/proc.c`를 편집해도 된다. `scheduler()` 함수를 찾아서 `if(p->state == RUNNABLE)` 블록 안에 `printf`를 추가하면 된다:

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();
    intr_off();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // 여기에 추가!
        printf("[sched] cpu%d: switch to pid=%d name=%s\n",
               cpuid(), p->pid, p->name);

        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      asm volatile("wfi");
    }
  }
}
```

### 빌드 및 실행

```bash
make clean
make qemu
```

### 예상 출력

xv6가 부팅되면 다음과 같은 메시지가 나타난다:

```
[sched] cpu0: switch to pid=1 name=
[sched] cpu0: switch to pid=1 name=initcode
[sched] cpu0: switch to pid=1 name=init
[sched] cpu0: switch to pid=2 name=sh
[sched] cpu0: switch to pid=2 name=sh
...
```

**참고**: CPU가 여러 개인 경우 (기본 QEMU 설정에서 CPUS=3) 여러 CPU의 출력이 섞여서 나온다. 출력이 너무 많으면 `CPUS=1`로 빌드하자:

```bash
make clean
make CPUS=1 qemu
```

### 관찰 포인트

1. `pid=1`은 `init` 프로세스이다. 부팅 시 가장 먼저 스케줄링된다.
2. `init`이 `sh`(shell)를 fork하면 `pid=2`인 `sh`가 나타난다.
3. shell이 키 입력을 기다릴 때는 SLEEPING 상태이므로 스케줄링 되지 않는다.

### 실습 과제

`printf` 출력을 보면서 다음 질문에 답하라:

1. 부팅 직후 어떤 프로세스가 가장 먼저 스케줄링되는가?
2. shell 프롬프트가 나타난 후, 아무 키도 누르지 않으면 스케줄러 출력이 멈추는가? 왜 그런가?
3. `CPUS=1`일 때와 `CPUS=3`일 때 출력이 어떻게 다른가?

---

## 실습 3: Round-Robin 동작 확인 (~15분)

### 목표

여러 프로세스가 동시에 실행될 때 round-robin 스케줄링이 어떻게 동작하는지 확인한다.

### 테스트 방법

xv6 셸에서 다음 명령어를 실행하여 CPU-bound 프로세스 여러 개를 동시에 실행한다:

```
$ spin &; spin &; spin &
```

**참고**: `spin`은 무한 루프를 도는 간단한 프로그램이다. xv6에 `spin`이 없다면, 다음과 같은 간단한 사용자 프로그램을 만들 수 있다. (이 과정은 건너뛰고 `cat README`와 같이 이미 있는 명령어를 여러 개 백그라운드로 실행해도 된다.)

대안으로, 기존 명령어를 활용:

```
$ cat README &
$ cat README &
$ ls &
```

### 예상 출력

스케줄러 트레이스가 활성화된 상태에서, 여러 프로세스가 동시에 RUNNABLE일 때:

```
[sched] cpu0: switch to pid=3 name=spin
[sched] cpu0: switch to pid=4 name=spin
[sched] cpu0: switch to pid=5 name=spin
[sched] cpu0: switch to pid=3 name=spin
[sched] cpu0: switch to pid=4 name=spin
[sched] cpu0: switch to pid=5 name=spin
...
```

### 관찰 포인트

스케줄러는 `proc[]` 배열을 처음부터 끝까지 순회하면서 RUNNABLE 프로세스를 찾는다:

```c
for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
        // 이 프로세스를 실행
    }
    release(&p->lock);
}
```

이것이 **round-robin 스케줄링**의 가장 단순한 형태이다:
- 배열을 순서대로 순회
- RUNNABLE인 프로세스를 발견하면 실행
- 타이머 인터럽트에 의해 다시 스케줄러로 돌아옴
- 배열의 처음부터 다시 순회

### 실습 과제

1. 트레이스 출력에서 pid의 패턴을 관찰하라. 정말 순환(round-robin) 패턴인가?
2. 이 구현의 단점은 무엇인가? (힌트: `proc[]` 배열의 앞쪽에 있는 프로세스가 유리한가?)
3. **도전**: `scheduler()` 함수를 수정하여, 마지막으로 실행한 프로세스의 다음 위치부터 순회를 시작하도록 변경해 보라. (힌트: 마지막 인덱스를 기억하는 변수가 필요하다.)

---

## 실습 4: sleep/wakeup 동작 추적 (~10분)

### 목표

pipe를 통한 read가 블로킹될 때, `sleep`과 `wakeup`이 어떻게 동작하는지 추적한다.

### sleep/wakeup 메커니즘 이해

`kernel/proc.c`의 `sleep()` 함수를 살펴보자:

```c
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  // Go to sleep.
  p->chan = chan;          // 어떤 이벤트를 기다리는지 기록
  p->state = SLEEPING;    // 상태를 SLEEPING으로 변경

  sched();                // 스케줄러로 전환 (CPU 양보)

  // 여기서 깨어남 (wakeup에 의해)
  p->chan = 0;

  release(&p->lock);
  acquire(lk);
}
```

`wakeup()` 함수:

```c
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;  // 깨워서 다시 실행 가능하게
      }
      release(&p->lock);
    }
  }
}
```

### pipe에서의 sleep/wakeup

`kernel/pipe.c`의 `piperead()` 함수를 보자:

```c
int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  // 읽을 데이터가 없으면
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock);   // sleep! (채널: &pi->nread)
  }
  // ... 데이터 읽기 ...
  wakeup(&pi->nwrite);              // writer를 깨움
  release(&pi->lock);
  return i;
}
```

### 추적 실습

`sleep()` 함수에 다음 printf를 추가해 보자 (직접 수정):

```c
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  // 추가된 트레이스
  printf("[sleep] pid=%d name=%s chan=%p\n", p->pid, p->name, chan);

  p->chan = chan;
  p->state = SLEEPING;
  sched();
  p->chan = 0;

  // 추가된 트레이스
  printf("[wakeup] pid=%d name=%s\n", p->pid, p->name);

  release(&p->lock);
  acquire(lk);
}
```

### 테스트

xv6 셸에서 pipe를 사용하는 명령을 실행한다:

```
$ echo hello | cat
```

### 예상 출력

```
[sleep] pid=3 name=cat chan=0x________
[sched] cpu0: switch to pid=4 name=echo
[wakeup] pid=3 name=cat
[sched] cpu0: switch to pid=3 name=cat
hello
```

흐름 해석:
1. `cat`이 pipe에서 read하려 하지만, 아직 데이터가 없으므로 `sleep` 호출
2. 스케줄러가 `echo`를 실행
3. `echo`가 pipe에 "hello"를 write하고, `wakeup(&pi->nread)` 호출
4. `cat`이 깨어나서 데이터를 읽고 출력

### 실습 과제

1. 출력에서 `chan` 주소가 의미하는 것은 무엇인가?
2. `wakeup`이 호출된 후, 깨어난 프로세스가 바로 실행되는가? 아니면 스케줄러를 거쳐야 하는가?
3. `sleep`과 `wakeup`에서 lock의 역할은 무엇인가? (lost wakeup 문제와 연결하여 생각하라)

---

## 정리 및 핵심 요약

| 개념 | 핵심 포인트 |
|------|------------|
| **swtch.S** | callee-saved 레지스터만 저장/복원. `ret`으로 새 프로세스의 실행 지점으로 점프 |
| **scheduler()** | proc 배열을 순회하며 RUNNABLE 프로세스를 찾아 실행 (round-robin) |
| **yield/sched** | 프로세스가 CPU를 양보할 때 호출. 상태를 RUNNABLE로 바꾸고 swtch |
| **sleep/wakeup** | 조건 변수와 유사. 채널(chan) 기반으로 프로세스를 재우고 깨움 |

## 수정사항 되돌리기

실습이 끝나면 수정한 코드를 되돌려 놓자:

```bash
cd /path/to/xv6-riscv
git checkout kernel/proc.c
```

또는 패치를 역적용:

```bash
git apply -R /path/to/materials/week6/lab/scheduler_trace.patch
```

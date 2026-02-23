# Week 9 Lab: 동기화 도구와 예제 - sleep/wakeup, pipe

## 학습 목표

이번 실습을 마치면 다음을 이해할 수 있다:

1. xv6의 `sleep()`과 `wakeup()` 메커니즘이 어떻게 프로세스 간 조정(coordination)을 구현하는지 설명할 수 있다
2. pipe 구현에서 sleep/wakeup이 producer-consumer 패턴을 어떻게 지원하는지 분석할 수 있다
3. pipe를 이용한 프로세스 간 통신 프로그램을 xv6에서 작성할 수 있다
4. "lost wakeup" 문제가 왜 발생하며, sleep에 lock을 넘기는 것이 어떻게 이를 해결하는지 설명할 수 있다

## 배경 지식

### sleep/wakeup이란?

운영체제에서 프로세스가 어떤 조건이 충족될 때까지 기다려야 하는 상황은 매우 흔하다. 예를 들어:
- pipe에서 데이터가 도착하길 기다리는 reader
- pipe 버퍼에 공간이 생기길 기다리는 writer
- 자식 프로세스가 종료하길 기다리는 부모 프로세스

이때 busy waiting(조건을 계속 확인하는 루프)은 CPU를 낭비한다. xv6는 이를 해결하기 위해 **sleep/wakeup** 메커니즘을 제공한다:

- **`sleep(chan, lk)`**: 현재 프로세스를 `SLEEPING` 상태로 전환하고, 채널(`chan`)에서 깨워질 때까지 CPU를 양보한다. `lk`는 조건을 보호하는 lock이다.
- **`wakeup(chan)`**: 해당 채널에서 잠자는 모든 프로세스를 `RUNNABLE` 상태로 전환한다.

### 채널(channel) 개념

sleep/wakeup에서 "채널"은 특정 조건을 식별하는 임의의 주소값이다. xv6에서는 보통 관련된 데이터 구조체의 주소를 채널로 사용한다. 같은 채널에서 sleep한 프로세스들은 그 채널에 대한 wakeup이 호출될 때 모두 깨어난다.

---

## Exercise 1: sleep/wakeup 코드 분석 (15분)

### 1.1 `sleep()` 함수 분석

`kernel/proc.c`의 540~569번째 줄에 있는 `sleep()` 함수를 읽어보자:

```c
// kernel/proc.c, line 540-569

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}
```

### 관찰 질문

**Q1-1.** `sleep()`의 두 매개변수는 각각 무엇인가?

- `chan` (void *): sleep 채널. 프로세스가 어떤 이벤트를 기다리는지를 식별하는 주소값이다.
- `lk` (struct spinlock *): 조건(condition)을 보호하는 lock. sleep에 진입하기 전에 이 lock을 들고 있어야 한다.

**Q1-2.** `sleep()` 함수 안에서 lock이 교환되는 순서를 정리해 보자:

1. `acquire(&p->lock)` - 프로세스 자신의 lock을 획득
2. `release(lk)` - 조건 lock을 해제
3. (sleep 상태에서 깨어난 후)
4. `release(&p->lock)` - 프로세스 lock 해제
5. `acquire(lk)` - 조건 lock을 다시 획득

> **핵심**: `p->lock`을 먼저 획득한 후에 `lk`를 해제한다. 이 순서가 왜 중요한지는 Exercise 4에서 다시 다룬다.

**Q1-3.** `p->chan = chan`과 `p->state = SLEEPING`을 설정한 후 `sched()`를 호출한다. `sched()`는 무엇을 하는가?

> `sched()`는 현재 프로세스의 컨텍스트를 저장하고, CPU의 스케줄러 컨텍스트로 전환한다(context switch). 스케줄러는 다른 RUNNABLE 프로세스를 찾아 실행시킨다. 현재 프로세스는 누군가가 `wakeup(chan)`을 호출할 때까지 실행되지 않는다.

**Q1-4.** sleep에서 깨어난 후 `p->chan = 0`으로 채널을 초기화하는 이유는?

> 프로세스가 더 이상 특정 채널에서 기다리고 있지 않음을 표시하기 위해서이다. 이후 다른 wakeup 호출에 잘못 깨어나는 것을 방지한다.

### 1.2 `wakeup()` 함수 분석

`kernel/proc.c`의 571~587번째 줄에 있는 `wakeup()` 함수를 읽어보자:

```c
// kernel/proc.c, line 571-587

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}
```

### 관찰 질문

**Q1-5.** `wakeup()`은 프로세스 테이블 전체를 순회한다. 왜 `p != myproc()` 조건이 필요한가?

> 자기 자신을 깨우려고 시도하면 의미가 없다. 또한 자기 자신의 `p->lock`을 이미 가지고 있는 상태에서 다시 acquire하면 deadlock이 발생할 수 있다.

**Q1-6.** wakeup은 채널이 일치하는 **모든** 프로세스를 깨운다. 이것의 장단점은 무엇인가?

> - **장점**: 구현이 단순하고, wakeup이 깨워야 할 특정 프로세스를 알 필요가 없다.
> - **단점**: 실제로 진행할 수 있는 프로세스가 하나뿐이어도 모든 프로세스가 깨어난다("thundering herd" 문제). 깨어난 프로세스들은 조건을 다시 확인하고, 조건이 충족되지 않으면 다시 sleep해야 한다.

**Q1-7.** `wakeup()`에서 각 프로세스의 `p->lock`을 획득하는 이유는?

> `p->state`와 `p->chan`을 안전하게 읽고 수정하기 위해서이다. 다른 CPU에서 동시에 같은 프로세스의 상태를 변경하는 것을 방지한다.

---

## Exercise 2: pipe 구현 분석 (15분)

### 2.1 pipe 자료구조

`kernel/pipe.c`의 11~20번째 줄에 정의된 pipe 구조체를 살펴보자:

```c
// kernel/pipe.c, line 11-20

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read
  uint nwrite;    // number of bytes written
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
};
```

**Q2-1.** pipe 버퍼는 원형 버퍼(circular buffer)로 구현되어 있다. `nread`와 `nwrite`는 단조 증가하는 카운터다. 실제 버퍼 인덱스는 어떻게 계산하는가?

> `pi->data[pi->nwrite % PIPESIZE]`와 `pi->data[pi->nread % PIPESIZE]`처럼 모듈러 연산을 사용한다.

**Q2-2.** 버퍼가 가득 찬 조건은? 버퍼가 비어 있는 조건은?

> - **가득 참**: `nwrite == nread + PIPESIZE` (쓴 양이 읽은 양보다 PIPESIZE만큼 앞서 있음)
> - **비어 있음**: `nread == nwrite` (읽은 양이 쓴 양과 같음)

### 2.2 `pipewrite()` 분석

```c
// kernel/pipe.c, line 76-103

int
pipewrite(struct pipe *pi, uint64 addr, int n)
{
  int i = 0;
  struct proc *pr = myproc();

  acquire(&pi->lock);
  while(i < n){
    if(pi->readopen == 0 || killed(pr)){
      release(&pi->lock);
      return -1;
    }
    if(pi->nwrite == pi->nread + PIPESIZE){ //DOC: pipewrite-full
      wakeup(&pi->nread);
      sleep(&pi->nwrite, &pi->lock);
    } else {
      char ch;
      if(copyin(pr->pagetable, &ch, addr + i, 1) == -1)
        break;
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  wakeup(&pi->nread);
  release(&pi->lock);

  return i;
}
```

### 관찰 질문

**Q2-3.** 88번째 줄에서 버퍼가 가득 찼을 때 writer는 어떤 동작을 하는가? 단계별로 설명하라.

> 1. `wakeup(&pi->nread)` - reader를 깨운다 (데이터를 읽어서 공간을 만들도록)
> 2. `sleep(&pi->nwrite, &pi->lock)` - 채널 `&pi->nwrite`에서 sleep한다
>    - sleep 내부에서 `pi->lock`이 해제되어 reader가 진행할 수 있다
>    - reader가 데이터를 읽고 `wakeup(&pi->nwrite)`를 호출하면 writer가 깨어난다
> 3. 깨어나면 다시 while 루프 상단으로 돌아가 조건을 확인한다

**Q2-4.** `sleep(&pi->nwrite, &pi->lock)`에서 채널로 `&pi->nwrite`를 사용하는 이유는?

> pipe 구조체 안의 `nwrite` 필드의 주소를 채널로 사용한다. 이는 "writer가 쓸 수 있기를 기다림"을 식별하는 고유한 주소값이다. reader의 `wakeup(&pi->nwrite)` 호출과 매칭된다.

**Q2-5.** writer 루프가 끝난 후(99번째 줄) `wakeup(&pi->nread)`를 호출하는 이유는?

> 데이터를 쓴 후 reader에게 "읽을 데이터가 있다"고 알려주기 위해서이다. reader가 빈 버퍼에서 sleep하고 있을 수 있다.

### 2.3 `piperead()` 분석

```c
// kernel/pipe.c, line 105-134

int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  //DOC: pipe-empty
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock); //DOC: piperead-sleep
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(pi->nread == pi->nwrite)
      break;
    ch = pi->data[pi->nread % PIPESIZE];
    if(copyout(pr->pagetable, addr + i, &ch, 1) == -1) {
      if(i == 0)
        i = -1;
      break;
    }
    pi->nread++;
  }
  wakeup(&pi->nwrite);  //DOC: piperead-wakeup
  release(&pi->lock);
  return i;
}
```

### 관찰 질문

**Q2-6.** 113번째 줄의 while 조건을 분석하라. 두 가지 조건이 모두 참일 때 sleep하는 이유는?

> - `pi->nread == pi->nwrite`: 버퍼가 비어 있다 (읽을 데이터가 없다)
> - `pi->writeopen`: writer가 아직 열려 있다 (앞으로 데이터가 올 가능성이 있다)
>
> 두 조건이 모두 참이면, 아직 데이터가 올 수 있으므로 기다린다. 만약 `writeopen`이 0이면 더 이상 데이터가 올 수 없으므로 기다리지 않고 바로 0을 반환한다 (EOF).

**Q2-7.** 아래 표를 완성하여 piperead와 pipewrite의 sleep/wakeup 관계를 정리하라:

| 동작 | sleep 채널 | wakeup 채널 | 의미 |
|------|-----------|-------------|------|
| writer가 버퍼 full에서 대기 | `&pi->nwrite` | (reader가) `&pi->nwrite` | "쓸 공간 생김" |
| reader가 버퍼 empty에서 대기 | `&pi->nread` | (writer가) `&pi->nread` | "읽을 데이터 있음" |

**Q2-8.** `piperead()`에서 while 루프를 빠져나온 후 for 루프(120번째 줄)에서 `if(pi->nread == pi->nwrite) break;`를 다시 확인하는 이유는?

> while 루프에서 빠져나오는 경우는 두 가지이다:
> 1. 버퍼에 데이터가 있는 경우 (`pi->nread != pi->nwrite`)
> 2. writer가 닫힌 경우 (`!pi->writeopen`)
>
> 2번의 경우 버퍼에 데이터가 남아있을 수도, 없을 수도 있다. for 루프에서 데이터를 하나씩 읽다가 버퍼가 비면 중단해야 한다.

---

## Exercise 3: Producer-Consumer 프로그램 작성 (15분)

### 3.1 개요

pipe를 이용하여 producer-consumer 패턴을 구현하는 xv6 유저 프로그램을 작성한다.

- **Producer (부모 프로세스)**: pipe에 데이터를 쓴다
- **Consumer (자식 프로세스)**: pipe에서 데이터를 읽어 출력한다

### 3.2 코드 작성

`examples/producer_consumer.c` 파일을 참고하라. 아래는 핵심 구조이다:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fds[2];
  int pid;

  // pipe 생성
  if(pipe(fds) < 0){
    printf("pipe() failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    printf("fork() failed\n");
    exit(1);
  }

  if(pid == 0){
    // 자식 프로세스 (Consumer)
    close(fds[1]);  // write end 닫기
    // ... fds[0]에서 read ...
    close(fds[0]);
    exit(0);
  } else {
    // 부모 프로세스 (Producer)
    close(fds[0]);  // read end 닫기
    // ... fds[1]에 write ...
    close(fds[1]);
    wait(0);
    exit(0);
  }
}
```

### 실습 단계

**단계 1**: `examples/producer_consumer.c` 파일의 전체 코드를 읽고 이해한다.

**단계 2**: 이 프로그램을 xv6에 추가하여 실행해 본다.

xv6에 새로운 유저 프로그램을 추가하려면:

1. 소스 파일을 `user/` 디렉토리에 복사한다:
   ```
   cp practice/week9/lab/examples/producer_consumer.c user/producer_consumer.c
   ```

2. `Makefile`의 `UPROGS` 목록에 추가한다:
   ```makefile
   UPROGS=\
       ...
       $U/_producer_consumer\
   ```

3. xv6를 빌드하고 실행한다:
   ```
   make clean && make qemu
   ```

4. xv6 셸에서 실행한다:
   ```
   $ producer_consumer
   ```

**단계 3**: 출력 결과를 관찰한다.

예상 출력:
```
[Producer] Sending 5 messages through pipe...
[Producer] Sent: msg 0: hello from producer
[Producer] Sent: msg 1: hello from producer
[Producer] Sent: msg 2: hello from producer
[Producer] Sent: msg 3: hello from producer
[Producer] Sent: msg 4: hello from producer
[Producer] Done. Closing write end.
[Consumer] Reading from pipe...
[Consumer] Received: msg 0: hello from producer
[Consumer] Received: msg 1: hello from producer
[Consumer] Received: msg 2: hello from producer
[Consumer] Received: msg 3: hello from producer
[Consumer] Received: msg 4: hello from producer
[Consumer] Pipe closed by producer (read returned 0). Exiting.
```

> **참고**: fork() 후 부모와 자식의 실행 순서는 스케줄러에 따라 달라지므로, Producer와 Consumer의 메시지가 섞여서 출력될 수 있다. 이것은 정상이다.

### 관찰 질문

**Q3-1.** Consumer가 Producer보다 먼저 실행되면 어떻게 되는가?

> Consumer가 `read()`를 호출하면 pipe 버퍼가 비어 있으므로, 커널의 `piperead()` 안에서 `sleep(&pi->nread, &pi->lock)`이 호출된다. Consumer는 SLEEPING 상태가 되고, Producer가 데이터를 쓰면 `wakeup(&pi->nread)`로 깨어난다.

**Q3-2.** Producer가 `close(fds[1])`로 write end를 닫으면 Consumer의 `read()`는 어떤 값을 반환하는가?

> 0을 반환한다 (EOF). `piperead()`에서 `pi->writeopen`이 0이 되므로 while 루프를 빠져나오고, 버퍼에 데이터가 없으면 `i = 0`으로 for 루프를 건너뛰어 0을 반환한다.

**Q3-3.** Producer가 pipe 버퍼 크기(512 bytes)보다 큰 데이터를 한번에 쓰려 하면 어떻게 되는가?

> `pipewrite()`에서 버퍼가 가득 차면(`nwrite == nread + PIPESIZE`) writer는 sleep한다. Consumer가 데이터를 읽어 공간을 만들면 writer가 깨어나서 나머지를 쓴다. 따라서 데이터가 손실되지 않고 자동으로 흐름 제어(flow control)가 이루어진다.

---

## Exercise 4: Lost Wakeup 문제 이해 (5분)

### 4.1 Lost Wakeup이란?

"Lost wakeup"은 wakeup 신호가 유실되어 프로세스가 영원히 잠드는 버그이다. 이 문제는 sleep/wakeup을 사용할 때 lock을 올바르게 다루지 않으면 발생한다.

### 4.2 잘못된 구현 예시

만약 `sleep()`이 lock 매개변수를 받지 않는 단순한 구현이라고 가정하자:

```c
// 잘못된 sleep 구현 (lock 매개변수 없음)
void broken_sleep(void *chan)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->chan = chan;
  p->state = SLEEPING;
  sched();
  p->chan = 0;
  release(&p->lock);
}
```

이 구현으로 piperead를 작성하면:

```c
// 잘못된 piperead - lost wakeup 발생 가능!
int broken_piperead(struct pipe *pi, uint64 addr, int n)
{
  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){
    release(&pi->lock);        // (A) lock 해제
    // <-- 여기서 wakeup이 발생하면?! -->
    broken_sleep(&pi->nread);  // (B) sleep 진입
  }
  // ... read data ...
  release(&pi->lock);
}
```

### 4.3 문제 시나리오

다음 순서로 실행이 진행된다고 가정하자:

```
시간  Reader (CPU 0)                    Writer (CPU 1)
────  ────────────────────────────────  ────────────────────────────────
 1    버퍼 비어있음 확인 (조건 true)
 2    release(&pi->lock)  // (A)
 3                                      acquire(&pi->lock)
 4                                      데이터를 버퍼에 씀
 5                                      wakeup(&pi->nread)  // Reader는
 6                                      // 아직 SLEEPING이 아니므로 무시됨!
 7                                      release(&pi->lock)
 8    broken_sleep(&pi->nread) // (B)
 9    // 영원히 잠듦 - 깨워줄 사람이 없다!
```

**핵심 문제**: (A)에서 lock을 해제한 시점과 (B)에서 실제로 SLEEPING 상태가 되는 시점 사이에 **빈틈(gap)**이 존재한다. 이 빈틈에서 writer가 wakeup을 호출하면, reader는 아직 잠들지 않았으므로 wakeup이 무시된다. 이후 reader가 잠들면 깨워줄 사람이 없다.

### 4.4 xv6의 올바른 해결책

xv6의 `sleep()`은 이 문제를 다음과 같이 해결한다:

```c
// kernel/proc.c, line 540-569 (올바른 구현)
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);   // (1) 프로세스 lock 획득
  release(lk);         // (2) 조건 lock 해제

  p->chan = chan;
  p->state = SLEEPING; // (3) SLEEPING 상태 설정

  sched();             // (4) 스케줄러로 전환

  p->chan = 0;

  release(&p->lock);
  acquire(lk);
}
```

**해결 원리**:

1. `acquire(&p->lock)` 후에 `release(lk)`를 한다.
2. `p->lock`을 들고 있는 상태에서 `lk`를 해제하므로, writer가 `wakeup()`을 호출하더라도 `p->lock`을 획득할 수 없어 reader의 상태를 확인할 수 없다.
3. reader가 `p->state = SLEEPING`으로 상태를 완전히 설정하고 `sched()`로 CPU를 양보한 **후에야** `p->lock`이 해제된다 (스케줄러 내부에서).
4. 그때 비로소 writer의 `wakeup()`이 `p->lock`을 획득하고 reader를 깨울 수 있다.

즉, **"조건 확인 -> sleep 상태 전환"이 원자적(atomic)으로 이루어진다** (p->lock에 의해 보호됨).

```
시간  Reader (CPU 0)                    Writer (CPU 1)
────  ────────────────────────────────  ────────────────────────────────
 1    버퍼 비어있음 확인 (조건 true)
 2    acquire(&p->lock)
 3    release(&pi->lock)
 4    p->state = SLEEPING               acquire(&pi->lock)
 5    sched() -> p->lock 해제           데이터를 버퍼에 씀
 6                                      wakeup(&pi->nread)
 7                                        acquire(&p->lock) // 이제 성공
 8                                        p->state = RUNNABLE // 깨움!
 9                                        release(&p->lock)
```

### 관찰 질문

**Q4-1.** "lock을 sleep에 넘겨야 한다"는 규칙을 한 문장으로 요약하면?

> sleep은 조건 lock을 받아서, 자신이 SLEEPING 상태로 전환되는 것과 lock 해제가 원자적으로 이루어지도록 보장한다. 이를 통해 "조건 확인"과 "잠들기" 사이의 빈틈에서 wakeup이 유실되는 것을 방지한다.

**Q4-2.** `examples/wakeup_demo.c` 프로그램을 실행하여 pipe를 통한 blocking/wakeup 동작을 관찰해 보자.

xv6에 추가하는 방법은 Exercise 3과 동일하다:
1. `user/` 디렉토리에 소스 파일을 복사한다
2. Makefile의 `UPROGS`에 `$U/_wakeup_demo\`를 추가한다
3. `make clean && make qemu` 후 xv6 셸에서 `wakeup_demo`를 실행한다

---

## 요약 및 핵심 정리

### 1. sleep/wakeup 메커니즘

| 항목 | 설명 |
|------|------|
| `sleep(chan, lk)` | 현재 프로세스를 `chan` 채널에서 SLEEPING 상태로 전환. `lk`를 원자적으로 해제 |
| `wakeup(chan)` | `chan`에서 잠자는 모든 프로세스를 RUNNABLE로 전환 |
| 채널(channel) | 이벤트를 식별하는 임의의 주소값 (보통 관련 데이터의 주소) |

### 2. pipe에서의 sleep/wakeup 사용

| 상황 | 호출하는 함수 | sleep 채널 | 깨워주는 쪽 |
|------|-------------|-----------|-----------|
| reader가 빈 버퍼에서 대기 | `sleep(&pi->nread, &pi->lock)` | `&pi->nread` | writer의 `wakeup(&pi->nread)` |
| writer가 가득 찬 버퍼에서 대기 | `sleep(&pi->nwrite, &pi->lock)` | `&pi->nwrite` | reader의 `wakeup(&pi->nwrite)` |

### 3. Lost Wakeup 방지

- **문제**: 조건 확인과 sleep 사이에 wakeup이 발생하면 유실됨
- **해결**: `sleep()`에 조건 lock을 넘겨서, SLEEPING 상태 전환과 lock 해제를 원자적으로 수행
- **핵심 순서**: `acquire(p->lock)` -> `release(lk)` -> `SLEEPING` -> `sched()` -> (스케줄러가 `p->lock` 해제)

### 4. Producer-Consumer 패턴

- pipe는 커널 수준의 producer-consumer 구현이다
- 자동 흐름 제어: 버퍼가 가득 차면 producer가 sleep, 비면 consumer가 sleep
- EOF 처리: write end가 닫히면 reader의 `read()`가 0을 반환

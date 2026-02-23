# Week 3 과제: trace 시스템 콜 추가

> **마감**: 다음 수업 전까지
> **기반**: MIT 6.1810 Lab: System Calls
> **난이도**: 필수(trace) + 보너스(sysinfo)

---

## 개요

이번 과제에서는 xv6에 새로운 시스템 콜 `trace(int mask)`를 추가합니다.
이 과정을 통해 시스템 콜이 어떻게 등록되고, 유저 프로그램에서 커널까지 어떤 경로로 전달되는지를 직접 체험합니다.

---

## 과제 1: `trace` 시스템 콜 (필수)

### 기능 명세

`trace(int mask)` 시스템 콜은 호출한 프로세스에 대해 시스템 콜 추적(tracing)을 활성화합니다.

- **mask**: 추적할 시스템 콜의 비트마스크. 시스템 콜 번호 `n`을 추적하려면 `(1 << n)` 비트를 설정한다.
- 추적이 활성화된 시스템 콜이 호출될 때마다, 커널은 다음 형식으로 한 줄을 출력한다:

  ```
  <pid>: syscall <syscall_name> -> <return_value>
  ```

  예시:
  ```
  3: syscall fork -> 4
  4: syscall write -> 1
  ```

- `fork()`로 생성된 자식 프로세스는 부모의 trace mask를 **상속**한다.
- `trace(0)`을 호출하면 추적을 비활성화한다.

### 동작 예시

```c
// trace(1 << SYS_fork)를 호출하면 fork 시스템 콜을 추적
// fork가 호출될 때마다 로그 출력

$ trace 1 fork    // mask = (1 << SYS_fork) = 2
3: syscall fork -> 4

$ trace 5 read write  // mask = (1<<SYS_read)|(1<<SYS_write) = 0x10020
// read, write 호출 시마다 로그 출력
```

---

## 단계별 구현 가이드

### 수정해야 할 파일 목록

| 파일 | 수정 내용 |
|------|-----------|
| `kernel/syscall.h` | `SYS_trace` 번호 추가 (22번) |
| `user/usys.pl` | `trace` 엔트리 추가 |
| `user/user.h` | `trace()` 함수 선언 추가 |
| `kernel/proc.h` | `struct proc`에 `trace_mask` 필드 추가 |
| `kernel/sysproc.c` | `sys_trace()` 함수 구현 |
| `kernel/syscall.c` | syscalls 테이블에 추가, `syscall()`에 trace 로직 추가 |
| `kernel/proc.c` | `kfork()`에서 `trace_mask` 복사 |
| `Makefile` | 테스트 프로그램을 UPROGS에 추가 (선택) |

### Step 1: 시스템 콜 번호 등록

`kernel/syscall.h`에 새 시스템 콜 번호를 추가합니다.

```c
// kernel/syscall.h
// 기존 마지막 줄:
#define SYS_close  21

// 추가:
#define SYS_trace  22
```

### Step 2: 유저 공간 인터페이스

**`user/usys.pl`** 파일 끝에 엔트리를 추가합니다:

```perl
entry("trace");
```

이렇게 하면 빌드 시 `usys.S`에 다음과 같은 어셈블리가 생성됩니다:

```asm
.global trace
trace:
 li a7, SYS_trace
 ecall
 ret
```

**`user/user.h`**에 함수 선언을 추가합니다:

```c
// system calls 섹션에 추가
int trace(int);
```

### Step 3: 프로세스 구조체에 필드 추가

`kernel/proc.h`의 `struct proc`에 trace mask를 저장할 필드를 추가합니다.

```c
struct proc {
  // ... 기존 필드들 ...
  char name[16];               // Process name (debugging)
  int trace_mask;              // Trace syscall mask (추가)
};
```

### Step 4: `sys_trace()` 구현

`kernel/sysproc.c`에 `sys_trace()` 함수를 구현합니다.

이 함수는:
1. 첫 번째 인자 (mask)를 읽어온다 (`argint()` 사용)
2. 현재 프로세스의 `trace_mask` 필드에 저장한다

```c
uint64
sys_trace(void)
{
  // TODO:
  // 1. argint()로 첫 번째 인자(mask)를 읽어온다
  // 2. myproc()->trace_mask에 저장한다
  // 3. 성공 시 0을 반환한다
}
```

**힌트**: `sys_kill()` 함수를 참고하세요. `argint()`로 인자를 읽는 패턴이 동일합니다.

```c
// 참고: kernel/sysproc.c의 sys_kill()
uint64
sys_kill(void)
{
  int pid;
  argint(0, &pid);
  return kkill(pid);
}
```

### Step 5: syscall 테이블과 디스패치 로직 수정

**`kernel/syscall.c`**에서 세 가지를 수정합니다.

**(a)** 함수 프로토타입 추가:

```c
extern uint64 sys_trace(void);
```

**(b)** `syscalls[]` 배열에 항목 추가:

```c
static uint64 (*syscalls[])(void) = {
  // ... 기존 항목들 ...
  [SYS_close]   sys_close,
  [SYS_trace]   sys_trace,   // 추가
};
```

**(c)** 시스템 콜 이름 배열 추가 및 `syscall()` 함수에 trace 로직 추가:

```c
// syscall 이름 문자열 배열 (trace 출력용)
static char *syscall_names[] = {
  [SYS_fork]    "fork",
  [SYS_exit]    "exit",
  [SYS_wait]    "wait",
  [SYS_pipe]    "pipe",
  [SYS_read]    "read",
  [SYS_kill]    "kill",
  [SYS_exec]    "exec",
  [SYS_fstat]   "fstat",
  [SYS_chdir]   "chdir",
  [SYS_dup]     "dup",
  [SYS_getpid]  "getpid",
  [SYS_sbrk]    "sbrk",
  [SYS_pause]   "pause",
  [SYS_uptime]  "uptime",
  [SYS_open]    "open",
  [SYS_write]   "write",
  [SYS_mknod]   "mknod",
  [SYS_unlink]  "unlink",
  [SYS_link]    "link",
  [SYS_mkdir]   "mkdir",
  [SYS_close]   "close",
  [SYS_trace]   "trace",
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();

    // TODO: trace 로직 추가
    // p->trace_mask의 num번째 비트가 설정되어 있으면
    // printf로 "<pid>: syscall <name> -> <return_value>" 출력
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

**힌트**:
- 비트마스크 검사는 `(p->trace_mask & (1 << num))` 연산을 사용합니다.
- `p->trapframe->a0`는 `uint64` 타입이므로, `printf`의 `%d`와 함께 쓸 때 `(int)`로 캐스팅해야 합니다.
  (`-Werror` 플래그 때문에 타입 불일치 경고가 컴파일 에러로 처리됩니다.)

### Step 6: `fork()`에서 trace_mask 상속

`kernel/proc.c`의 `kfork()` 함수에서, 자식 프로세스가 부모의 `trace_mask`를 상속하도록 한 줄을 추가합니다.

```c
int
kfork(void)
{
  // ...
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // TODO: trace_mask를 부모에서 자식으로 복사
  // np->trace_mask = ???;

  // increment reference counts on open file descriptors.
  // ...
}
```

### Step 7: 프로세스 초기화 시 trace_mask 초기화

`kernel/proc.c`의 `freeproc()` 함수에서 `trace_mask`를 0으로 초기화해야 합니다.

```c
static void
freeproc(struct proc *p)
{
  // ... 기존 코드 ...
  p->state = UNUSED;
  // TODO: p->trace_mask = 0; 추가
}
```

---

## 테스트

### 테스트 프로그램

`trace_test.c`를 `user/` 디렉토리에 복사하고 `Makefile`의 `UPROGS`에 추가한 뒤 테스트합니다.
(과제 배포 파일에 `trace_test.c`가 포함되어 있습니다.)

```bash
# xv6 빌드 및 실행
make clean && make qemu

# xv6 셸에서 테스트
$ trace_test
```

### 예상 출력

```
=== Test 1: trace fork ===
<pid>: syscall fork -> <child_pid>
Test 1 PASSED

=== Test 2: trace read/write ===
(write/read trace 출력이 나타남)
Test 2 PASSED

=== Test 3: trace multiple syscalls ===
(여러 시스템 콜의 trace 출력)
Test 3 PASSED

=== Test 4: trace inheritance ===
(자식 프로세스에서도 trace 출력)
Test 4 PASSED

All tests passed!
```

### 수동 테스트

xv6 셸에서 직접 확인할 수도 있습니다:

```bash
# 간단한 프로그램 작성 후 테스트
$ trace_test
```

---

## 제출 방법

다음 중 하나로 제출:

**방법 1: patch 파일**
```bash
cd xv6-riscv
git diff > trace.patch
```

**방법 2: 수정된 파일들**

다음 파일들을 제출:
- `kernel/syscall.h`
- `kernel/syscall.c`
- `kernel/sysproc.c`
- `kernel/proc.h`
- `kernel/proc.c`
- `user/usys.pl`
- `user/user.h`

---

## 과제 2: `sysinfo` 시스템 콜 (보너스, +10점)

### 기능 명세

`sysinfo(struct sysinfo *)` 시스템 콜을 추가합니다. 이 시스템 콜은 시스템의 현재 상태 정보를 반환합니다.

### 구현할 내용

1. `kernel/sysinfo.h` 파일 생성:

```c
struct sysinfo {
  uint64 freemem;   // 사용 가능한 메모리 (바이트)
  uint64 nproc;     // 활성 프로세스 수 (state != UNUSED)
};
```

2. `SYS_sysinfo` 번호를 23으로 등록

3. 필요한 헬퍼 함수 구현:
   - `kernel/kalloc.c`에 `freemem_count()`: free list를 순회하며 빈 페이지 수 * PGSIZE 반환
   - `kernel/proc.c`에 `proc_count()`: `proc[]` 배열을 순회하며 UNUSED가 아닌 프로세스 수 반환

4. `kernel/sysproc.c`에 `sys_sysinfo()` 구현:
   - 유저 공간 포인터로 `struct sysinfo` 데이터를 복사 (`copyout()` 사용)

### 힌트

- `copyout()`을 사용하여 커널 데이터를 유저 공간으로 복사합니다
- `argaddr()`로 유저 포인터를 받아옵니다
- `sys_fstat()`의 구현 패턴을 참고하세요

---

## 채점 기준

| 항목 | 배점 |
|------|------|
| `trace` 시스템 콜이 정상 등록됨 (컴파일 성공) | 20점 |
| `sys_trace()` 구현 및 mask 저장 | 20점 |
| `syscall()`에서 trace 출력이 올바른 형식으로 나옴 | 30점 |
| `fork()` 시 trace_mask 상속이 동작 | 20점 |
| 코드 스타일 및 주석 | 10점 |
| (보너스) `sysinfo` 시스템 콜 | +10점 |

---

## 참고 자료

- xv6 교재 Chapter 2: Operating System Organization
- xv6 교재 Chapter 4: Traps and System Calls
- [MIT 6.1810 Lab: System Calls](https://pdos.csail.mit.edu/6.828/2024/labs/syscall.html)
- 수업 실습 (Week 3 Lab)에서 분석한 시스템 콜 호출 경로

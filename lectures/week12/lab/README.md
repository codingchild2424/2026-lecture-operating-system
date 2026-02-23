# Week 12 Lab: Virtual Memory - COW Fork & Lazy Allocation

## 학습 목표

이번 실습에서는 가상 메모리의 핵심 최적화 기법인 **Copy-On-Write (COW) Fork**와 **Lazy Allocation**을 학습한다. 이 두 기법은 모두 **page fault**를 의도적으로 활용하여 성능을 향상시키는 전략이다.

실습을 마치면 다음을 이해할 수 있다:
- RISC-V에서 page fault가 발생하고 처리되는 전체 흐름
- COW fork의 원리와 xv6에서의 구현 설계
- Lazy allocation의 원리와 xv6에서의 구현 설계
- PTE 플래그를 활용한 메모리 관리 기법

**참고 교재:** xv6 book Chapter 4.6

---

## 배경 지식: Page Fault

### Page Fault란?

프로세스가 유효하지 않은 가상 주소에 접근하면 MMU(Memory Management Unit)가 **page fault** 예외를 발생시킨다. "유효하지 않다"는 것은 다음과 같은 경우를 포함한다:

1. 해당 가상 주소에 대한 PTE(Page Table Entry)가 존재하지 않음 (PTE_V = 0)
2. PTE는 존재하지만, 접근 권한이 없음 (읽기 전용 페이지에 쓰기 시도 등)
3. 사용자 모드에서 커널 전용 페이지 접근 (PTE_U = 0)

### RISC-V scause 값 (Page Fault 관련)

RISC-V에서 trap 발생 시 `scause` 레지스터에 원인이 기록된다:

| scause 값 | 이름 | 설명 |
|-----------|------|------|
| **12** | Instruction page fault | 명령어 fetch 시 page fault |
| **13** | Load page fault | 메모리 읽기(load) 시 page fault |
| **15** | Store/AMO page fault | 메모리 쓰기(store) 시 page fault |

### stval 레지스터

page fault 발생 시 `stval` (Supervisor Trap Value) 레지스터에는 **fault를 발생시킨 가상 주소**가 저장된다. 커널은 이 값을 사용하여 어떤 페이지에서 fault가 발생했는지 알 수 있다.

### xv6의 PTE 플래그 (kernel/riscv.h)

```c
#define PTE_V (1L << 0) // valid - 유효한 PTE
#define PTE_R (1L << 1) // read  - 읽기 허용
#define PTE_W (1L << 2) // write - 쓰기 허용
#define PTE_X (1L << 3) // execute - 실행 허용
#define PTE_U (1L << 4) // user - 사용자 모드 접근 허용
```

RISC-V PTE 구조 (Sv39):
```
63    54 53    28 27    19 18    10 9  8 7 6 5 4 3 2 1 0
+------+--------+--------+--------+----+-+-+-+-+-+-+-+-+
|  (0) | PPN[2] | PPN[1] | PPN[0] | RSW|D|A|G|U|X|W|R|V|
+------+--------+--------+--------+----+-+-+-+-+-+-+-+-+
```

- **RSW** (비트 8-9): Reserved for Software - OS가 자유롭게 사용 가능한 비트
- 이 RSW 비트를 COW 표시에 활용할 수 있다

---

## Exercise 1: Page Fault 처리 흐름 추적 (10분)

### 목표
xv6에서 page fault가 발생했을 때 커널이 어떻게 처리하는지 코드를 따라가며 이해한다.

### 1.1 usertrap() 분석

`kernel/trap.c`의 `usertrap()` 함수를 살펴보자:

```c
uint64
usertrap(void)
{
  // ... (생략) ...

  struct proc *p = myproc();
  p->trapframe->epc = r_sepc();  // fault 발생 시점의 PC 저장

  if(r_scause() == 8){
    // system call 처리
    // ...
  } else if((which_dev = devintr()) != 0){
    // 디바이스 인터럽트 처리
  } else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // page fault 처리 (lazy allocation)
  } else {
    // 처리할 수 없는 예외 -> 프로세스 종료
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    setkilled(p);
  }
  // ...
}
```

**질문 1-1:** `usertrap()`에서 page fault를 감지하는 조건은 무엇인가?
- `r_scause()` 값이 `13` (load page fault) 또는 `15` (store page fault)일 때 page fault로 판단한다.

**질문 1-2:** page fault가 발생한 가상 주소는 어떻게 알 수 있는가?
- `r_stval()`을 호출하여 `stval` 레지스터에서 fault를 발생시킨 가상 주소를 읽는다.

**질문 1-3:** `scause == 12` (instruction page fault)는 왜 처리하지 않는가?

> **생각해보기:** 일반적인 프로그램 실행에서 instruction page fault가 발생할 수 있는 상황은 어떤 경우인가? xv6에서는 왜 이를 별도로 처리하지 않는 것이 합리적인가?

### 1.2 vmfault() 분석

현재 xv6에 구현된 `vmfault()` 함수를 분석하자 (`kernel/vm.c`):

```c
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();

  if (va >= p->sz)           // (1) 프로세스 주소 공간 범위 확인
    return 0;
  va = PGROUNDDOWN(va);      // (2) 페이지 경계로 정렬
  if(ismapped(pagetable, va)) {  // (3) 이미 매핑된 페이지인지 확인
    return 0;
  }
  mem = (uint64) kalloc();   // (4) 물리 페이지 할당
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);  // (5) 0으로 초기화
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);      // (6) 페이지 테이블에 매핑
    return 0;
  }
  return mem;
}
```

**질문 1-4:** `vmfault()`는 현재 어떤 기능을 수행하는가?

> 이 함수는 **lazy allocation**을 위한 page fault 핸들러이다. `sbrk()`로 프로세스 크기를 늘렸지만 아직 물리 메모리가 할당되지 않은 페이지에 접근하면, 그때서야 물리 메모리를 할당하고 매핑한다.

**질문 1-5:** `vmfault()`에서 처리하는 단계를 순서대로 설명하라.

| 단계 | 코드 | 설명 |
|------|------|------|
| (1) | `va >= p->sz` | fault 주소가 프로세스 메모리 범위 내인지 확인 |
| (2) | `PGROUNDDOWN(va)` | 가상 주소를 페이지 시작 주소로 정렬 |
| (3) | `ismapped()` | 이미 매핑된 페이지라면 처리할 필요 없음 (다른 원인) |
| (4) | `kalloc()` | 새 물리 페이지 할당 |
| (5) | `memset(0)` | 보안을 위해 0으로 초기화 |
| (6) | `mappages()` | 가상 주소 -> 물리 주소 매핑을 페이지 테이블에 추가 |

### 1.3 Page Fault 처리 전체 흐름도

```
사용자 프로그램: 메모리 접근 (예: *ptr = 42;)
        |
        v
   [MMU] PTE 검사 -> 유효하지 않음!
        |
        v
   [하드웨어] scause = 15 (store page fault)
              stval = fault 발생 가상 주소
              sepc = fault 발생 명령어 주소
        |
        v
   [trampoline.S] uservec -> 레지스터 저장
        |
        v
   [trap.c] usertrap()
        |--- scause == 8? --> syscall 처리
        |--- devintr()?    --> 디바이스 인터럽트 처리
        |--- scause == 13 or 15? --> vmfault() 호출
        |         |
        |         |--- 성공: 물리 메모리 할당 + 매핑
        |         |--- 실패: 프로세스 종료
        |
        v
   [trampoline.S] userret -> 사용자 모드로 복귀
                              (같은 명령어를 다시 실행 -> 이번에는 성공!)
```

**핵심 포인트:** page fault 처리 후 사용자 프로그램으로 돌아갈 때, `epc`가 fault를 발생시킨 명령어를 가리키고 있으므로 **같은 명령어가 다시 실행**된다. 이번에는 매핑이 완료되었으므로 정상적으로 수행된다. (시스템 콜의 경우 `epc += 4`로 다음 명령어를 가리키게 하지만, page fault에서는 그렇게 하지 않는다.)

---

## Exercise 2: Copy-On-Write (COW) Fork 설계 (20분)

### 2.1 현재 fork의 문제점

현재 xv6의 `kfork()` (`kernel/proc.c`)는 `uvmcopy()`를 호출하여 부모 프로세스의 **모든 페이지를 즉시 복사**한다:

```c
// kernel/vm.c - uvmcopy()
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)       // <-- 매 페이지마다 새 물리 메모리 할당
      goto err;
    memmove(mem, (char*)pa, PGSIZE); // <-- 4KB 전체 복사
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;
  // ...
}
```

**문제:** `fork()` 직후 `exec()`를 호출하는 경우 (예: shell에서 명령어 실행), 복사한 메모리를 곧바로 버리게 된다. 불필요한 복사가 성능을 크게 저하시킨다.

**질문 2-1:** 프로세스가 100개의 페이지(400KB)를 사용 중일 때 fork()를 호출하면, 현재 구현에서 몇 번의 `kalloc()`과 `memmove()`가 호출되는가?

> `kalloc()` 100번, `memmove()` 100번 (각각 4KB씩 총 400KB 복사)

### 2.2 COW Fork의 핵심 아이디어

```
[ 현재 fork ]                        [ COW fork ]

  부모 PT      자식 PT                 부모 PT      자식 PT
  +----+      +----+                  +----+      +----+
  | VA | -->  | VA | --> 새 물리 페이지   | VA | -+   | VA | -+
  +----+      +----+    (내용 복사)     +----+  |   +----+  |
                                              |           |
                                              +---> 같은 물리 페이지 <---+
                                                   (읽기 전용으로 공유)
```

**COW fork 동작:**
1. `fork()` 시: 페이지를 복사하지 않고, 부모와 자식이 **같은 물리 페이지를 공유**
2. 두 프로세스 모두 해당 페이지를 **읽기 전용**으로 설정
3. 어느 한쪽이 **쓰기를 시도**하면 store page fault 발생
4. page fault handler에서 **그때서야 페이지를 복사**하고, 쓰기 권한을 부여

### 2.3 COW Fork 구현 설계

#### 단계 1: COW 비트 정의

PTE의 RSW (Reserved for Software) 비트를 COW 표시에 사용한다.

`kernel/riscv.h`에 추가:
```c
#define PTE_COW (1L << 8)  // RSW 비트 중 하나를 COW 표시로 사용
```

**질문 2-2:** RSW 비트는 PTE의 몇 번째 비트인가? 하드웨어가 이 비트를 어떻게 처리하는가?

> RSW는 비트 8-9이다. 하드웨어(MMU)는 이 비트를 무시한다. 따라서 OS가 원하는 용도로 자유롭게 사용할 수 있다.

#### 단계 2: uvmcopy() 수정 (COW 버전)

기존 `uvmcopy()`를 다음과 같이 수정한다:

```c
// COW fork를 위한 uvmcopy() 수정안 (의사 코드)
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // [핵심 변경] 쓰기 가능한 페이지를 읽기 전용 + COW 표시로 변경
    if(flags & PTE_W) {
      flags = (flags & ~PTE_W) | PTE_COW;  // W 제거, COW 추가
      *pte = PA2PTE(pa) | flags;           // 부모의 PTE도 수정!
    }

    // 같은 물리 페이지를 자식 페이지 테이블에 매핑 (복사 없음!)
    if(mappages(new, i, PGSIZE, pa, flags) != 0)
      goto err;

    // 참조 카운트 증가 (아래에서 설명)
    krefpage((void*)pa);
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

**질문 2-3:** 부모의 PTE도 수정해야 하는 이유는 무엇인가?

> 부모가 먼저 쓰기를 시도할 수도 있기 때문이다. 부모의 PTE도 읽기 전용 + COW로 변경하여, 부모든 자식이든 먼저 쓰기를 시도하는 쪽에서 page fault가 발생하여 복사가 이루어지도록 해야 한다.

#### 단계 3: Page Fault Handler에서 COW 처리

`usertrap()` 또는 별도의 COW fault handler에서 다음 로직을 수행한다:

```c
// COW page fault 처리 의사 코드
int
cowfault(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa, flags;
  char *mem;

  va = PGROUNDDOWN(va);

  // 1. PTE 찾기
  pte = walk(pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0)
    return -1;

  // 2. COW 페이지인지 확인
  if((*pte & PTE_COW) == 0)
    return -1;  // COW가 아닌 페이지에 대한 write fault -> 진짜 오류

  pa = PTE2PA(*pte);
  flags = PTE_FLAGS(*pte);

  // 3. 참조 카운트 확인
  if(kgetrefcnt((void*)pa) == 1) {
    // 이 물리 페이지를 사용하는 프로세스가 나뿐이면 복사 불필요
    // PTE 플래그만 수정: COW 제거, W 복원
    *pte = PA2PTE(pa) | ((flags & ~PTE_COW) | PTE_W);
    return 0;
  }

  // 4. 새 물리 페이지 할당 및 복사
  if((mem = kalloc()) == 0)
    return -1;
  memmove(mem, (char*)pa, PGSIZE);

  // 5. PTE 업데이트: 새 물리 주소, COW 제거, W 복원
  *pte = PA2PTE(mem) | ((flags & ~PTE_COW) | PTE_W);

  // 6. 이전 물리 페이지의 참조 카운트 감소 (0이 되면 해제)
  kfree((void*)pa);

  return 0;
}
```

**질문 2-4:** `usertrap()`에서 COW fault를 어떻게 감지하는가?

> `scause == 15` (store page fault)이고, 해당 PTE에 `PTE_COW` 플래그가 설정되어 있으면 COW fault이다.

#### 단계 4: 참조 카운트 (Reference Count) 관리

COW에서는 하나의 물리 페이지를 여러 프로세스가 공유하므로, 물리 페이지가 더 이상 사용되지 않을 때만 해제해야 한다. 이를 위해 **참조 카운트**가 필요하다.

```c
// kernel/kalloc.c에 추가할 내용 (의사 코드)

struct {
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];  // 각 물리 페이지의 참조 카운트
} refcnt;

// 참조 카운트 증가
void krefpage(void *pa) {
  acquire(&refcnt.lock);
  refcnt.cnt[(uint64)pa / PGSIZE]++;
  release(&refcnt.lock);
}

// 참조 카운트 조회
int kgetrefcnt(void *pa) {
  int cnt;
  acquire(&refcnt.lock);
  cnt = refcnt.cnt[(uint64)pa / PGSIZE];
  release(&refcnt.lock);
  return cnt;
}

// kfree() 수정: 참조 카운트가 0일 때만 실제 해제
void kfree(void *pa) {
  acquire(&refcnt.lock);
  refcnt.cnt[(uint64)pa / PGSIZE]--;
  if(refcnt.cnt[(uint64)pa / PGSIZE] > 0) {
    release(&refcnt.lock);
    return;  // 아직 다른 프로세스가 사용 중
  }
  release(&refcnt.lock);
  // ... 기존 해제 로직 ...
}
```

**질문 2-5:** 참조 카운트 배열에 락(lock)이 필요한 이유는?

> 다중 CPU 환경에서 여러 프로세스가 동시에 fork/exit하여 같은 물리 페이지의 참조 카운트를 수정할 수 있기 때문이다. 데이터 레이스를 방지하려면 spinlock이 필요하다.

### 2.4 COW Fork 수정이 필요한 파일 요약

| 파일 | 수정 내용 |
|------|----------|
| `kernel/riscv.h` | `PTE_COW` 매크로 정의 |
| `kernel/kalloc.c` | 참조 카운트 배열 및 관리 함수 추가, `kfree()` 수정 |
| `kernel/vm.c` | `uvmcopy()` 수정 (복사 대신 공유), `cowfault()` 추가 |
| `kernel/trap.c` | `usertrap()`에서 COW fault 처리 분기 추가 |
| `kernel/defs.h` | 새로 추가한 함수의 프로토타입 선언 |

---

## Exercise 3: Lazy Allocation 설계 (10분)

### 3.1 현재 sbrk()의 동작

`sys_sbrk()` (`kernel/sysproc.c`)를 살펴보자:

```c
uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {       // 즉시 할당 (eager allocation)
      return -1;
    }
  } else {
    // Lazy allocation: 크기만 증가, 물리 메모리 할당하지 않음
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;          // 프로세스 크기만 변경!
  }
  return addr;
}
```

**질문 3-1:** `SBRK_EAGER`일 때와 lazy allocation일 때의 차이는?

> `SBRK_EAGER`: `growproc()` -> `uvmalloc()`을 호출하여 물리 메모리를 즉시 할당하고 페이지 테이블에 매핑한다. Lazy: `p->sz`만 증가시키고 물리 메모리는 할당하지 않는다.

### 3.2 Lazy Allocation의 핵심 아이디어

```
[ Eager Allocation (기존) ]          [ Lazy Allocation ]

sbrk(4096) 호출                      sbrk(4096) 호출
  |                                    |
  v                                    v
kalloc() -> 물리 페이지 할당            p->sz += 4096 (크기만 변경)
mappages() -> PTE 생성                 PTE 없음, 물리 페이지 없음
  |                                    |
  v                                    v
바로 사용 가능                          접근 시 page fault 발생!
                                       |
                                       v
                                     vmfault()에서 kalloc() + mappages()
                                       |
                                       v
                                     이제 사용 가능
```

**장점:** 프로그램이 할당한 메모리를 모두 사용하지 않을 수 있다. Lazy allocation은 실제로 접근하는 페이지만 물리 메모리를 소비하므로 메모리 효율이 높다.

### 3.3 Lazy Allocation 구현에서 주의할 점

현재 xv6에는 이미 lazy allocation이 부분적으로 구현되어 있다. 완전한 구현을 위해 고려해야 할 점들을 살펴보자.

#### 주의점 1: uvmunmap()에서의 처리

lazy allocation된 영역을 해제할 때, 아직 물리 페이지가 할당되지 않은 PTE가 있을 수 있다:

```c
// kernel/vm.c - uvmunmap() (현재 구현)
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  // ...
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;       // <-- lazy: PTE가 없으면 건너뜀
    if((*pte & PTE_V) == 0)
      continue;       // <-- lazy: 유효하지 않으면 건너뜀
    // ...
  }
}
```

**질문 3-2:** 원래 xv6의 `uvmunmap()`은 PTE가 없거나 유효하지 않으면 `panic()`을 호출했다. lazy allocation을 지원하려면 왜 이를 `continue`로 변경해야 하는가?

> lazy allocation에서는 `sbrk()`로 주소 공간을 늘렸지만 아직 물리 페이지를 할당하지 않은 페이지가 존재한다. 이 상태에서 프로세스가 종료되면 `uvmunmap()`이 호출되는데, 할당되지 않은 페이지에 대해 panic하면 안 된다.

#### 주의점 2: copyin/copyout에서의 처리

커널이 사용자 메모리에 접근할 때 (`copyin`, `copyout`), lazy allocation된 페이지에 접근할 수 있다:

```c
// kernel/vm.c - copyout() (현재 구현)
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  // ...
  pa0 = walkaddr(pagetable, va0);
  if(pa0 == 0) {
    if((pa0 = vmfault(pagetable, va0, 0)) == 0) {  // <-- lazy 페이지 처리
      return -1;
    }
  }
  // ...
}
```

**질문 3-3:** 커널 코드에서 사용자 주소에 접근할 때 page fault가 발생하면 어떻게 되는가? 왜 `copyout()`에서 `vmfault()`를 직접 호출하는 방식으로 처리하는가?

> 커널 모드에서의 page fault는 `kerneltrap()`에서 처리되며, 기본적으로 panic을 발생시킨다 (커널 코드의 버그로 간주). 따라서 커널이 lazy allocation된 사용자 메모리에 접근할 때는 하드웨어 page fault에 의존하지 않고, `walkaddr()`로 매핑을 확인한 후 매핑이 없으면 `vmfault()`를 명시적으로 호출하여 할당한다.

#### 주의점 3: sbrk로 메모리 줄이기 (n < 0)

```c
if(t == SBRK_EAGER || n < 0) {
  if(growproc(n) < 0) {
    return -1;
  }
}
```

**질문 3-4:** `n < 0`일 때는 왜 lazy하게 처리하지 않고 즉시 처리하는가?

> 메모리를 줄이는 경우에는 실제로 물리 페이지를 해제하여 즉시 메모리를 회수해야 한다. lazy하게 처리하면 이미 사용 중인 메모리를 적절히 해제하지 못할 수 있다.

### 3.4 Lazy Allocation 전체 흐름

```
1. sbrk(n) 호출 (n > 0, SBRK_LAZY)
   -> p->sz += n (크기만 변경, 물리 메모리 할당 없음)

2. 프로그램이 새로 할당된 영역에 접근
   -> MMU: PTE 없음 -> store/load page fault 발생
   -> scause = 13 or 15, stval = fault 주소

3. usertrap() -> vmfault() 호출
   -> va < p->sz 확인 (유효한 주소인지)
   -> ismapped() 확인 (아직 매핑되지 않은 것을 확인)
   -> kalloc() + memset(0) + mappages()

4. usertrap() 리턴 -> 사용자 모드로 복귀
   -> 같은 명령어 재실행 -> 이번에는 성공
```

---

## Exercise 4: COW Fork 테스트 프로그램 설계 (10분)

### 4.1 COW 동작 확인 프로그램 (개념 설계)

COW fork가 올바르게 구현되었는지 확인하기 위한 xv6 사용자 프로그램을 설계해보자:

```c
// user/cowtest.c (의사 코드)
#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096

int
main(void)
{
  // 1. 큰 메모리 영역 할당
  char *mem = sbrk(PGSIZE * 10);  // 10 페이지 할당

  // 2. 초기값 쓰기
  for(int i = 0; i < 10; i++) {
    mem[i * PGSIZE] = 'A' + i;
  }

  int pid = fork();

  if(pid == 0) {
    // 자식 프로세스
    // 3. 읽기만 수행 - COW 페이지 복사가 발생하지 않아야 함
    for(int i = 0; i < 10; i++) {
      if(mem[i * PGSIZE] != 'A' + i) {
        printf("cowtest: read failed at page %d\n", i);
        exit(1);
      }
    }
    printf("cowtest: read test passed\n");

    // 4. 쓰기 수행 - COW 페이지 복사가 발생해야 함
    mem[0] = 'Z';  // 첫 번째 페이지에 쓰기 -> page fault -> 복사
    printf("cowtest: write to page 0 done (value=%c)\n", mem[0]);

    // 5. 부모의 데이터는 변경되지 않아야 함
    exit(0);
  } else {
    // 부모 프로세스
    wait(0);

    // 6. 부모의 데이터가 그대로인지 확인
    if(mem[0] == 'A') {
      printf("cowtest: parent data unchanged - PASS\n");
    } else {
      printf("cowtest: parent data corrupted! - FAIL\n");
    }
    exit(0);
  }
}
```

**질문 4-1:** 위 테스트에서 자식이 `mem[0] = 'Z'`를 실행할 때 내부적으로 어떤 일이 발생하는가?

> 1. 자식이 `mem[0]`에 쓰기를 시도한다.
> 2. 이 페이지는 COW로 읽기 전용이므로 store page fault가 발생한다 (scause = 15).
> 3. `usertrap()` -> COW fault handler가 호출된다.
> 4. 새 물리 페이지를 할당하고 기존 내용을 복사한다.
> 5. 자식의 PTE를 새 물리 페이지로 변경하고, 쓰기 권한을 부여한다.
> 6. 참조 카운트를 조정한다.
> 7. 사용자 모드로 복귀하여 같은 명령어를 재실행 -> 이번에는 성공.
> 8. 부모의 페이지는 변경되지 않는다.

### 4.2 Linux/macOS에서 COW 동작 관찰

`examples/cow_concept.c`에서 실제 OS의 COW 동작을 관찰할 수 있다.

```bash
# 컴파일 및 실행
cd practice/week12/lab/examples
gcc -Wall -o cow_concept cow_concept.c
./cow_concept
```

이 프로그램은 `mmap()`과 `fork()`를 사용하여 COW의 동작을 보여준다. 자세한 내용은 `examples/cow_concept.c` 소스 코드를 참고하라.

### 4.3 Linux/macOS에서 Lazy Allocation 관찰

`examples/lazy_concept.c`에서 lazy allocation의 동작을 관찰할 수 있다.

```bash
# 컴파일 및 실행
cd practice/week12/lab/examples
gcc -Wall -o lazy_concept lazy_concept.c
./lazy_concept
```

이 프로그램은 `mmap(MAP_ANONYMOUS)`로 대용량 메모리를 할당하고, 페이지에 접근할 때마다 실제 물리 메모리 사용량이 증가하는 것을 보여준다. 자세한 내용은 `examples/lazy_concept.c` 소스 코드를 참고하라.

---

## 요약 및 핵심 정리

### Page Fault 활용 기법 비교

| | COW Fork | Lazy Allocation |
|---|---------|-----------------|
| **목적** | fork 시 불필요한 메모리 복사 방지 | 사용하지 않는 메모리 할당 방지 |
| **시점** | fork() 시 | sbrk() 시 |
| **동작** | 공유 후 write 시 복사 | 할당 없이 접근 시 할당 |
| **fault 종류** | Store page fault (scause=15) | Load/Store page fault (scause=13,15) |
| **PTE 변경** | W 제거, COW 비트 설정 | PTE 자체가 없음 |
| **추가 자료구조** | 참조 카운트 배열 | 없음 (p->sz로 범위 판단) |
| **핵심 함수** | uvmcopy(), cowfault() | sys_sbrk(), vmfault() |

### 핵심 개념 정리

1. **Page fault는 오류가 아니라 기회다**
   - 전통적으로 page fault는 프로그램의 잘못된 메모리 접근을 의미했지만, 현대 OS는 이를 **메모리 관리 최적화의 메커니즘**으로 적극 활용한다.

2. **지연(defer)의 가치**
   - COW와 lazy allocation 모두 "지금 당장 하지 않아도 되는 일은 나중에 한다"는 원칙을 따른다.
   - 실제로 필요할 때까지 비용이 큰 작업을 미루면 전체적인 성능이 향상된다.

3. **하드웨어-소프트웨어 협력**
   - MMU가 PTE를 검사하여 page fault를 발생시키고 (하드웨어), 커널의 trap handler가 이를 적절히 처리한다 (소프트웨어). PTE의 RSW 비트는 이러한 협력을 위해 설계된 인터페이스이다.

4. **TLB flush의 중요성**
   - PTE를 수정한 후에는 반드시 `sfence_vma()`로 TLB를 flush해야 한다. 그렇지 않으면 MMU가 캐싱된 이전 PTE를 사용하여 잘못된 동작을 할 수 있다.

# Week 11 Lab: Main Memory - Page Tables

## 학습 목표

이번 실습을 통해 다음을 이해하고 실습합니다:

1. RISC-V Sv39 3단계 페이지 테이블 구조를 이해한다
2. xv6 커널이 페이지 테이블을 어떻게 설정하는지 코드를 읽고 분석한다
3. 가상 주소가 물리 주소로 변환되는 과정을 `walk()` 함수를 통해 추적한다
4. 프로세스의 페이지 테이블 내용을 출력하는 `vmprint()` 함수를 직접 구현한다
5. `sbrk()`를 통한 메모리 할당 시 페이지 테이블이 어떻게 변하는지 추적한다

**교재 참고**: xv6 book Chapter 3 (Page Tables)

---

## 배경 지식: RISC-V Sv39 페이징 구조

### 페이지 테이블이란?

운영체제는 각 프로세스에게 독립된 가상 주소 공간을 제공합니다. 프로세스가 사용하는 **가상 주소(Virtual Address)**를 실제 메모리의 **물리 주소(Physical Address)**로 변환하는 것이 페이지 테이블의 역할입니다.

RISC-V는 **Sv39** 방식을 사용합니다:
- 가상 주소: **39비트** (512GB 주소 공간)
- 물리 주소: **56비트**
- 페이지 크기: **4KB** (4096 바이트 = 2^12 바이트)
- 페이지 테이블: **3단계** (Level 2 -> Level 1 -> Level 0)
- 각 페이지 테이블 페이지: **512개 PTE** (2^9 = 512)

### Sv39 가상 주소 구조 (39비트)

```
  38       30 29       21 20       12 11          0
 +-----------+-----------+-----------+-------------+
 | L2 index  | L1 index  | L0 index  | Page Offset |
 | (9 bits)  | (9 bits)  | (9 bits)  |  (12 bits)  |
 +-----------+-----------+-----------+-------------+
   VPN[2]      VPN[1]      VPN[0]       offset

 비트 63~39: 사용하지 않음 (반드시 0이어야 함)
```

- **L2 index (VPN[2])**: 비트 38~30 -- Level-2 페이지 테이블에서의 인덱스 (0~511)
- **L1 index (VPN[1])**: 비트 29~21 -- Level-1 페이지 테이블에서의 인덱스 (0~511)
- **L0 index (VPN[0])**: 비트 20~12 -- Level-0 페이지 테이블에서의 인덱스 (0~511)
- **Page Offset**: 비트 11~0 -- 4KB 페이지 내에서의 오프셋 (0~4095)

### 3단계 페이지 테이블 변환 과정

```
 satp 레지스터
 (L2 테이블의
  물리 주소)
      |
      v
 +----------+      +----------+      +----------+
 | L2 Table |      | L1 Table |      | L0 Table |
 |----------|      |----------|      |----------|
 |  PTE  0  |      |  PTE  0  |      |  PTE  0  |
 |  PTE  1  |      |  PTE  1  |      |  PTE  1  |
 |  ...     |      |  ...     |      |  ...     |
 |  PTE  i  |----->|  PTE  j  |----->|  PTE  k  |-----> 물리 페이지
 |  ...     |      |  ...     |      |  ...     |        + offset
 |  PTE 511 |      |  PTE 511 |      |  PTE 511 |
 +----------+      +----------+      +----------+

  i = VPN[2]        j = VPN[1]        k = VPN[0]
  (L2 index)        (L1 index)        (L0 index)
```

**변환 절차:**
1. `satp` 레지스터에서 L2 페이지 테이블의 물리 주소를 얻는다
2. L2 테이블에서 `VPN[2]`를 인덱스로 PTE를 읽는다 -> L1 테이블의 물리 주소를 얻는다
3. L1 테이블에서 `VPN[1]`을 인덱스로 PTE를 읽는다 -> L0 테이블의 물리 주소를 얻는다
4. L0 테이블에서 `VPN[0]`을 인덱스로 PTE를 읽는다 -> 최종 물리 페이지의 주소를 얻는다
5. 물리 페이지 주소 + Page Offset = 최종 물리 주소

### PTE (Page Table Entry) 구조 (64비트)

```
  63    54 53                    10 9   8 7 6 5 4 3 2 1 0
 +--------+------------------------+-----+-+-+-+-+-+-+-+-+
 |Reserved|   PPN (물리 페이지 번호) |RSW  |D|A|G|U|X|W|R|V|
 | (10)   |       (44 bits)        | (2) | | | | | | | | |
 +--------+------------------------+-----+-+-+-+-+-+-+-+-+
```

**플래그 비트 설명:**

| 비트 | 이름 | 설명 |
|------|------|------|
| 0 | V (Valid) | 이 PTE가 유효한지 여부 |
| 1 | R (Read) | 읽기 가능 |
| 2 | W (Write) | 쓰기 가능 |
| 3 | X (Execute) | 실행 가능 |
| 4 | U (User) | 유저 모드에서 접근 가능 |
| 5 | G (Global) | 글로벌 매핑 |
| 6 | A (Accessed) | 접근된 적이 있음 |
| 7 | D (Dirty) | 쓰기가 발생함 |

**중요한 규칙:**
- R=0, W=0, X=0이고 V=1이면: 이 PTE는 **다음 단계 페이지 테이블을 가리키는 포인터**
- R, W, X 중 하나라도 1이고 V=1이면: 이 PTE는 **리프(leaf) 엔트리** (실제 물리 페이지를 가리킴)

### xv6에서의 PTE 관련 매크로 (kernel/riscv.h)

```c
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1) // read
#define PTE_W (1L << 2) // write
#define PTE_X (1L << 3) // execute
#define PTE_U (1L << 4) // user can access

// 물리 주소 -> PTE 형식: 오른쪽 12비트 제거, 왼쪽 10비트 이동
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

// PTE -> 물리 주소: 오른쪽 10비트(플래그) 제거, 왼쪽 12비트 이동
#define PTE2PA(pte) (((pte) >> 10) << 12)

// PTE에서 플래그만 추출 (하위 10비트)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 가상 주소에서 각 단계의 인덱스 추출
#define PXMASK          0x1FF // 9 bits (= 0b111111111)
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)
//   PX(2, va) -> 비트 38~30 추출 (L2 인덱스)
//   PX(1, va) -> 비트 29~21 추출 (L1 인덱스)
//   PX(0, va) -> 비트 20~12 추출 (L0 인덱스)
```

**PA2PTE와 PTE2PA 변환의 이해:**

```
 물리 주소 (PA):
  55                          12 11       0
 +------------------------------+---------+
 |      PPN (44 bits)           | offset  |
 +------------------------------+---------+

 PA >> 12:  (offset 제거)
  43                             0
 +-------------------------------+
 |      PPN (44 bits)            |
 +-------------------------------+

 (PA >> 12) << 10:  (플래그 자리 확보)
  53                    10 9    0
 +------------------------+------+
 |   PPN (44 bits)        | 0000 |
 +------------------------+------+
  = PTE 형식 (플래그 비트는 0)
```

---

## Exercise 1: xv6 커널 메모리 맵 분석 (15분)

### 목표
`kernel/vm.c`의 `kvmmake()` 함수를 읽고, xv6 커널이 물리 메모리를 어떻게 가상 주소 공간에 매핑하는지 분석합니다.

### 코드 읽기

`kernel/vm.c`의 `kvmmake()` 함수를 열어보세요:

```c
// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}
```

`kvmmap()`의 시그니처: `kvmmap(pagetable, va, pa, size, perm)`
- `va`: 가상 주소
- `pa`: 물리 주소
- `size`: 매핑 크기
- `perm`: 권한 플래그

`kernel/memlayout.h`의 주요 상수:

```c
#define UART0     0x10000000L
#define VIRTIO0   0x10001000
#define PLIC      0x0c000000L
#define KERNBASE  0x80000000L
#define PHYSTOP   (KERNBASE + 128*1024*1024)  // = 0x88000000
#define TRAMPOLINE (MAXVA - PGSIZE)           // 가상 주소 최상단
#define TRAPFRAME  (TRAMPOLINE - PGSIZE)
```

### 질문 1-1: 커널 메모리 맵 표 완성하기

아래 표를 `kvmmake()`의 코드를 참고하여 완성하세요.

| 영역 | 가상 주소 (VA) | 물리 주소 (PA) | 크기 | 권한 | 용도 |
|------|---------------|---------------|------|------|------|
| UART | `0x10000000` | `___________` | 4KB | R, W | UART 장치 레지스터 |
| VIRTIO | `0x10001000` | `___________` | 4KB | R, W | 디스크 I/O 인터페이스 |
| PLIC | `0x0c000000` | `___________` | `___`MB | R, W | 인터럽트 컨트롤러 |
| 커널 텍스트 | `0x80000000` | `___________` | etext까지 | `___, ___` | 커널 코드 |
| 커널 데이터 | `etext` | `___________` | PHYSTOP까지 | `___, ___` | 커널 데이터+힙 |
| TRAMPOLINE | `MAXVA-PGSIZE` | `___________` | 4KB | `___, ___` | 트랩 진입/복귀 |

### 질문 1-2: Direct Mapping (직접 매핑)

`kvmmake()` 코드에서 대부분의 커널 매핑은 `VA == PA`입니다 (직접 매핑).

1. 직접 매핑이 **아닌** 영역은 어디인가요?

   ```
   답: ________________________________________
   ```

2. 왜 그 영역은 직접 매핑을 사용하지 않을까요?

   ```
   답: ________________________________________
   ```

### 질문 1-3: 권한 분석

1. 커널 텍스트 영역의 권한이 `PTE_R | PTE_X`인 이유는 무엇인가요? `PTE_W`가 빠진 이유는?

   ```
   답: ________________________________________
   ```

2. 커널 데이터 영역의 권한이 `PTE_R | PTE_W`인 이유는? `PTE_X`가 빠진 이유는?

   ```
   답: ________________________________________
   ```

3. 커널 영역에는 `PTE_U` 플래그가 없습니다. 이것이 의미하는 것은?

   ```
   답: ________________________________________
   ```

### 커널 메모리 레이아웃 다이어그램

```
가상 주소                      물리 주소

MAXVA (0x4000000000) ----+
                         |
  TRAMPOLINE  -----------+-------> trampoline 코드 (물리)
  (MAXVA - 4KB)          |         (커널 코드 내)
                         |
  Kernel Stacks          |         (별도 할당된 물리 페이지)
  (Guard pages 포함)      |
                         |
         ...             |
         (빈 공간)        |
         ...             |
                         |
  PHYSTOP (0x88000000) --+-------> 0x88000000
         |               |            |
    커널 데이터 (R/W)      |       커널 데이터 (물리)
         |               |            |
  etext  ----------------+-------> etext
         |               |            |
    커널 텍스트 (R/X)      |       커널 텍스트 (물리)
         |               |            |
  KERNBASE (0x80000000) -+-------> 0x80000000
         |               |
         ...              |
         |               |
  VIRTIO0 (0x10001000)---+-------> 0x10001000
  UART0 (0x10000000) ----+-------> 0x10000000
         |               |
  PLIC (0x0C000000) -----+-------> 0x0C000000
         |               |
  0x00000000 ------------+
```

---

## Exercise 2: walk() 함수 추적 (10분)

### 목표
`walk()` 함수의 코드를 읽고, 주어진 가상 주소에 대해 3단계 페이지 테이블을 탐색하는 과정을 직접 추적합니다.

### walk() 함수 코드

```c
// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}
```

### 함수 동작 분석

1. **입력**: 최상위 페이지 테이블(`pagetable`), 가상 주소(`va`), 할당 플래그(`alloc`)
2. **루프**: `level = 2, 1` (L2, L1 두 단계를 처리)
   - `PX(level, va)`로 현재 레벨의 인덱스 추출
   - PTE가 유효(V=1)하면: PTE에서 다음 단계 테이블의 물리 주소 추출
   - PTE가 무효(V=0)하면:
     - `alloc=1`이면 새 페이지를 할당하여 연결
     - `alloc=0`이면 `NULL` 반환
3. **반환**: L0 테이블에서 `PX(0, va)` 인덱스의 PTE 주소 반환

### 질문 2-1: 수동 주소 변환

보조 Python 스크립트(`examples/va_translate.py`)를 사용하거나, 직접 계산하여 아래를 풀어보세요.

**가상 주소 `0x0000000080001234`에 대해:**

```
전체 VA (2진수):  0000...0 10 000000000 000000000 001 001000110100

39비트만 추출:    1_00000000_000000000_000000001_001000110100
                  ^         ^         ^          ^
```

1. L2 인덱스 (VPN[2], 비트 38~30) = ?

   ```
   계산: (0x80001234 >> 30) & 0x1FF = _______________
   답: ___
   ```

2. L1 인덱스 (VPN[1], 비트 29~21) = ?

   ```
   계산: (0x80001234 >> 21) & 0x1FF = _______________
   답: ___
   ```

3. L0 인덱스 (VPN[0], 비트 20~12) = ?

   ```
   계산: (0x80001234 >> 12) & 0x1FF = _______________
   답: ___
   ```

4. Page Offset (비트 11~0) = ?

   ```
   계산: 0x80001234 & 0xFFF = _______________
   답: ___
   ```

### 질문 2-2: walk() 실행 추적

위의 가상 주소 `0x80001234`에 대해 `walk()` 함수의 실행을 추적해 보세요.

```
1단계 (level=2):
  PX(2, 0x80001234) = ___ (L2 인덱스)
  L2 테이블[___]의 PTE를 읽음
  PTE가 유효하면 -> PTE2PA(PTE) = L1 테이블의 물리 주소

2단계 (level=1):
  PX(1, 0x80001234) = ___ (L1 인덱스)
  L1 테이블[___]의 PTE를 읽음
  PTE가 유효하면 -> PTE2PA(PTE) = L0 테이블의 물리 주소

3단계 (루프 종료 후):
  PX(0, 0x80001234) = ___ (L0 인덱스)
  L0 테이블[___]의 PTE 주소를 반환

최종 물리 주소 = PTE2PA(L0_PTE) + offset(0x___) = 0x___________
```

### 질문 2-3: walk()에서 alloc 매개변수의 역할

1. `walk(pagetable, va, 0)`과 `walk(pagetable, va, 1)`의 차이는 무엇인가요?

   ```
   답: ________________________________________
   ```

2. `mappages()` 함수에서는 `walk(pagetable, a, 1)`을 호출합니다. 왜 `alloc=1`인가요?

   ```
   답: ________________________________________
   ```

3. `walkaddr()` 함수에서는 `walk(pagetable, va, 0)`을 호출합니다. 왜 `alloc=0`인가요?

   ```
   답: ________________________________________
   ```

---

## Exercise 3: vmprint() 함수 구현 (15분)

### 목표
프로세스의 3단계 페이지 테이블 내용을 출력하는 `vmprint()` 함수를 작성합니다. 이 함수를 통해 페이지 테이블의 실제 구조를 눈으로 확인할 수 있습니다.

### 구현 가이드

`examples/vmprint.c` 파일에 구현 코드가 있습니다. 이 코드를 `kernel/vm.c`에 추가합니다.

**출력 형식:**
```
page table 0x0000000087f6b000
 ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
 .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000
 .. .. ..0: pte 0x0000000021fda41f pa 0x0000000087f69000
 .. .. ..1: pte 0x0000000021fd9017 pa 0x0000000087f64000
 .. .. ..2: pte 0x0000000021fd8c07 pa 0x0000000087f63000
 ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000
 .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
 .. .. ..510: pte 0x0000000021fdcc17 pa 0x0000000087f73000
 .. .. ..511: pte 0x0000000020001c4b pa 0x0000000080007000
```

- 들여쓰기로 레벨 표시: `..` (L2), `.. ..` (L1), `.. .. ..` (L0)
- 각 유효한 PTE에 대해: 인덱스, PTE 원본 값, 물리 주소 출력

### Step 1: kernel/vm.c에 vmprint() 추가

`examples/vmprint.c` 파일의 코드를 `kernel/vm.c` 파일의 맨 끝에 추가하세요.

```c
// 페이지 테이블 출력을 위한 재귀 헬퍼 함수
void
vmprint_recursive(pagetable_t pagetable, int level)
{
  // 페이지 테이블의 512개 엔트리를 순회
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){   // 유효한 PTE만 출력
      // 들여쓰기: 레벨에 따라 ".." 반복
      for(int j = 2; j >= level; j--){
        if(j == 2)
          printf(" ..");
        else
          printf(" ..");
      }
      uint64 pa = PTE2PA(pte);
      printf("%d: pte 0x%016lx pa 0x%016lx", i, (uint64)pte, pa);

      // 플래그 출력
      printf(" [");
      if(pte & PTE_R) printf("R");
      if(pte & PTE_W) printf("W");
      if(pte & PTE_X) printf("X");
      if(pte & PTE_U) printf("U");
      printf("]");
      printf("\n");

      // 리프가 아닌 PTE (R=0, W=0, X=0) -> 다음 레벨 재귀
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        vmprint_recursive((pagetable_t)pa, level - 1);
      }
    }
  }
}

// 프로세스의 페이지 테이블 내용을 출력
void
vmprint(pagetable_t pagetable)
{
  printf("page table 0x%016lx\n", (uint64)pagetable);
  vmprint_recursive(pagetable, 2);
}
```

### Step 2: kernel/defs.h에 함수 선언 추가

`kernel/defs.h` 파일에서 vm.c 관련 선언 부분을 찾아 아래를 추가하세요:

```c
void            vmprint(pagetable_t);
```

### Step 3: kernel/exec.c에서 vmprint() 호출

`kernel/exec.c`의 `kexec()` 함수 `return argc;` 직전에 다음을 추가하세요:

```c
  // 디버깅: 새 프로세스의 페이지 테이블 출력
  if(p->pid == 1){
    printf("== pid 1 (init) page table ==\n");
    vmprint(p->pagetable);
  }
```

`pid == 1`로 제한하는 이유는 첫 번째 프로세스(init)의 페이지 테이블만 출력하여 출력량을 줄이기 위함입니다.

### Step 4: 빌드 및 실행

```bash
$ make clean
$ make qemu
```

### 질문 3-1: 출력 분석

`vmprint()`의 출력을 보고 아래 질문에 답하세요.

1. L2 테이블에 유효한 PTE가 몇 개인가요? 그 인덱스는?

   ```
   답: ________________________________________
   ```

2. 인덱스 0의 L2 PTE는 어떤 가상 주소 범위를 담당하나요?
   (힌트: L2 인덱스 0 -> VA 범위 = `0x0` ~ `0x0000003FFFFFFF`, 즉 1GB)

   ```
   답: ________________________________________
   ```

3. 인덱스 255의 L2 PTE는 어떤 가상 주소 범위를 담당하나요?
   (힌트: 255 * 1GB = ?)

   ```
   답: ________________________________________
   ```

4. L0 레벨에서 `[U]` 플래그가 있는 PTE와 없는 PTE의 차이는 무엇인가요?

   ```
   답: ________________________________________
   ```

### 질문 3-2: vmprint()의 재귀 구조 이해

1. `vmprint_recursive()` 함수에서 `(pte & (PTE_R|PTE_W|PTE_X)) == 0` 조건은 무엇을 확인하나요?

   ```
   답: ________________________________________
   ```

2. 이 조건이 참이면 재귀 호출을 합니다. 왜 이것이 올바른 동작인가요?

   ```
   답: ________________________________________
   ```

3. `freewalk()` 함수도 비슷한 재귀 구조를 가지고 있습니다. `kernel/vm.c`에서 해당 함수를 찾아 비교해 보세요. `vmprint_recursive()`와 `freewalk()`의 유사점과 차이점은?

   ```c
   // freewalk() 코드 참고
   void
   freewalk(pagetable_t pagetable)
   {
     for(int i = 0; i < 512; i++){
       pte_t pte = pagetable[i];
       if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
         uint64 child = PTE2PA(pte);
         freewalk((pagetable_t)child);
         pagetable[i] = 0;
       } else if(pte & PTE_V){
         panic("freewalk: leaf");
       }
     }
     kfree((void*)pagetable);
   }
   ```

   ```
   유사점: ________________________________________
   차이점: ________________________________________
   ```

---

## Exercise 4: sbrk() 동작 추적 (10분)

### 목표
프로세스가 `sbrk()` 시스템 콜로 힙 메모리를 확장할 때, 페이지 테이블이 어떻게 변하는지 추적합니다.

### sbrk() 호출 경로

```
유저 프로그램: sbrk(n)
    |
    v
sys_sbrk()          [kernel/sysproc.c]
    |
    v
growproc(n)         [kernel/proc.c]
    |
    v
uvmalloc()          [kernel/vm.c]
    |
    v
kalloc() + mappages()
```

### 관련 코드

**sys_sbrk() - kernel/sysproc.c:**

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
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory
    myproc()->sz += n;
  }
  return addr;
}
```

**growproc() - kernel/proc.c:**

```c
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if(sz + n > TRAPFRAME) {
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```

**uvmalloc() - kernel/vm.c:**

```c
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();                // 1. 물리 페이지 할당
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);       // 2. 0으로 초기화
    if(mappages(pagetable, a, PGSIZE, (uint64)mem,
                PTE_R|PTE_U|xperm) != 0){  // 3. 페이지 테이블에 매핑
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}
```

### 질문 4-1: uvmalloc() 동작 분석

프로세스의 현재 크기(`p->sz`)가 `0x3000` (12KB, 3 페이지)이고, `sbrk(8192)` (8KB, 2 페이지)를 호출한다고 가정합니다.

1. `uvmalloc()`에서 `oldsz`와 `newsz`의 값은?

   ```
   oldsz = ___________
   newsz = ___________
   ```

2. 루프는 몇 번 실행되나요? 각 반복에서 어떤 가상 주소에 매핑이 생기나요?

   ```
   1회차: va = ___________
   2회차: va = ___________
   ```

3. 새로 생긴 PTE의 플래그는 무엇인가요? (`PTE_R|PTE_U|xperm`, 여기서 `xperm`은 `PTE_W`)

   ```
   답: PTE_R | PTE_U | PTE_W | PTE_V = ___________
   (힌트: PTE_V는 mappages()에서 자동으로 추가됨)
   ```

### 질문 4-2: sbrk() 전후 페이지 테이블 변화

아래 다이어그램에서 `sbrk(4096)` 호출 전후의 페이지 테이블 변화를 그려보세요.

```
sbrk(4096) 호출 전 (p->sz = 0x3000):

 VA 범위        매핑 상태
 0x0000~0x0FFF  [페이지 0] -> PA: 0x87f60000 (코드)
 0x1000~0x1FFF  [페이지 1] -> PA: 0x87f5f000 (데이터)
 0x2000~0x2FFF  [페이지 2] -> PA: 0x87f5e000 (스택)
 0x3000~0x3FFF  [미할당]
 0x4000~0x4FFF  [미할당]


sbrk(4096) 호출 후 (p->sz = 0x4000):

 VA 범위        매핑 상태
 0x0000~0x0FFF  [페이지 0] -> PA: 0x87f60000 (코드)
 0x1000~0x1FFF  [페이지 1] -> PA: 0x87f5f000 (데이터)
 0x2000~0x2FFF  [페이지 2] -> PA: 0x87f5e000 (스택)
 0x3000~0x3FFF  [___________] -> PA: ___________ (새 힙)
 0x4000~0x4FFF  [미할당]
```

### 질문 4-3: Lazy Allocation

이 xv6 버전에서는 `sys_sbrk()`에 lazy allocation이 구현되어 있습니다.

1. Eager allocation과 lazy allocation의 차이는 무엇인가요?

   ```
   Eager: ________________________________________
   Lazy:  ________________________________________
   ```

2. Lazy allocation에서 `sbrk(4096)`을 호출하면 물리 페이지가 바로 할당되나요?

   ```
   답: ________________________________________
   ```

3. 실제 물리 페이지는 언제 할당되나요? (`kernel/vm.c`의 `vmfault()` 함수를 참고하세요)

   ```
   답: ________________________________________
   ```

### 질문 4-4: 유저 프로세스 메모리 레이아웃

xv6의 유저 프로세스 가상 주소 공간 레이아웃을 그려보세요.

```
  MAXVA (0x4000000000)
    +------------------+
    |   TRAMPOLINE     | <- 커널과 동일한 물리 페이지
    +------------------+
    |   TRAPFRAME      | <- 프로세스별 트랩 프레임
    +------------------+
    |                  |
    |   (빈 공간)       |
    |                  |
    +------------------+
    |                  |
    |    힙 (Heap)      | <- sbrk()로 확장
    |    ^^^^^^^^^^^    |
    +------------------+ <- p->sz
    |  Guard Page       | <- PTE_U 없음 (접근 불가)
    +------------------+
    |  User Stack       | <- 스택
    +------------------+
    |  Data + BSS       | <- 전역 변수, 초기화 데이터
    +------------------+
    |  Text (Code)      | <- 프로그램 코드
    +------------------+
    0x0000000000
```

(참고: xv6에서는 `exec.c`의 `kexec()` 함수에서 스택이 text/data 바로 위에 할당됩니다. heap은 스택 위에서 `sbrk()`로 증가합니다.)

---

## 요약 및 핵심 정리

### 핵심 개념

1. **Sv39 3단계 페이지 테이블**: RISC-V는 39비트 가상 주소를 9+9+9+12로 분할하여 3단계 페이지 테이블로 변환합니다.

2. **PTE 구조**: 각 PTE는 64비트로, 물리 페이지 번호(PPN)와 플래그 비트(V, R, W, X, U 등)를 포함합니다.

3. **커널 직접 매핑**: xv6 커널은 대부분의 물리 메모리를 VA=PA로 직접 매핑하여, 물리 주소를 쉽게 접근할 수 있도록 합니다.

4. **walk() 함수**: 가상 주소에서 L2->L1->L0 순으로 페이지 테이블을 탐색하여 최종 PTE를 찾습니다. `alloc=1`이면 중간 테이블이 없을 때 새로 할당합니다.

5. **uvmalloc()**: 프로세스 메모리 확장 시 `kalloc()`으로 물리 페이지를 할당하고, `mappages()`로 페이지 테이블에 매핑을 추가합니다.

6. **Lazy allocation**: `sbrk()` 호출 시 `p->sz`만 증가시키고 물리 페이지는 실제 접근 시점(page fault)에 `vmfault()`에서 할당합니다.

### 핵심 함수 요약

| 함수 | 파일 | 역할 |
|------|------|------|
| `kvmmake()` | vm.c | 커널 페이지 테이블 생성 (직접 매핑) |
| `kvmmap()` | vm.c | 커널 페이지 테이블에 매핑 추가 |
| `walk()` | vm.c | VA에 해당하는 PTE를 3단계로 찾아 반환 |
| `walkaddr()` | vm.c | VA -> PA 변환 (유저 페이지용) |
| `mappages()` | vm.c | VA 범위를 PA에 매핑 (PTE 설정) |
| `uvmcreate()` | vm.c | 빈 유저 페이지 테이블 생성 |
| `uvmalloc()` | vm.c | 유저 메모리 확장 (물리 페이지 할당 + 매핑) |
| `uvmdealloc()` | vm.c | 유저 메모리 축소 (매핑 해제 + 물리 페이지 해제) |
| `growproc()` | proc.c | 프로세스 크기 변경 (sbrk 백엔드) |
| `vmfault()` | vm.c | 페이지 폴트 핸들러 (lazy allocation) |

### 핵심 매크로 요약

| 매크로 | 동작 |
|--------|------|
| `PX(level, va)` | VA에서 해당 level의 9비트 인덱스 추출 |
| `PA2PTE(pa)` | 물리 주소 -> PTE 형식 변환 |
| `PTE2PA(pte)` | PTE -> 물리 주소 변환 |
| `PTE_FLAGS(pte)` | PTE에서 플래그 비트(하위 10비트) 추출 |
| `PGROUNDUP(sz)` | 페이지 크기 경계로 올림 |
| `PGROUNDDOWN(a)` | 페이지 크기 경계로 내림 |

---

## 참고 자료

- xv6 book Chapter 3: Page Tables
- RISC-V Privileged Architecture Specification (Sv39 섹션)
- `kernel/vm.c` - 가상 메모리 관련 핵심 코드
- `kernel/riscv.h` - RISC-V 관련 매크로 정의
- `kernel/memlayout.h` - 메모리 레이아웃 상수 정의
- `kernel/exec.c` - exec 시스템 콜 구현
- `kernel/proc.c` - 프로세스 관리 (growproc 등)

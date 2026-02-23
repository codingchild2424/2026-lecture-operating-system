/*
 * vmprint() - 프로세스의 페이지 테이블 내용을 출력하는 함수
 *
 * 이 코드를 kernel/vm.c 파일의 맨 끝에 추가하세요.
 *
 * === 추가 방법 ===
 *
 * 1단계: 이 파일의 두 함수를 kernel/vm.c 맨 끝에 복사하세요.
 *
 * 2단계: kernel/defs.h 파일에서 "// vm.c" 섹션을 찾아 아래 선언을 추가하세요:
 *
 *     void            vmprint(pagetable_t);
 *
 * 3단계: kernel/exec.c의 kexec() 함수에서 "return argc;" 직전에 아래 코드를 추가하세요:
 *
 *     if(p->pid == 1){
 *       printf("== pid 1 (init) page table ==\n");
 *       vmprint(p->pagetable);
 *     }
 *
 * 4단계: 빌드 및 실행
 *     $ make clean
 *     $ make qemu
 *
 * === 예상 출력 ===
 *
 * == pid 1 (init) page table ==
 * page table 0x0000000087f6b000
 *  ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000 [--]
 *  .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000 [--]
 *  .. .. ..0: pte 0x0000000021fda41f pa 0x0000000087f69000 [RWXU]
 *  .. .. ..1: pte 0x0000000021fd9017 pa 0x0000000087f64000 [RWU]
 *  .. .. ..2: pte 0x0000000021fd8c07 pa 0x0000000087f63000 [RW]
 *  .. .. ..3: pte 0x0000000021fd8817 pa 0x0000000087f62000 [RWU]
 *  ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000 [--]
 *  .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000 [--]
 *  .. .. ..510: pte 0x0000000021fdcc17 pa 0x0000000087f73000 [RWU]
 *  .. .. ..511: pte 0x0000000020001c4b pa 0x0000000080007000 [RX]
 *
 * === 출력 해석 ===
 *
 * 1) L2 인덱스 0 -> 가상 주소 0x0000000000 ~ 0x003FFFFFFF 범위 (유저 코드/데이터 영역)
 *    - L1 인덱스 0 -> L0 테이블로 연결
 *      - L0 인덱스 0: 유저 코드 페이지 (RWXU) -- text
 *      - L0 인덱스 1: 유저 데이터 페이지 (RWU) -- data
 *      - L0 인덱스 2: 가드 페이지 (RW, U 없음) -- exec에서 uvmclear()로 PTE_U 제거
 *      - L0 인덱스 3: 유저 스택 페이지 (RWU) -- stack
 *
 * 2) L2 인덱스 255 -> 가상 주소의 최상위 영역
 *    - L1 인덱스 511 -> L0 테이블로 연결
 *      - L0 인덱스 510: TRAPFRAME 페이지 (RWU) -- 트랩 시 레지스터 저장
 *      - L0 인덱스 511: TRAMPOLINE 페이지 (RX, U 없음) -- 트랩 핸들러 코드
 *
 * 참고: 실제 출력은 실행 환경에 따라 물리 주소가 다를 수 있습니다.
 *       PTE 값과 플래그 구조는 동일합니다.
 */

// ====================================================================
// 아래 코드를 kernel/vm.c 맨 끝에 추가하세요
// ====================================================================

/*
 * vmprint_recursive - 페이지 테이블의 유효한 엔트리를 재귀적으로 출력
 *
 * pagetable: 현재 레벨의 페이지 테이블 포인터
 * level: 현재 레벨 (2 = L2, 1 = L1, 0 = L0)
 *
 * 비 리프 PTE (R=0, W=0, X=0인 유효 PTE)를 만나면
 * 해당 PTE가 가리키는 다음 레벨 테이블로 재귀 호출합니다.
 */
void
vmprint_recursive(pagetable_t pagetable, int level)
{
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(!(pte & PTE_V))
      continue;           // 유효하지 않은 PTE는 건너뜀

    // 들여쓰기 출력: 레벨이 낮을수록 깊은 들여쓰기
    // level 2: " .."
    // level 1: " .. .."
    // level 0: " .. .. .."
    for(int j = 2; j >= level; j--){
      printf(" ..");
    }

    // PTE 인덱스, PTE 값, 물리 주소 출력
    uint64 pa = PTE2PA(pte);
    printf("%d: pte 0x%016lx pa 0x%016lx", i, (uint64)pte, pa);

    // 플래그 비트를 읽기 쉬운 형태로 출력
    printf(" [");
    if(pte & PTE_R) printf("R");
    if(pte & PTE_W) printf("W");
    if(pte & PTE_X) printf("X");
    if(pte & PTE_U) printf("U");
    if((pte & (PTE_R|PTE_W|PTE_X)) == 0) printf("--");
    printf("]");
    printf("\n");

    // 비 리프 PTE이면 다음 레벨로 재귀
    // 리프 PTE: R, W, X 중 하나 이상이 설정됨
    // 비 리프 PTE: R=0, W=0, X=0 (다음 단계 테이블을 가리킴)
    if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
      vmprint_recursive((pagetable_t)pa, level - 1);
    }
  }
}

/*
 * vmprint - 프로세스의 전체 페이지 테이블을 출력
 *
 * pagetable: L2 (최상위) 페이지 테이블의 포인터
 *
 * 사용 예:
 *   vmprint(p->pagetable);   // 프로세스 p의 페이지 테이블 출력
 *   vmprint(kernel_pagetable); // 커널 페이지 테이블 출력
 */
void
vmprint(pagetable_t pagetable)
{
  printf("page table 0x%016lx\n", (uint64)pagetable);
  vmprint_recursive(pagetable, 2);
}

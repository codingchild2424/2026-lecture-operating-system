// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.
//
// [과제 솔루션] Per-CPU free list를 사용하도록 수정
// 원본: xv6-riscv/kernel/kalloc.c

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// Per-CPU free list: 각 CPU가 자체 lock과 freelist를 보유
// 이렇게 하면 서로 다른 CPU에서의 할당/해제가 병렬로 진행 가능
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  char lockname[8];

  // 모든 CPU의 lock 초기화
  for(int i = 0; i < NCPU; i++){
    snprintf(lockname, sizeof(lockname), "kmem%d", i);
    initlock(&kmem[i].lock, lockname);
  }

  // 초기에 모든 free page는 freerange -> kfree를 통해 추가된다.
  // kinit은 CPU 0에서 실행되므로 모든 페이지가 CPU 0의 freelist에 들어간다.
  // 이후 다른 CPU가 kalloc할 때 CPU 0에서 steal하게 된다.
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int id;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // cpuid()를 안전하게 호출하려면 인터럽트가 꺼져 있어야 한다.
  // acquire가 내부적으로 push_off를 호출하지만, cpuid()를 acquire 전에
  // 호출해야 어떤 CPU의 freelist에 넣을지 결정할 수 있다.
  push_off();
  id = cpuid();

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int id;

  // 현재 CPU 번호를 안전하게 얻기 위해 인터럽트 비활성화
  push_off();
  id = cpuid();

  // 1단계: 자신의 freelist에서 할당 시도
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  // 2단계: 자신의 freelist가 비었으면 다른 CPU에서 steal
  if(!r) {
    for(int i = 0; i < NCPU; i++) {
      if(i == id)
        continue;

      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r) {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;  // 하나 찾으면 종료
      }
      release(&kmem[i].lock);
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

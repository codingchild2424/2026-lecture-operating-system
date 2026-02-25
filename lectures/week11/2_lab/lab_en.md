---
theme: default
title: "Week 11 — Lab: Main Memory — Page Tables"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 11 — Page Tables

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science
</div>

---

# Lab Overview

**Topic**: xv6 Page Table Internals — address translation and management

**Objectives**
- Understand how `kvmmake()` builds the kernel page table with direct mapping
- Manually trace virtual-to-physical address translation through the 3-level Sv39 page table
- Implement `vmprint()` to recursively display a process's page table
- Observe how `sbrk()` modifies the page table when the heap grows

**Exercises**

| # | Topic | Time |
|---|-------|------|
| 1 | Kernel memory map — `kvmmake()` | 10 min |
| 2 | `walk()` tracing — manual address translation | 15 min |
| 3 | `vmprint()` implementation | 15 min |
| 4 | `sbrk()` behavior tracing | 10 min |

---
layout: section
---

# Exercise 1
## Kernel Memory Map

---

# Exercise 1: Kernel Memory Map

**`kvmmake()`** — `kernel/vm.c` — builds the kernel page table at boot

```c
pagetable_t kvmmake(void) {
  pagetable_t kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // I/O devices — direct mapped (VA == PA)
  kvmmap(kpgtbl, UART0,     UART0,     PGSIZE,   PTE_R|PTE_W);
  kvmmap(kpgtbl, VIRTIO0,   VIRTIO0,   PGSIZE,   PTE_R|PTE_W);

  // Kernel code — direct mapped, executable
  kvmmap(kpgtbl, KERNBASE,  KERNBASE,  (uint64)etext-KERNBASE, PTE_R|PTE_X);

  // Kernel data + free memory — direct mapped, writable
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R|PTE_W);

  // Trampoline — mapped at top of VA space
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R|PTE_X);
  return kpgtbl;
}
```

**Direct mapping**: `VA == PA` for most kernel regions (`KERNBASE = 0x80000000`)
- Simplifies kernel pointer arithmetic — kernel pointers are also valid physical addresses
- Exception: **trampoline** is mapped at the top of VA (`MAXVA - PGSIZE`) to share one physical page with all processes

---
layout: section
---

# Exercise 2
## walk() Tracing

---

# Exercise 2: walk() Tracing

**Sv39 virtual address** — 39 bits split into three 9-bit VPN fields + 12-bit offset

```
  38       30 29       21 20       12 11          0
 +-----------+-----------+-----------+-------------+
 | VPN[2]    | VPN[1]    | VPN[0]    |   offset    |
 | (9 bits)  | (9 bits)  | (9 bits)  |  (12 bits)  |
 +-----------+-----------+-----------+-------------+
   L2 index    L1 index    L0 index
```

**`walk(pagetable, va, alloc)`** — `kernel/vm.c`

```c
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
  for (int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];   // PX extracts VPN[level]
    if (*pte & PTE_V) {
      pagetable = (pagetable_t) PTE2PA(*pte); // follow to next level
    } else {
      if (!alloc) return 0;
      pagetable = (pagetable_t) kalloc();     // allocate new page table page
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];              // return L0 PTE
}
```

**PTE flags**: `PTE_V` (valid), `PTE_R` (read), `PTE_W` (write), `PTE_X` (execute), `PTE_U` (user)
- Physical address = `PTE2PA(*pte)` = PTE bits `[53:10]` shifted left by 12
- `walk()` with `alloc=1` allocates missing intermediate page table pages

---
layout: section
---

# Exercise 3
## vmprint() Implementation

---

# Exercise 3: vmprint() Implementation

**Goal**: print a page table in the format used by the xv6 book

```
page table 0x0000000087f6b000
 ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
 .. ..0: pte 0x0000000021fd9401 pa 0x0000000087f65000
 .. .. ..0: pte 0x0000000021fd98c7 pa 0x0000000087f66000
```

**Implementation sketch**

```c
void vmprint(pagetable_t pagetable) {
  printf("page table %p\n", pagetable);
  vmprint_level(pagetable, 2);
}

void vmprint_level(pagetable_t pagetable, int level) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if (!(pte & PTE_V)) continue;          // skip invalid entries
    // print indent: level 2 = " ..", level 1 = " .. ..", level 0 = " .. .. .."
    printf("%s%d: pte %p pa %p\n", indent[level], i, pte, PTE2PA(pte));
    if (level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      // non-leaf PTE — recurse into next level
      vmprint_level((pagetable_t) PTE2PA(pte), level - 1);
    }
  }
}
```

**Leaf vs. non-leaf PTE**: a PTE with no `R/W/X` flags set is an intermediate (pointer) PTE; recurse into it
- Call `vmprint(p->pagetable)` at the end of `exec()` to print the first process's page table
- Add `vmprint` declaration to `kernel/defs.h`

---
layout: section
---

# Exercise 4
## sbrk() Behavior Tracing

---

# Exercise 4: sbrk() Behavior Tracing

**`sbrk(n)`** — grows (or shrinks) the process heap by `n` bytes

```c
// kernel/sysproc.c
uint64 sys_sbrk(void) {
  int n;
  argint(0, &n);
  uint64 old_sz = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return old_sz;
}

// kernel/proc.c — growproc calls uvmalloc / uvmdealloc
int growproc(int n) {
  uint64 sz = p->sz;
  if (n > 0)
    sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W);  // allocates pages, maps them
  else if (n < 0)
    sz = uvmdealloc(p->pagetable, sz, sz + n);        // frees pages, removes PTEs
  p->sz = sz;
  return 0;
}
```

**What happens on `sbrk(4096)` (one new page)**

1. `uvmalloc` calls `kalloc()` to get a physical page
2. `mappages()` calls `walk(alloc=1)` to create PTE entries as needed
3. The new L0 PTE points to the allocated physical page with `PTE_R|PTE_W|PTE_U|PTE_V`
4. `p->sz` advances by `PGSIZE`; the new VA is now accessible to the user process

**Observation**: use `vmprint()` before and after `sbrk()` to see the new PTE appear at the heap boundary

---

# Key Takeaways

**Kernel memory map**
- xv6 uses direct mapping (`VA == PA`) from `KERNBASE` upward for simplicity
- Trampoline is the only exception: one physical page mapped at `MAXVA - PGSIZE` in every address space

**Sv39 three-level page table**
- 39-bit VA → VPN[2]:VPN[1]:VPN[0] (each 9 bits) + 12-bit offset
- `walk()` traverses three levels, allocating intermediate pages on demand

**vmprint() implementation**
- Recursive descent: at each level, iterate 512 PTEs, skip invalid ones
- Distinguish leaf PTEs (have R/W/X) from pointer PTEs (recurse further)

**sbrk() and page table growth**
- `growproc` → `uvmalloc` → `mappages` → `walk(alloc=1)`
- Each new heap page adds one L0 PTE; intermediate pages are shared and rarely reallocated
- Page table changes are immediately visible — no TLB shootdown needed on the same hart

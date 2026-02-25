---
theme: default
title: "Week 12 — Lab: COW Fork & Lazy Allocation"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 12 — COW Fork & Lazy Allocation

---

layout: section

---

# Lab Overview

---

# Lab Overview

- **Goal**: Understand page fault handling and implement virtual memory optimizations
- **Duration**: ~60 minutes
- **3 Exercises**:
  1. Page fault handling flow — trace `usertrap()` and `vmfault()`
  2. Copy-On-Write (COW) fork — share pages read-only, copy on write
  3. Lazy allocation — defer physical memory allocation until first access
- **Key insight**: Page faults are not errors — they are opportunities to defer and share work

---

layout: section

---

# Exercise 1

---

# Exercise 1: Page Fault Handling

**Flow when a user process accesses an unmapped page:**

```
usertrap()           ← trap entry (kernel/trap.c)
  └─ r_scause()      ← read RISC-V scause register
       13 = load page fault
       15 = store page fault
  └─ vmfault()       ← your handler (kernel/vm.c)
       └─ walk PTE
       └─ decide: COW? lazy? invalid?
       └─ allocate / remap as needed
```

- `scause == 13`: load fault (read to unmapped address)
- `scause == 15`: store fault (write to read-only or unmapped address)
- `r_stval()` returns the faulting virtual address

**Task**: Read `usertrap()` and identify where page faults are dispatched.

---

layout: section

---

# Exercise 2

---

# Exercise 2: COW Fork — Concept

**Problem with naive `fork()`**: copies all physical pages immediately — slow and wasteful.

**COW idea**: share parent's pages read-only; copy only when a write occurs.

```
fork()
  ├─ map child pages → same physical frames as parent
  ├─ mark both parent & child PTEs as read-only
  └─ set PTE_COW bit (RSW field) on both sides

write access by child
  └─ store page fault (scause = 15)
  └─ vmfault() detects PTE_COW
  └─ allocate new physical page
  └─ copy contents → remap child PTE writable
  └─ decrement refcount on old page
```

- Most `fork()` children call `exec()` immediately — no copy ever needed
- COW makes `fork()` nearly free in the common case

---

# Exercise 2: COW Fork — Implementation

**Step 1 — Mark PTEs during `uvmcopy()`**

```c
// For each PTE in parent's address space:
pte = walk(pagetable, va, 0);
*pte &= ~PTE_W;          // clear writable
*pte |= PTE_COW;         // set COW flag (RSW bit, e.g. bit 8)
// child PTE gets same flags
```

**Step 2 — Reference counting**

```c
int refcount[PHYSTOP / PGSIZE];   // one entry per physical page
// increment on fork, decrement on free
// only kfree() when refcount drops to 0
```

**Step 3 — Fault handler in `vmfault()`**

```c
if (*pte & PTE_COW) {
    char *mem = kalloc();
    memmove(mem, (char*)PA2KV(PTE2PA(*pte)), PGSIZE);
    *pte = PA2PTE((uint64)mem) | flags | PTE_W;
    *pte &= ~PTE_COW;
    decref(old_pa);
}
```

---

layout: section

---

# Exercise 3

---

# Exercise 3: Lazy Allocation

**Problem**: `sbrk(n)` immediately allocates `n` bytes of physical memory even if the process never uses it.

**Lazy allocation idea**: just grow `p->sz` in `sbrk()` — do not allocate physical pages yet.

```
sbrk(n)
  └─ p->sz += n          ← record new size, return old sz
  └─ NO physical alloc

first access to new page
  └─ load/store page fault
  └─ vmfault() checks: va < p->sz && va >= p->oldsz?
       └─ yes → kalloc() + mappages()
       └─ no  → kill process (invalid access)
```

**Edge cases to handle**:
- `sbrk(-n)`: must `uvmdealloc()` existing pages
- Stack growth below guard page must still fault fatally
- `read()`/`write()` syscalls that pass lazy pages to the kernel (`walkaddr()` must trigger allocation)

---

# Key Takeaways

**Page faults as a mechanism**
- `usertrap()` dispatches on `scause`; `vmfault()` is where the real work happens
- The faulting VA is always available via `r_stval()`

**COW Fork**
- Mark pages read-only + `PTE_COW` (RSW bit) at `fork()` time
- Fault on first write: allocate, copy, remap writable
- Reference counting prevents premature `kfree()`

**Lazy Allocation**
- `sbrk()` updates `p->sz` only; physical pages allocated on demand
- Reduces startup cost for programs that over-allocate heap space

**Common pitfall**: forgetting to handle kernel access to lazy pages — `walkaddr()` must check and allocate before passing addresses to device drivers or syscall I/O paths.

---
theme: default
title: "Week 05 — Lab: Thread & Concurrency 2 — Advanced Synchronization"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 5 — Advanced Synchronization

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science
</div>

---

# Lab Overview

**Topic:** Thread & Concurrency 2 — Advanced Synchronization

**Duration:** ~50 minutes · 4 exercises

**Objectives**

- Implement the Producer-Consumer pattern with condition variables and a mutex
- Scale to multiple producers and consumers on a bounded buffer
- Read and explain every line of `xv6-riscv/kernel/spinlock.c`
- Analyze lock usage in `xv6-riscv/kernel/kalloc.c` and understand the scalability bottleneck

**Setup**

```bash
cd examples/
gcc -Wall -pthread -o producer_consumer  producer_consumer.c
gcc -Wall -pthread -o bounded_buffer     bounded_buffer.c
# xv6 source: xv6-riscv/kernel/spinlock.c
# xv6 source: xv6-riscv/kernel/kalloc.c
```

---
layout: section
---

# Exercise 1
## Producer-Consumer

---

# Exercise 1: Producer-Consumer

**Goal:** Coordinate producers and consumers with a condition variable + mutex

**Core pattern — producer side:**

```c
pthread_mutex_lock(&buf->mutex);

while (buf->count == BUFFER_SIZE)        // buffer full?
    pthread_cond_wait(&buf->not_full, &buf->mutex);  // sleep + release lock atomically

buf->data[buf->in] = item;
buf->in = (buf->in + 1) % BUFFER_SIZE;
buf->count++;

pthread_cond_signal(&buf->not_empty);    // wake one waiting consumer
pthread_mutex_unlock(&buf->mutex);
```

**Why `while`, not `if`?**

POSIX uses **Mesa semantics** — a thread can wake up spuriously or find the condition already consumed by a faster thread. Always recheck the predicate after waking.

**`signal` vs `broadcast`:** `signal` wakes one thread (efficient); `broadcast` wakes all (safer when multiple threads must recheck).

---
layout: section
---

# Exercise 2
## Bounded Buffer

---

# Exercise 2: Multi-Producer/Consumer Bounded Buffer

**Goal:** Verify correctness and explore synchronization dynamics at scale

```bash
./bounded_buffer   # 3 producers + 3 consumers, BUFFER_SIZE = 4
```

Every item produced must be consumed exactly once — no duplicates, no drops.

**Experiments to try:**

| Change | Expected effect |
|---|---|
| Replace `while` with `if` | Assertion failures or wrong count |
| Replace `signal` with `broadcast` | Correct but more spurious wakeups |
| Set `BUFFER_SIZE = 1` | Strict alternation, low concurrency |
| Reduce consumers to 1 | Consumer becomes bottleneck, producers wait more |

**Key observation:** a small buffer relative to thread count causes frequent waiting — visible in the terminal output interleaving.

---
layout: section
---

# Exercise 3
## xv6 spinlock.c Analysis

---

# Exercise 3: xv6 spinlock.c Analysis

**File:** `xv6-riscv/kernel/spinlock.c`

**`acquire` — four steps:**

```c
void acquire(struct spinlock *lk) {
    push_off();                                          // 1. disable interrupts
    if (holding(lk)) panic("acquire");                  // 2. re-entrancy check

    while (__sync_lock_test_and_set(&lk->locked, 1))   // 3. atomic spin
        ;
    __sync_synchronize();                               // 4. full memory barrier
    lk->cpu = mycpu();
}
```

**`release` — order matters:**

```c
lk->cpu = 0;
__sync_synchronize();              // barrier BEFORE releasing lock
__sync_lock_release(&lk->locked);  // atomic clear
pop_off();                         // re-enable interrupts
```

**`push_off` / `pop_off` — nestable interrupt disable:**

- `noff` counter — increments per `acquire`, decrements per `release`
- Interrupts only restored when `noff` reaches 0 **and** were enabled before the first lock
- Prevents self-deadlock: an interrupt handler cannot spin on a lock held by interrupted code

---
layout: section
---

# Exercise 4
## xv6 kalloc.c Lock Analysis

---

# Exercise 4: xv6 kalloc.c Lock Analysis

**File:** `xv6-riscv/kernel/kalloc.c`

**Data structure:**

```c
struct {
    struct spinlock lock;
    struct run *freelist;   // singly-linked list of free pages
} kmem;                     // one global instance — all CPUs share it
```

**`kalloc` and `kfree` — minimal critical sections:**

```c
// kfree: memset BEFORE acquiring lock (page not yet visible to others)
memset(pa, 1, PGSIZE);
acquire(&kmem.lock);
r->next = kmem.freelist;  kmem.freelist = r;
release(&kmem.lock);

// kalloc: memset AFTER releasing lock (page already removed from list)
acquire(&kmem.lock);
r = kmem.freelist;  if(r) kmem.freelist = r->next;
release(&kmem.lock);
memset((char*)r, 5, PGSIZE);
```

**Scalability problem:** one lock for all CPUs — every allocation serializes.

**Per-CPU optimization (assignment):** give each CPU its own freelist + lock so cores allocate in parallel most of the time, stealing from neighbors only when empty.

---

# Key Takeaways

**Condition variables** solve producer-consumer coordination:
- Pattern: `mutex` + `cond_wait` in a `while` loop — always recheck the predicate.

**xv6 spinlock** requirements beyond a simple atomic spin:
- Disable interrupts (`push_off`) to prevent interrupt-handler deadlock.
- Memory barriers (`__sync_synchronize`) to prevent CPU/compiler reordering.
- `noff` counter for correct nesting of multiple locks.

**Lock granularity matters:**
- A single global lock (`kmem.lock`) is simple but creates a bottleneck under contention.
- Per-CPU data structures with per-CPU locks allow parallel execution.

**Minimize work inside critical sections** — `kalloc.c` deliberately places `memset` outside the lock.

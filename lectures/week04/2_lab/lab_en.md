---
theme: default
title: "Week 04 — Lab: Thread & Concurrency 1 — Race Conditions and Locks"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 4 — Race Conditions and Locks

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science
</div>

---

# Lab Overview

**Topic:** Thread & Concurrency 1 — Race Conditions and Locks

**Duration:** ~50 minutes · 4 labs

**Objectives**

- Observe non-deterministic behavior caused by unsynchronized shared state
- Protect critical sections with `pthread_mutex`
- Understand spinlock mechanics using atomic operations (xv6 model)
- Reproduce a deadlock and identify the four Coffman conditions

**Setup**

```bash
cd examples/
gcc -Wall -pthread -o race_demo      race_demo.c
gcc -Wall -pthread -o mutex_fix      mutex_fix.c
gcc -Wall -pthread -o spinlock_impl  spinlock_impl.c
gcc -Wall -pthread -o deadlock_demo  deadlock_demo.c
```

---
layout: section
---

# Lab 1
## Race Conditions

---

# Lab 1: Race Conditions

**Goal:** Observe that `counter++` is not atomic

```bash
./race_demo          # 4 threads × 1,000,000 increments
```

Expected: `4,000,000` — Actual: something less, different every run

**Why?** `counter++` compiles to three CPU instructions:

```
LOAD  reg ← counter
ADD   reg ← reg + 1
STORE counter ← reg
```

Two threads can interleave these steps and overwrite each other's write — a **lost update**.

**Experiment**

```bash
./race_demo 2 1000000   # fewer threads → fewer lost updates
./race_demo 8 1000000   # more threads  → more lost updates
```

Key insight: the output is **non-deterministic** — adding print statements changes the timing.

---
layout: section
---

# Lab 2
## Mutex Protection

---

# Lab 2: Mutex Protection

**Goal:** Eliminate the race with `pthread_mutex`

```bash
./mutex_fix   # always prints 4,000,000
```

**The fix — three lines around the critical section:**

```c
pthread_mutex_lock(&lock);
counter++;                   // critical section
pthread_mutex_unlock(&lock);
```

- `lock()` — if mutex is free, acquire it; otherwise **sleep** until it is free
- `unlock()` — release the mutex and wake one waiting thread

**Performance trade-off**

```bash
time ./race_demo  4 1000000   # fast but wrong
time ./mutex_fix  4 1000000   # correct but slower
```

Rule of thumb: keep the critical section **as small as possible** to minimize contention.

---
layout: section
---

# Lab 3
## Spinlock

---

# Lab 3: Spinlock (xv6 Model)

**Goal:** Understand atomic test-and-set and why kernels prefer spinlocks

**User-space spinlock core:**

```c
// acquire
while (__sync_lock_test_and_set(&lock->locked, 1) != 0)
    ;   // spin — burn CPU until the lock is free

// release
__sync_lock_release(&lock->locked);
```

**xv6 `kernel/spinlock.c` — additional requirements:**

```c
void acquire(struct spinlock *lk) {
    push_off();   // disable interrupts (prevent self-deadlock)
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;
    __sync_synchronize();   // full memory barrier
    lk->cpu = mycpu();
}
```

| | Mutex | Spinlock |
|---|---|---|
| Waiting thread | Sleeps (no CPU) | Spins (burns CPU) |
| Best for | Long critical sections | Very short kernel sections |

---
layout: section
---

# Lab 4
## Deadlock

---

# Lab 4: Deadlock Scenario

**Goal:** Observe deadlock and recognize the four Coffman conditions

```bash
./deadlock_demo   # may hang — kill with: kill -9 $(pgrep deadlock_demo)
```

**What happens:**

| Thread A | Thread B |
|---|---|
| acquire `lock1` | acquire `lock2` |
| … wait for `lock2` | … wait for `lock1` |

Neither can proceed. **Circular wait.**

**Four Coffman conditions** (all must hold for deadlock):

1. **Mutual exclusion** — only one thread may hold a lock at a time
2. **Hold and wait** — a thread holds one lock while requesting another
3. **No preemption** — locks cannot be forcibly taken away
4. **Circular wait** — a cycle exists in the dependency graph

**Fix:** enforce a global lock-acquisition order — every thread acquires `lock1` before `lock2`.

---

# Key Takeaways

**Race conditions** arise from unsynchronized access to shared mutable state.

**Mutex** (`pthread_mutex`) puts waiting threads to sleep — correct and efficient for user-space.

**Spinlock** busy-waits using atomic instructions — preferred in kernels for very short critical sections.
- xv6 adds `push_off()` (disable interrupts) + `__sync_synchronize()` (memory barrier)

**Deadlock** requires all four Coffman conditions — break any one to prevent it.
- Most practical fix: enforce a consistent **lock-ordering** across all threads.

**You will see these in xv6:**
- `kernel/spinlock.c` — `acquire` / `release`
- `kernel/kalloc.c` — memory allocator guarded by a spinlock
- `kernel/proc.c` — process table locked per-entry

---
theme: default
title: "Week 10 — Lab: Deadlocks"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 10 — Deadlocks

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science
</div>

---

# Lab Overview

**Topic**: Observing and Resolving Deadlocks — pthread & xv6 kernel

**Objectives**
- Directly observe a deadlock using `pthread` mutexes and identify the 4 necessary conditions
- Fix deadlock by enforcing a consistent global lock acquisition order
- Apply the `trylock` + back-off strategy as an alternative avoidance technique
- Analyze how the xv6 kernel enforces lock ordering (`wait_lock → p->lock`)

**Exercises**

| # | Topic | Time |
|---|-------|------|
| 1 | Observe deadlock with `pthread_mutex` | 10 min |
| 2 | Fix with lock ordering | 10 min |
| 3 | `trylock` + back-off strategy | 15 min |
| 4 | xv6 lock ordering analysis | 15 min |

---
layout: section
---

# Exercise 1
## Observe Deadlock

---

# Exercise 1: Observe Deadlock

**Setup**: two threads, two mutexes — classic circular wait

```c
pthread_mutex_t lock_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_B = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *arg) {
  pthread_mutex_lock(&lock_A);   // acquires A
  sleep(1);                      // yield to let thread2 acquire B
  pthread_mutex_lock(&lock_B);   // waits for B — held by thread2
  // ... critical section ...
  pthread_mutex_unlock(&lock_B);
  pthread_mutex_unlock(&lock_A);
  return NULL;
}

void *thread2(void *arg) {
  pthread_mutex_lock(&lock_B);   // acquires B
  sleep(1);
  pthread_mutex_lock(&lock_A);   // waits for A — held by thread1
  // ... critical section ...
  pthread_mutex_unlock(&lock_A);
  pthread_mutex_unlock(&lock_B);
  return NULL;
}
```

**Observe**: run `deadlock_demo` — the program hangs indefinitely

- All 4 Coffman conditions are satisfied: mutual exclusion, hold & wait, no preemption, circular wait
- Use `Ctrl-C` to kill the process; there is no automatic recovery

---
layout: section
---

# Exercise 2
## Lock Ordering Fix

---

# Exercise 2: Lock Ordering Fix

**Principle**: establish a **global total order** on all locks; always acquire in that order

```c
// Bad — inconsistent order causes circular wait
// Thread 1: lock(A) → lock(B)
// Thread 2: lock(B) → lock(A)

// Good — consistent order eliminates circular wait
// Both threads: lock(A) → lock(B)

void *thread1_fixed(void *arg) {
  pthread_mutex_lock(&lock_A);   // always A first
  pthread_mutex_lock(&lock_B);   // then B
  // ... critical section ...
  pthread_mutex_unlock(&lock_B);
  pthread_mutex_unlock(&lock_A);
  return NULL;
}

void *thread2_fixed(void *arg) {
  pthread_mutex_lock(&lock_A);   // same order: A first
  pthread_mutex_lock(&lock_B);
  // ... critical section ...
  pthread_mutex_unlock(&lock_B);
  pthread_mutex_unlock(&lock_A);
  return NULL;
}
```

**Why it works**
- Circular wait is impossible: to hold lock B, a thread must already hold lock A
- No thread can be waiting for a lock held by a thread that is waiting for it
- Release order does not matter — only acquisition order must be consistent

---
layout: section
---

# Exercise 3
## trylock Strategy

---

# Exercise 3: trylock Strategy

**`pthread_mutex_trylock`** — non-blocking attempt; returns `EBUSY` if lock is unavailable

```c
void *thread_trylock(void *arg) {
  while (1) {
    pthread_mutex_lock(&lock_A);

    if (pthread_mutex_trylock(&lock_B) == 0) {
      // Success — hold both locks
      // ... critical section ...
      pthread_mutex_unlock(&lock_B);
      pthread_mutex_unlock(&lock_A);
      break;
    }

    // Failed to acquire B — release A and back off
    pthread_mutex_unlock(&lock_A);

    // Back-off: sleep a random short interval before retrying
    usleep(rand() % 1000);
  }
  return NULL;
}
```

**Key points**
- Back-off prevents **livelock**: without it, both threads could retry in lockstep forever
- Randomized delay ensures the threads desynchronize and one succeeds
- Trade-off: livelock risk if back-off is not carefully tuned; lock ordering is generally preferred
- `trylock` is useful when lock ordering is impractical (e.g., locks in different subsystems)

---
layout: section
---

# Exercise 4
## xv6 Lock Ordering Analysis

---

# Exercise 4: xv6 Lock Ordering

**xv6 enforces a strict global lock hierarchy to prevent deadlock in the kernel**

**Example: `wait()` → `exit()` interaction**

```c
// kernel/proc.c — wait()
acquire(&wait_lock);          // (1) outer lock first
for (p = proc; ...; p++) {
  acquire(&p->lock);          // (2) per-process lock second
  if (p->state == ZOMBIE) { ... }
  release(&p->lock);
}
release(&wait_lock);

// kernel/proc.c — exit()
acquire(&wait_lock);          // same outer lock first
acquire(&p->lock);            // same per-process lock second
p->state = ZOMBIE;
wakeup(p->parent);
release(&p->lock);
release(&wait_lock);
```

**xv6 lock ordering rules (from the xv6 book)**

| Order | Lock pair | Reason |
|-------|-----------|--------|
| 1st | `wait_lock` | protects the parent-child relationship |
| 2nd | `p->lock` | protects individual process state |
| 1st | `tickslock` | timer interrupt lock — must not hold `p->lock` when acquiring |

- Violating this order (e.g., holding `p->lock` then trying `wait_lock`) would create circular wait
- `acquire()` in xv6 checks for re-entrant locking and panics on violation

---

# Key Takeaways

**Deadlock conditions (Coffman)**
- Mutual exclusion, Hold & wait, No preemption, Circular wait
- Breaking **any one** condition prevents deadlock

**Prevention — lock ordering**
- Define a global total order; always acquire locks in that order
- Eliminates circular wait by construction
- Used pervasively in the xv6 kernel (`wait_lock → p->lock`)

**Avoidance — trylock + back-off**
- Non-blocking `trylock` allows a thread to detect contention and retreat
- Randomized back-off breaks livelock symmetry
- Suitable when a strict global order cannot be enforced

**xv6 discipline**
- Every kernel code path acquires locks in a documented, consistent order
- `acquire()` panics on double-acquire of the same lock (detects mistakes early)

---
theme: default
title: "Week 09 — Lab: Synchronization — sleep/wakeup, pipe"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 9 — sleep/wakeup and Pipes

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science
</div>

---

# Lab Overview

**Topic**: xv6 Synchronization Primitives — sleep/wakeup and Pipes

**Objectives**
- Analyze xv6's `sleep()` and `wakeup()` implementation and understand the channel concept
- Trace how pipes use sleep/wakeup to implement producer-consumer coordination
- Write an inter-process communication program using xv6 pipes
- Understand the lost wakeup problem and how proper locking prevents it

**Exercises**

| # | Topic | Time |
|---|-------|------|
| 1 | sleep/wakeup code analysis | 15 min |
| 2 | Pipe implementation analysis | 15 min |
| 3 | Producer-consumer with pipes | 15 min |
| 4 | Lost wakeup problem | 5 min |

---
layout: section
---

# Exercise 1
## sleep/wakeup Code Analysis

---

# Exercise 1: sleep/wakeup

**Key concepts**: sleep channel, wakeup broadcast, lock protocol

**`sleep(chan, lk)`** — `kernel/proc.c`

```c
void sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();
  acquire(&p->lock);   // (1) acquire process lock
  release(lk);         // (2) release condition lock
  p->chan = chan;
  p->state = SLEEPING; // (3) mark as sleeping
  sched();             // (4) yield CPU to scheduler
  p->chan = 0;
  release(&p->lock);
  acquire(lk);         // (5) re-acquire condition lock on wakeup
}
```

**`wakeup(chan)`** — scans entire process table, sets matching `SLEEPING` processes to `RUNNABLE`

- Channel = an arbitrary address used as an event identifier (e.g., `&pi->nread`)
- `wakeup` is a **broadcast**: all processes sleeping on `chan` are woken up
- Callers must re-check the condition after wakeup (spurious wakeup is possible)

---
layout: section
---

# Exercise 2
## Pipe Implementation Analysis

---

# Exercise 2: Pipe Implementation

**Pipe buffer** — circular buffer of 512 bytes (`kernel/pipe.c`)

```c
struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];  // PIPESIZE = 512
  uint nread;           // total bytes read (monotonically increasing)
  uint nwrite;          // total bytes written
  int readopen;
  int writeopen;
};
// index = nread % PIPESIZE  |  full: nwrite == nread + PIPESIZE
```

**`pipewrite` / `piperead` sleep/wakeup relationship**

| Situation | sleep channel | Woken up by |
|-----------|--------------|-------------|
| Writer: buffer full | `&pi->nwrite` | Reader calls `wakeup(&pi->nwrite)` |
| Reader: buffer empty | `&pi->nread` | Writer calls `wakeup(&pi->nread)` |

- Both sides hold `pi->lock` before checking the condition
- `sleep()` atomically releases `pi->lock` while transitioning to `SLEEPING`
- EOF: when write end closes, `piperead` exits the wait loop and returns 0

---
layout: section
---

# Exercise 3
## Producer-Consumer with Pipes

---

# Exercise 3: Producer-Consumer with Pipes

**Pattern**: parent = producer (writes), child = consumer (reads)

```c
int fds[2];
pipe(fds);
int pid = fork();

if (pid == 0) {
  // Child — Consumer
  close(fds[1]);              // close unused write end
  char buf[64];
  int n;
  while ((n = read(fds[0], buf, sizeof(buf))) > 0)
    printf("[Consumer] %s\n", buf);
  close(fds[0]);
  exit(0);
} else {
  // Parent — Producer
  close(fds[0]);              // close unused read end
  for (int i = 0; i < 5; i++)
    write(fds[1], "hello from producer", 19);
  close(fds[1]);              // signals EOF to consumer
  wait(0);
  exit(0);
}
```

**Key points**
- Close the unused pipe end in each process immediately after `fork()`
- Producer closing `fds[1]` causes consumer's `read()` to return 0 (EOF)
- If consumer runs first, it blocks in `piperead` until producer writes

---
layout: section
---

# Exercise 4
## Lost Wakeup Problem

---

# Exercise 4: Lost Wakeup Problem

**The problem**: a wakeup can be lost if the process is not yet sleeping

```
Time  Reader (CPU 0)               Writer (CPU 1)
  1   check: buffer empty → true
  2   release(&pi->lock)           acquire(&pi->lock)
  3                                write data
  4                                wakeup(&pi->nread)  ← reader not SLEEPING yet!
  5                                release(&pi->lock)
  6   broken_sleep(&pi->nread)     ← sleeps forever, no one to wake it
```

**xv6's fix**: pass the condition lock to `sleep()`

```c
// Inside sleep():
acquire(&p->lock);   // hold p->lock ...
release(lk);         // ... before releasing condition lock
p->state = SLEEPING; // state is set while p->lock is still held
sched();             // scheduler releases p->lock only after context switch
```

- Writer's `wakeup()` must `acquire(&p->lock)` to change `p->state`
- Because `p->lock` is held until after `p->state = SLEEPING`, there is **no gap**
- "Check condition → go to sleep" is effectively **atomic** under `p->lock`

---

# Key Takeaways

**sleep/wakeup**
- `sleep(chan, lk)` — releases `lk` atomically with entering `SLEEPING` state
- `wakeup(chan)` — broadcasts to all processes sleeping on `chan`
- Channel is any address that uniquely identifies a wait condition

**Pipes**
- Circular buffer with `nread` / `nwrite` counters
- Automatic flow control: full buffer blocks writer; empty buffer blocks reader
- EOF detected when `writeopen == 0` and buffer is empty

**Lost wakeup prevention**
- Always pass the condition lock to `sleep()`
- xv6 rule: `acquire(p->lock)` before `release(lk)` inside `sleep()`
- The lock overlap closes the race window between condition check and sleep

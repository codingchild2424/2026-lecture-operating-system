---
theme: default
title: "Week 06 — Lab: CPU Scheduling — Context Switching"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 6 — Context Switching

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science
</div>

---

# Lab Overview

**Topic:** CPU Scheduling — Context Switching in xv6

**Duration:** ~50 minutes · 4 exercises

**Objectives**

- Read `swtch.S` and explain what each instruction does and why
- Instrument the xv6 scheduler with `printf` to observe scheduling decisions live
- Trace round-robin behavior with multiple competing processes
- Add sleep/wakeup trace output and follow a process through blocking and unblocking

**Prerequisites**

```bash
# Working QEMU + xv6 build environment required
cd xv6-riscv
make qemu   # verify clean boot before starting
```

---
layout: section
---

# Exercise 1
## swtch.S Analysis

---

# Exercise 1: swtch.S Analysis

**Files:** `kernel/swtch.S`, `kernel/proc.h`

**`struct context` — only callee-saved registers:**

```c
struct context {
  uint64 ra;   uint64 sp;           // return address + stack pointer
  uint64 s0;   uint64 s1;           // callee-saved: s0 – s11
  /* ... s2 through s11 ... */
};
```

Caller-saved registers (`t0`–`t6`, `a0`–`a7`) are already on the stack — no need to save them again.

**`swtch(old, new)` — save current, load next:**

```asm
swtch:
    sd ra, 0(a0)  ;  sd sp, 8(a0)  ;  sd s0, 16(a0) ...  # save → old context
    ld ra, 0(a1)  ;  ld sp, 8(a1)  ;  ld s0, 16(a1) ...  # load ← new context
    ret                                                     # jump to new->ra
```

`ret` jumps to the `ra` loaded from the **new** context — execution resumes inside the new process where it last called `swtch`.

New process bootstrap: `allocproc` sets `p->context.ra = (uint64)forkret`.

---
layout: section
---

# Exercise 2
## Scheduler Tracing

---

# Exercise 2: Scheduler Tracing

**Goal:** Make the scheduler visible with `printf`

**Edit `kernel/proc.c` — inside `scheduler()`:**

```c
if (p->state == RUNNABLE) {
    printf("[sched] cpu%d: switch to pid=%d name=%s\n",
           cpuid(), p->pid, p->name);   // add this line
    p->state = RUNNING;
    c->proc = p;
    swtch(&c->context, &p->context);
    c->proc = 0;
    found = 1;
}
```

Alternatively, apply the provided patch: `git apply scheduler_trace.patch`

**Build and observe:**

```bash
make clean && make CPUS=1 qemu   # single CPU for readable output
```

**What to look for:**
- pid 1 (`init`) is scheduled first — confirm this
- After the shell prompt appears, scheduling output should stop — the shell is **sleeping** waiting for keyboard input (SLEEPING state, not RUNNABLE)

---
layout: section
---

# Exercise 3
## Round-Robin Observation

---

# Exercise 3: Round-Robin Observation

**Goal:** See the round-robin policy in action with competing processes

**Start multiple background processes in the xv6 shell:**

```
$ spin &
$ spin &
$ spin &
```

**Expected trace output:**

```
[sched] cpu0: switch to pid=3 name=spin
[sched] cpu0: switch to pid=4 name=spin
[sched] cpu0: switch to pid=5 name=spin
[sched] cpu0: switch to pid=3 name=spin
...
```

**Why round-robin?** The scheduler scans `proc[]` linearly from index 0:

```c
for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == RUNNABLE) { /* run it */ }
}
```

**Subtle bias:** after a context switch, the loop resumes scanning from the beginning — lower-index processes are checked first every cycle.

**Multi-CPU experiment:** rebuild with `CPUS=3` — multiple CPUs pick up different processes simultaneously, producing interleaved output.

---
layout: section
---

# Exercise 4
## sleep/wakeup Tracing

---

# Exercise 4: sleep/wakeup Tracing

**Goal:** Follow a process as it blocks on a pipe and gets unblocked

**Add trace to `kernel/proc.c` — `sleep()` function:**

```c
printf("[sleep]  pid=%d name=%s chan=%p\n", p->pid, p->name, chan);
p->chan = chan;
p->state = SLEEPING;
sched();
p->chan = 0;
printf("[wakeup] pid=%d name=%s\n", p->pid, p->name);
```

**Test in xv6 shell:**

```
$ echo hello | cat
```

**Expected output sequence:**

```
[sleep]  pid=3 name=cat  chan=0x...   ← cat waits: pipe is empty
[sched]  cpu0: switch to pid=4 name=echo
[wakeup] pid=3 name=cat              ← echo wrote data; cat is RUNNABLE
[sched]  cpu0: switch to pid=3 name=cat
hello
```

`wakeup` only changes state to RUNNABLE — the woken process does **not** run immediately.

**Cleanup:** `git checkout kernel/proc.c` (or reverse-apply patch) before next week.

---

# Key Takeaways

**`swtch.S`** saves callee-saved registers to `old->context` and loads them from `new->context`. The final `ret` jumps to `new->ra`, resuming the new process exactly where it last called `swtch`.

**Scheduler loop** is a linear scan of `proc[]` for RUNNABLE processes — simple round-robin with a low-index bias.

**Round-robin** ensures fairness: every RUNNABLE process gets a turn before any process gets a second turn (within one scan of the table).

**sleep/wakeup** is xv6's blocking mechanism:
- `sleep(chan, lk)` — set state to SLEEPING, call `sched()`; atomically releases `lk` to avoid lost wakeups
- `wakeup(chan)` — scan all processes; set matching SLEEPING processes to RUNNABLE

**End-to-end flow:** `yield` → `sched` → `swtch` (to scheduler) → `swtch` (to next process) → resume

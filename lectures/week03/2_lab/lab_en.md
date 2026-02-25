---
theme: default
title: "Week 03 — Lab: Process 2 — Exploring xv6 Internals"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 3 — Exploring xv6 Internals

---

layout: section

---

# Lab Overview

---

# Lab Overview

- **Goal**: Navigate the xv6 kernel source and trace how process-related system calls work end-to-end
- **Duration**: ~50 minutes
- **Exercises**: 5 labs (Lab 0 through Lab 4)
- **Topics covered**:
  - xv6 build environment and QEMU
  - Kernel source directory layout
  - System call path from user space to kernel
  - `struct proc` — the process descriptor
  - `fork()` implementation line by line

---

layout: section

---

# Labs 0 & 1

---

# Lab 0 & 1: Environment Check & Source Structure

**Lab 0 — Verify your build**
```bash
cd xv6-riscv
make qemu         # should boot xv6 in QEMU
# Expected: "xv6 kernel is booting" followed by a shell prompt ($)
# Exit with: Ctrl-A X
```

**Lab 1 — Key files in `kernel/`**

| File | Purpose |
|---|---|
| `proc.h` | `struct proc` definition |
| `proc.c` | process management (fork, exit, wait, scheduler) |
| `syscall.c` | dispatch table — maps syscall numbers to handlers |
| `sysproc.c` | syscall handler implementations (sys_fork, sys_exit, …) |
| `trap.c` | trap/interrupt entry point from user space |
| `usys.pl` | generates `usys.S` — user-space syscall stubs |

- Spend a few minutes reading each file's top-level comments

---

layout: section

---

# Lab 2

---

# Lab 2: System Call Tracing

**Full path of a `fork()` call from user space to kernel**

```
User program calls fork()
        │
        ▼
usys.S  — li a7, SYS_fork  (load syscall number into register a7)
           ecall            (trap into supervisor mode)
        │
        ▼
trap.c  — usertrap()        (handles all traps from user space)
           calls syscall()
        │
        ▼
syscall.c — syscall()       (reads a7, looks up dispatch table)
             calls sys_fork()
        │
        ▼
sysproc.c — sys_fork()      (thin wrapper, calls fork())
        │
        ▼
proc.c  — fork()            (does the actual work)
```

- Trace this path by opening each file and finding the relevant function
- Add a `printf` in `sys_fork()` and rebuild to confirm you found the right spot

---

layout: section

---

# Lab 3

---

# Lab 3: struct proc Analysis

**Defined in `kernel/proc.h`**

```c
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;   // UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
  void *chan;             // If non-zero, sleeping on chan
  int killed;             // If non-zero, have been killed
  int xstate;             // Exit status to be returned to parent's wait
  int pid;                // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;    // Parent process

  // private to the process, so p->lock need not be held:
  uint64 kstack;          // Virtual address of kernel stack
  uint64 sz;              // Size of process memory (bytes)
  pagetable_t pagetable;  // User page table
  struct trapframe *trapframe; // data for trampoline.S
  struct context context; // swtch() here to run process
  struct file *ofile[NOFILE]; // Open files
  struct inode *cwd;      // Current directory
  char name[16];          // Process name (debugging)
};
```

- Identify which fields change at each lifecycle transition: UNUSED → RUNNABLE → RUNNING → ZOMBIE

---

layout: section

---

# Lab 4

---

# Lab 4: fork() Implementation Deep Dive

**`fork()` in `kernel/proc.c` — step by step**

```c
int fork(void) {
  // 1. Allocate a new process slot (allocproc)
  //    Sets state = USED, allocates pid, kstack, trapframe, context

  // 2. Copy the parent's user memory into the child
  uvmcopy(p->pagetable, np->pagetable, p->sz);
  np->sz = p->sz;

  // 3. Copy the trapframe so the child returns from fork() too
  *(np->trapframe) = *(p->trapframe);

  // 4. Make fork() return 0 in the child
  np->trapframe->a0 = 0;

  // 5. Copy open file descriptors
  for (int i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);

  // 6. Set parent pointer and mark RUNNABLE
  np->parent = p;
  np->state = RUNNABLE;

  return np->pid;  // returns child PID to parent
}
```

- Why does the child need its own trapframe copy?
- What would happen if step 4 were skipped?

---

# Key Takeaways

**xv6 as a learning OS**
- Small enough to read entirely (~10 k lines of C)
- Every concept from lecture has a direct, readable implementation

**System call path**
- User code → `ecall` → `usertrap()` → `syscall()` → handler → implementation
- Understanding this path helps debug kernel issues at any layer

**`struct proc` is the kernel's view of a process**
- All scheduling, memory, and file state hangs off this structure
- Lifecycle: UNUSED → USED → RUNNABLE → RUNNING → ZOMBIE → UNUSED

**fork() key insight**
- `uvmcopy` does the heavy lifting — full page-table duplication
- Setting `trapframe->a0 = 0` is the entire reason the child sees return value 0

> Upcoming: threads, scheduling, and synchronization — all built on top of what you explored today.

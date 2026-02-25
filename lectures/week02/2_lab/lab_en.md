---
theme: default
title: "Week 02 — Lab: Process 1 — fork, exec, wait, pipe"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 2 — Process System Calls

---

layout: section

---

# Lab Overview

---

# Lab Overview

- **Goal**: Practice core UNIX process system calls through hands-on C programs
- **Duration**: ~50 minutes
- **Exercises**: 4 labs
- **Topics covered**:
  - `fork()` — process duplication
  - `exec()` — replace process image
  - `wait()` — parent/child synchronization
  - `pipe()` — inter-process communication
  - `dup()` — I/O redirection via file descriptors

---

layout: section

---

# Lab 1

---

# Lab 1: fork() and wait()

**Concept**: `fork()` duplicates the calling process. The child gets a copy of the parent's address space.

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        printf("Child PID: %d\n", getpid());
    } else {
        // Parent process
        wait(NULL);   // block until child exits
        printf("Parent: child finished\n");
    }
    return 0;
}
```

- `fork()` returns **0** to the child, **child PID** to the parent
- `wait(NULL)` blocks the parent until any child terminates
- Try: what happens if you remove `wait()`?

---

layout: section

---

# Lab 2

---

# Lab 2: exec()

**Concept**: `exec()` replaces the current process image with a new program. The PID does not change.

```c
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Before exec\n");

    char *args[] = {"ls", "-l", NULL};
    execvp("ls", args);

    // This line is never reached if exec succeeds
    printf("After exec — should not print\n");
    return 0;
}
```

- Common variants: `execl`, `execv`, `execvp`, `execve`
- Combine with `fork()` to run a subprocess without replacing the parent
- Exercise: fork a child, then exec `echo "hello from child"` in the child

---

layout: section

---

# Lab 3

---

# Lab 3: pipe()

**Concept**: `pipe()` creates a unidirectional communication channel between two file descriptors.

```c
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd[2];
    pipe(fd);          // fd[0] = read end, fd[1] = write end

    if (fork() == 0) {
        // Child: producer
        close(fd[0]);
        char *msg = "hello from child";
        write(fd[1], msg, strlen(msg));
        close(fd[1]);
    } else {
        // Parent: consumer
        close(fd[1]);
        char buf[64] = {0};
        read(fd[0], buf, sizeof(buf));
        printf("Parent received: %s\n", buf);
        close(fd[0]);
    }
    return 0;
}
```

- Always close the unused end in each process
- Models the **producer-consumer** pattern

---

layout: section

---

# Lab 4

---

# Lab 4: dup() and I/O Redirection

**Concept**: `dup2(oldfd, newfd)` makes `newfd` refer to the same file as `oldfd`. Used to redirect stdin/stdout.

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    // Redirect stdout (fd 1) to output.txt
    dup2(fd, STDOUT_FILENO);
    close(fd);

    printf("This goes to output.txt, not the terminal\n");
    return 0;
}
```

- This is how shells implement `ls > file.txt`
- Exercise: combine `fork()` + `pipe()` + `dup2()` to implement a pipe between two commands, e.g., `ls | wc -l`

---

# Key Takeaways

**Process lifecycle**
- `fork()` creates a child — both processes run from the same point
- `exec()` loads a new program — the old image is gone
- `wait()` reaps the child and prevents zombie processes

**IPC mechanisms**
- `pipe()` is the simplest one-way channel between related processes
- `dup2()` lets you wire pipes to standard file descriptors

**Shell internals**
- Every shell command you run uses these exact syscalls
- `cmd1 | cmd2` = fork + pipe + exec (twice) + dup2 + wait

> Next week: we go inside xv6 and read the kernel source that implements these calls.

# Week 01 Theory — Lecture Script

---

## Slide 1: Operating Systems (Title)

Welcome to the Operating Systems course. Throughout this semester, we'll study how operating systems work — not just theory, but also reading and modifying real OS code. Today in Week 1, we'll start with the orientation, then take a big-picture look at what an operating system actually is.

---

## Slide 2: Part 1. OT

Alright, let's begin with the course logistics.

---

## Slide 3: Instructor

I'm Unggi Lee. I'm an assistant professor in the Department of Computer Science here at Korea University Sejong Campus. Before academia, I worked as an AI and NLP engineer at companies called Enuma and I-Scream Edu, building educational AI products. And before that, I was actually an elementary school teacher for about eight years — so I've been in education for a long time, just from very different angles. My research focuses on generative AI in education — large language models, pedagogical alignment, knowledge tracing, and more recently vision-language-action models, or VLA. While I was at Chosun University, I actually published a VLA preprint together with undergraduate students there. If any of you join my lab, you'll have the opportunity to collaborate with those Chosun University students as well as with PhD students and professors overseas — mainly in the US, Singapore, and the UK. We also have ongoing industry–academia partnerships with companies like Upstage and Newdive, so there are opportunities on that front too. You can check out the lab website linked on this slide. For publications, there's my Google Scholar page, and for any questions, reach me at codingchild@korea.ac.kr.

---

## Slide 4: Syllabus

The syllabus is posted on LMS — please make sure to check it. It's a 15-week course. The midterm exam is in Week 8, and the final exam is in Week 15.

---

## Slide 5: Grading

Here's the grade breakdown. Assignments are 10%, the midterm is 30%, the final written exam is 30%, and the final project is 30%. Attendance itself has no grade weight, but if you miss one-third or more of the total classes, you will receive no grade at all. So attendance is effectively mandatory.

---

## Slide 6: Assignment (10%)

Assignments are split into two parts. First, in-class quizzes — there are 10 of them, each worth 0.5%. They start from Week 3 and run through Week 7, then Week 9 through Week 13. Second, take-home assignments — there are 5 of them, each worth 1%, from Week 2 through Week 6. Neither has a huge weight individually, but you need to stay on top of them consistently.

---

## Slide 7: Midterm & Final Exam (Written)

Both the midterm and final written exams are handwritten — no laptops, tablets, or digital devices allowed. Each exam is one hour. Make sure you understand the material well, because writing by hand under time pressure means you need to have the concepts firmly in your head.

---

## Slide 8: Final Exam — Project (30%)

The final project is worth 30%, so it's a big deal. It starts from Week 9, and you'll form teams of 3 to 4 people. The most important point is that you can use coding agents without any restrictions — Claude Code, Codex, Gemini CLI, whatever you want. Each team needs to design and develop an OS prototype, write specification documentation, and give a presentation in Week 14. The evaluation is split between the professor at 15% and peer review at 15%. We'll actually try coding agents in the third hour today, so you'll get a feel for how they work.

---

## Slide 9: Class Format

Each week is 3 hours. The first and second hours are theory lectures, and in weeks that have quizzes, the quiz comes at the end of the second hour. The third hour is a hands-on lab. The textbook is Silberschatz's Operating System Concepts, 10th Edition, and for labs we'll use xv6, a teaching OS from MIT. Alright, that wraps up the OT. Let's take a short break and come back for Part 2 where we'll look at what an OS actually is.

---

## Slide 10: Part 2. What is an Operating System?

Now let's get into the substance. We'll take a big-picture look at what an operating system is. Today we're not going deep — we're surveying the entire landscape of what we'll cover this semester.

---

## Slide 11: What is an Operating System?

If you had to define an operating system in one sentence, it's the one program that's always running on the computer — the kernel. Everything else is either a system program or an application program. The textbook compares the OS to a government. A government doesn't do useful work by itself, but it provides the environment where people can get useful work done. The OS is the same — it makes sure that browsers, games, and other applications can run correctly on the hardware.

---

## Slide 12: Where the OS Sits

Looking at the layers of a computer system, the user is at the top, then application programs like browsers and editors, then the OS kernel in the middle, and hardware at the bottom. The OS sits between hardware and applications — it directly controls the hardware and provides a clean interface to applications. Process management, memory management, the file system, I/O, networking, security — all of that is what the kernel does.

---

## Slide 13: Two Roles of the OS

We can divide the OS's role into two big categories. First, Resource Allocator. The OS manages CPU time, memory, disk, and I/O devices, distributing them efficiently and fairly among programs. Second, Control Program. The OS manages the execution of user programs and prevents errors and misuse. For example, stopping one program from accessing another program's memory — that's the OS's job.

---

## Slide 14: How Does the OS Protect Itself?

Here's an important question. The OS manages hardware, but what if a user program messes with the hardware directly? The system would crash. That's why we have dual-mode operation. The CPU operates in two modes. In kernel mode, any instruction can execute. In user mode, only a restricted set of instructions is allowed. When a user program needs an OS service, it makes a system call. This triggers a trap, which switches the CPU to kernel mode. The OS performs the service and then returns to user mode. Privileged instructions like I/O control and timer management can only run in kernel mode.

---

## Slide 15: System Calls — The OS API

Let's see how a system call actually works. When a user program calls something like read, the C library receives it and issues a syscall instruction, which triggers a trap. The CPU switches to kernel mode, and the kernel's sys_read function executes. It performs disk I/O, copies data to the buffer, and then returns to user mode. The sequence diagram on the slide shows this flow clearly. Here's the fun part — even a simple file copy command triggers thousands of system calls. We'll see this firsthand in Week 2.

---

## Slide 16: How a Computer System Works

Let's briefly look at the basic hardware structure. There's a CPU and memory, and devices like disks and USB controllers connect through their own device controllers. They all access shared memory through a bus. When a device finishes its work, it sends an interrupt signal to the CPU. The CPU pauses what it's doing, handles the interrupt, and then goes back to its original task. For bulk data transfers, we use DMA — Direct Memory Access. Instead of the CPU moving bytes one by one, the device controller transfers data directly to and from memory. The CPU can do other work in the meantime.

---

## Slide 17: Storage Hierarchy

Storage devices form a hierarchy. At the top, registers are the fastest — about 0.3 nanoseconds. Cache is 1 to 25 nanoseconds, main memory is around 100 nanoseconds. SSDs are about 50 microseconds, and HDDs about 5 milliseconds. Notice the color gradient on the slide — red at the top for hot and fast, blue at the bottom for cold and slow. There's a thousand-fold gap between memory and SSD, and another hundred-fold between SSD and HDD. To bridge these speed gaps, we use caching — keeping frequently used data in faster storage. Hardware manages the registers-to-cache level, and the OS manages main memory and below. This becomes very important in Weeks 11 and 12 when we study memory management.

---

## Slide 18: OS Structures

There are several ways to design an OS. Monolithic puts everything in one kernel binary — Linux is the classic example. That's the penguin Tux you see on the right. A monolithic kernel is fast, but if one part breaks, it can affect everything. Microkernel keeps only the bare minimum in the kernel and runs everything else in user space. It's more stable but has performance overhead from message passing. In reality, most modern OSes use a hybrid approach. macOS combines the Mach microkernel with BSD. Linux is monolithic but supports loadable kernel modules. Pragmatic compromise beats theoretical purity.

---

## Slide 19: What We'll Use: xv6

For labs this semester, we'll use xv6. It's a teaching operating system built by MIT, written in C for the RISC-V architecture — that's the logo you see on the right. RISC-V is an open-source instruction set architecture, which makes it perfect for educational purposes. The entire xv6 codebase is about 10,000 lines — small enough to read in its entirety. It implements processes, virtual memory, a file system, and a shell. Throughout the semester, we'll read this code, modify it, and extend it with new features. We'll set up the build environment next week, but if you're curious, feel free to clone it and try `make qemu` ahead of time.

---

## Slide 20: Part 3. Semester Preview

Now I want to give you a taste of each major topic we'll cover this semester. Think of this as a trailer for the course — don't worry about understanding everything now. The goal is to see the big picture and get excited about what's ahead.

---

## Slide 21: Preview — Process (Week 2–3)

A process is simply a program in execution. It has its own memory, its own state, and it goes through a lifecycle. Look at the state diagram — a new process starts in the "New" state, moves to "Ready" when it's waiting for CPU time, then "Running" when the scheduler picks it. If it needs to wait for I/O, it goes to "Waiting", and when it's done, it moves to "Terminated." The key system calls are fork, exec, wait, and exit. fork creates a copy of the current process — the parent creates a child. exec replaces the process image with a new program. So the typical pattern in Unix is: fork to create a child, then exec in the child to run a different program. We'll trace all of this in xv6's kernel/proc.c file.

---

## Slide 22: Preview — Threads & Concurrency (Week 4–5)

A thread is a lightweight execution unit within a process. In a single-threaded process, there's just one flow of control. But in a multi-threaded process, multiple threads share the same code, data, and files, each with its own stack and registers. Why does this matter? Because modern CPUs have multiple cores, and threads let you use all those cores within a single process. A web browser might use one thread for rendering, another for downloading, and another for JavaScript. The challenge is that when multiple threads access shared data simultaneously, you can get race conditions — unpredictable results depending on timing. That's a huge topic we'll spend time on.

---

## Slide 23: Preview — CPU Scheduling (Week 6–7)

When multiple processes are ready to run but there's only one CPU — or even with multiple cores, there are more processes than cores — the OS needs a scheduling algorithm. The ready queue holds all processes waiting for CPU time. The scheduler picks one, dispatches it to the CPU, and when it's done or preempted, it goes back to the queue. There are different algorithms: First-Come First-Served is simple but can cause the convoy effect. Shortest Job First gives optimal average wait time but requires predicting execution time. Round Robin gives each process a fixed time slice and rotates — it's fair and gives good response time. Priority scheduling runs the highest-priority process first but risks starvation. Each has trade-offs, and we'll analyze them mathematically.

---

## Slide 24: Preview — Synchronization (Week 9)

When multiple threads share data, we need coordination to avoid race conditions. Look at this example: two threads both modify a shared balance variable. Thread A adds 100, Thread B subtracts 50. If they run concurrently without coordination, the result could be wrong — we might get 950 instead of the expected 1050, because one thread's update overwrites the other's. The solution is to protect shared data with locks — also called mutexes. Only one thread can hold the lock at a time, so accesses are serialized. We'll also learn about semaphores and condition variables for more flexible coordination. Classic problems like Producer-Consumer, Readers-Writers, and Dining Philosophers will give us practice thinking about these challenges.

---

## Slide 25: Preview — Deadlocks (Week 10)

A deadlock is when two or more processes are stuck, each waiting for the other to release a resource. Look at the diagram — Thread 1 holds Lock A and wants Lock B. Thread 2 holds Lock B and wants Lock A. Neither can proceed. This is a circular wait. For a deadlock to occur, four conditions must all hold simultaneously: mutual exclusion, hold and wait, no preemption, and circular wait. If we can break any one of these conditions, we prevent deadlock. The most common strategy is lock ordering — always acquire locks in the same global order. We'll see how xv6 enforces this discipline in its kernel.

---

## Slide 26: Preview — Memory Management (Week 11–12)

Virtual memory is one of the most elegant ideas in OS design. Each process thinks it has its own private address space, starting from address zero. But in reality, these virtual addresses are mapped to different physical locations through a page table. Look at the diagram — Process A's virtual address 0x1000 maps to physical address 0x5A000, while Process B's same virtual address 0x1000 maps to a completely different physical location 0x8B000. This provides isolation — processes can't see each other's memory. The page table also enables cool optimizations like COW fork, where forked processes share physical pages until one writes, and lazy allocation, where memory is only allocated when first accessed. In xv6, we'll work with the RISC-V Sv39 scheme — a 3-level page table with 39-bit virtual addresses.

---

## Slide 27: Preview — File System & Security (Week 13–14)

The file system is the most complex subsystem in xv6, organized in six layers. At the top, file descriptors provide the familiar open, read, write, close interface. Below that, pathnames resolve paths like /home/user/file.txt. Directories map names to inodes. Inodes store file metadata and block pointers. The logging layer provides crash safety — if the system crashes mid-write, the file system can recover to a consistent state. At the bottom, the buffer cache keeps frequently accessed disk blocks in memory. Each layer relies only on the one below it, which makes the design clean and modular. In Week 14, we'll also touch on security — protection rings, access control, and encryption.

---

## Slide 28: Course Roadmap

Here's the roadmap for the entire semester. The left side is the first half, the right side is the second half. Week 1, today, is Introduction and coding agents. Weeks 2 and 3 cover processes. Weeks 4 and 5 are threads and concurrency. Weeks 6 and 7 are CPU scheduling. After the midterm in Week 8, we cover synchronization, deadlocks, memory, file systems, and security. Project presentations are in Week 14, and the final exam is in Week 15. As you saw in the preview slides, the earlier weeks build the foundation for the later ones, so keeping up from the start is important.

---

## Slide 29: Summary

Let me wrap up. The OS is the kernel — the always-running program that manages hardware resources. Dual-mode operation separates the kernel from user programs to protect the system. System calls are the interface between applications and the kernel. This semester we'll cover processes, threads, scheduling, synchronization, memory, and file systems, and we'll use xv6 to see how a real OS works from the inside. You've also gotten a preview of each major topic — think of those as appetizers. Next week, we dive into processes — fork, exec, wait, and pipe.

---

## Slide 30: Q & A

If you have any questions, feel free to ask now or send me an email. Alright, let's take a 10-minute break and then we'll start the coding agents lab in the third hour.

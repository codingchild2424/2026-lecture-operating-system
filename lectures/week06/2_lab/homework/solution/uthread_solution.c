#include "uthread.h"  /* must be first -- defines _XOPEN_SOURCE */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global thread table */
struct thread threads[MAX_THREADS];

/* Index of the currently running thread */
int current_thread = 0;

/* ---------- Internal state ---------- */

/* The scheduler's own context.  Threads swap back to this when they
 * yield or exit.  The scheduler loop then picks the next thread. */
static ucontext_t _sched_context;

/* Per-thread startup info */
static void (*_thread_funcs[MAX_THREADS])(void *);
static void  *_thread_args[MAX_THREADS];

/*
 * _thread_wrapper: entry point for every new thread.
 *
 * makecontext requires a function taking int parameters, so we
 * pass the thread index and look up the real func/arg from tables.
 */
static void _thread_wrapper(int idx) {
    _thread_funcs[idx](_thread_args[idx]);
    /* If the user's function returns without calling thread_exit(),
     * we clean up automatically. */
    thread_exit();
    /* NOT REACHED */
}

/* ---------- Public API ---------- */

void thread_init(void) {
    memset(threads, 0, sizeof(threads));
    memset(_thread_funcs, 0, sizeof(_thread_funcs));
    memset(_thread_args, 0, sizeof(_thread_args));

    /* Thread 0 is the main/calling thread -- already running. */
    threads[0].state = T_RUNNING;
    snprintf(threads[0].name, sizeof(threads[0].name), "main");
    current_thread = 0;
}

int thread_create(void (*func)(void *), void *arg) {
    int i;

    /* Find a free slot */
    for (i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == T_FREE)
            break;
    }
    if (i == MAX_THREADS) {
        fprintf(stderr, "thread_create: no free thread slots\n");
        return -1;
    }

    struct thread *t = &threads[i];
    t->state = T_RUNNABLE;
    snprintf(t->name, sizeof(t->name), "thread_%d", i);

    _thread_funcs[i] = func;
    _thread_args[i] = arg;

    /*
     * Set up the thread's execution context:
     *
     * 1. getcontext() initializes the ucontext_t structure with the
     *    current execution state (needed before makecontext).
     *
     * 2. Configure uc_stack to use the thread's private stack so the
     *    thread runs independently from other threads.
     *
     * 3. Set uc_link to &_sched_context so that if the thread's
     *    function returns (without calling thread_exit), control
     *    transfers back to the scheduler.
     *
     * 4. makecontext() modifies the context so that when we swapcontext
     *    into it, execution starts at _thread_wrapper(i).
     */
    getcontext(&t->context);
    t->context.uc_stack.ss_sp   = t->stack;
    t->context.uc_stack.ss_size = STACK_SIZE;
    t->context.uc_link          = &_sched_context;
    makecontext(&t->context, (void (*)(void))_thread_wrapper, 1, i);

    return i;
}

void thread_yield(void) {
    /*
     * Voluntarily give up the CPU:
     *
     * 1. Mark ourselves as RUNNABLE (ready to be scheduled again).
     * 2. swapcontext saves our current state into threads[current].context
     *    and switches to _sched_context (the scheduler loop).
     * 3. When the scheduler later swaps back to us, swapcontext returns
     *    and we resume execution right after this call.
     */
    threads[current_thread].state = T_RUNNABLE;
    swapcontext(&threads[current_thread].context, &_sched_context);
}

void thread_schedule(void) {
    int last = current_thread;

    /*
     * Scheduler loop: repeatedly find the next RUNNABLE thread using
     * round-robin order and dispatch it.
     *
     * When a dispatched thread yields (state becomes RUNNABLE) or exits
     * (state becomes EXITED), swapcontext returns here and we loop to
     * find the next thread.
     */
    while (1) {
        int found = 0;

        for (int i = 1; i <= MAX_THREADS; i++) {
            int idx = (last + i) % MAX_THREADS;

            if (threads[idx].state == T_RUNNABLE) {
                /* Found a runnable thread -- dispatch it */
                threads[idx].state = T_RUNNING;
                current_thread = idx;
                found = 1;

                /*
                 * Save scheduler state into _sched_context and switch
                 * to the thread.  When the thread yields or exits,
                 * execution returns here.
                 */
                swapcontext(&_sched_context, &threads[idx].context);

                /*
                 * We're back.  The thread either:
                 *   - yielded: its state is T_RUNNABLE
                 *   - exited:  its state is T_EXITED
                 *
                 * Update 'last' to continue round-robin from where we
                 * left off, then break to restart the scan.
                 */
                last = current_thread;
                break;
            }
        }

        if (!found) {
            /* No runnable threads remain -- exit the scheduler loop */
            break;
        }
    }

    /* Restore main thread as current */
    current_thread = 0;
    threads[0].state = T_RUNNING;
}

void thread_exit(void) {
    threads[current_thread].state = T_EXITED;
    printf("[uthread] thread %d (%s) exited\n",
           current_thread, threads[current_thread].name);

    /*
     * Transfer control directly to the scheduler without saving
     * our context (the thread is dead -- no need to save state).
     * setcontext never returns.
     */
    setcontext(&_sched_context);
    /* NOT REACHED */
}

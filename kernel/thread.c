/*
 * Threading system.
 *
 * Copyright (C) 2003 Juha Aatrokoski, Timo Lilja,
 *   Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: thread.c,v 1.22 2004/01/12 09:12:22 ttakanen Exp $
 *
 */

#include "lib/libc.h"
#include "drivers/metadev.h"
#include "kernel/spinlock.h"
#include "kernel/thread.h"
#include "kernel/scheduler.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/config.h"
#include "kernel/interrupt.h"
#include "kernel/idle.h"

/** @name Thread library
 *
 * Library containing thread creation, control and destruction functions.
 *
 * @{
 */

/** Spinlock which must be held when manipulating the thread table */
spinlock_t thread_table_slock;

/** The table containing all threads in the system, whether active or not. */
thread_table_t thread_table[CONFIG_MAX_THREADS];

#ifdef CHANGED_1
/** These are protected with thread_table_slock */
/** Global variable to the next sleeper thread to be woken up. -1 if no thread is sleeping */
TID_t thread_next_sleeper_id;
#endif

/* Thread stack areas for kernel threads */
char thread_stack_areas[CONFIG_THREAD_STACKSIZE * CONFIG_MAX_THREADS];

/* Import running thread id table from scheduler */
extern TID_t scheduler_current_thread[CONFIG_MAX_CPUS];

/** Initializes the threading system. Does this by setting all thread
 *  table entry states to THREAD_FREE. Called only once before any
 *  threads are created.
 */
void thread_table_init(void)
{
    int i;

    /* Thread table entry _must_ be 64 bytes long, because the
       context switching code in kernel/cswitch.S expects that. Let's
       make sure it is. If you hit this error, you have changed either
       context_t or thread_table_t, but have not changed padding in
       the end of thread_table_t definition in kernel/thread.h */
    KERNEL_ASSERT(sizeof(thread_table_t) == 64);

    spinlock_reset(&thread_table_slock);

    #ifdef CHANGED_1
    // init sleeping variables
    thread_next_sleeper_id = -1;
    #endif

    /* Init all entries to 'NULL' */
    for (i=0; i<CONFIG_MAX_THREADS; i++) {
	/* Set context pointers to the top of the stack*/
	thread_table[i].context      = (context_t *) (thread_stack_areas
	    +CONFIG_THREAD_STACKSIZE*i + CONFIG_THREAD_STACKSIZE - 
						      sizeof(context_t));
	thread_table[i].user_context = NULL;
	thread_table[i].state        = THREAD_FREE;
	thread_table[i].sleeps_on    = 0;
	thread_table[i].pagetable    = NULL;
	thread_table[i].process_id   = -1;	
	thread_table[i].next         = -1;	

    #ifdef CHANGED_1
	// thread sleeping state init
	thread_table[i].next_sleeper_id = -1;
	thread_table[i].wakeup_time     = 0;
        #ifdef CHANGED_ADDITIONAL_1
        thread_table[i].priority = THREAD_PRIORITY_NORMAL;
        #endif
    #endif
    #ifdef CHANGED_2
    thread_table[i].userland_pid = -1;
    #endif
    }

    thread_table[IDLE_THREAD_TID].context->cpu_regs[MIPS_REGISTER_SP] =
	(uint32_t) thread_stack_areas + CONFIG_THREAD_STACKSIZE -4 -
	sizeof(context_t);
    thread_table[IDLE_THREAD_TID].context->pc = 
        (uint32_t) _idle_thread_wait_loop;
    thread_table[IDLE_THREAD_TID].context->status = 
        INTERRUPT_MASK_ALL | INTERRUPT_MASK_MASTER;
    thread_table[IDLE_THREAD_TID].state = THREAD_READY;
    thread_table[IDLE_THREAD_TID].context->prev_context =
	thread_table[IDLE_THREAD_TID].context;
}


/** Creates a new thread. A free slot is allocated from the thread
 * table for the new thread and its content is initialized to 'nil'
 * values. The new thread will call function 'func' with the argument
 * 'arg' when the thread is run by thread_run().
 *
 * @param func Function pointer to the threads 'main' function.
 * @param arg Argument to pass to 'func' (meaning defined by 'func').
 *
 * @return The thread ID of the created thread, or negative if
 * creation failed (thread table is full).
 */
#ifdef CHANGED_ADDITIONAL_1
TID_t thread_create_with_priority(void (*func)(uint32_t), uint32_t arg, priority_t p)
{
    static TID_t next_tid = 0;
    TID_t i, tid = -1;


    interrupt_status_t intr_status;
      
    intr_status = _interrupt_disable();

    spinlock_acquire(&thread_table_slock);
    
    /* Find the first free thread table entry starting from 'next_tid' */
    for (i=0; i<CONFIG_MAX_THREADS; i++) {
	TID_t t = (i + next_tid) % CONFIG_MAX_THREADS;

	if(t == IDLE_THREAD_TID)
	    continue;
	
	if (thread_table[t].state
	    == THREAD_FREE) {
	    tid = t;
	    break;
	}
    }

    /* Is the thread table full? */
    if (tid < 0) { 
	spinlock_release(&thread_table_slock);
	_interrupt_set_state(intr_status);
	return tid;
    }

    next_tid = (tid+1) % CONFIG_MAX_THREADS;

    thread_table[tid].state = THREAD_NONREADY;

    spinlock_release(&thread_table_slock);
    _interrupt_set_state(intr_status);

    thread_table[tid].context      = (context_t *) (thread_stack_areas
	+CONFIG_THREAD_STACKSIZE*tid + CONFIG_THREAD_STACKSIZE - 
	 sizeof(context_t));

    for (i=0; i< (int) sizeof(context_t)/4; i++) {
	*(((uint32_t *) thread_table[tid].context) + i) = 0;
    }

    thread_table[tid].user_context = NULL;
    thread_table[tid].pagetable    = NULL;
    thread_table[tid].sleeps_on    = 0;
    thread_table[tid].process_id   = -1;
    thread_table[tid].next         = -1;

    /* the change */
    KERNEL_ASSERT(p == THREAD_PRIORITY_HIGH || p == THREAD_PRIORITY_NORMAL);
    thread_table[tid].priority = p;

    /* Make sure that we always have a valid back reference on context chain */
    thread_table[tid].context->prev_context = thread_table[tid].context;

    /* set stack pointer to the end of stack */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_SP] = 
	(uint32_t)thread_stack_areas
	+ (CONFIG_THREAD_STACKSIZE * tid) 
	+ CONFIG_THREAD_STACKSIZE-4-
	sizeof(context_t); /* to the end of stack */

    /* set program counter to the specified function */
    thread_table[tid].context->pc = (uint32_t)func;

    /* set the return address to thread_finish */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_RA] = 
	(uint32_t)thread_finish;    

    /* set the argument register to the specified argument ... */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_A0] = arg;    
    /* ... and reserve space for the argument in the stack (GCC calling
       convention requires this even when the argument is not in the stack) */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_SP] = 
        thread_table[tid].context->cpu_regs[MIPS_REGISTER_SP] - 4;

    /* enable interrupts for this new thread */
    thread_table[tid].context->status = 
        INTERRUPT_MASK_ALL | INTERRUPT_MASK_MASTER;

    return tid;
}
#endif /* CHANGED_ADDITIONAL_1 */


TID_t thread_create(void (*func)(uint32_t), uint32_t arg)
{
    static TID_t next_tid = 0;
    TID_t i, tid = -1;


    interrupt_status_t intr_status;
      
    intr_status = _interrupt_disable();

    spinlock_acquire(&thread_table_slock);
    
    /* Find the first free thread table entry starting from 'next_tid' */
    for (i=0; i<CONFIG_MAX_THREADS; i++) {
	TID_t t = (i + next_tid) % CONFIG_MAX_THREADS;

	if(t == IDLE_THREAD_TID)
	    continue;
	
	if (thread_table[t].state
	    == THREAD_FREE) {
	    tid = t;
	    break;
	}
    }

    /* Is the thread table full? */
    if (tid < 0) { 
	spinlock_release(&thread_table_slock);
	_interrupt_set_state(intr_status);
	return tid;
    }

    next_tid = (tid+1) % CONFIG_MAX_THREADS;

    thread_table[tid].state = THREAD_NONREADY;

    spinlock_release(&thread_table_slock);
    _interrupt_set_state(intr_status);

    thread_table[tid].context      = (context_t *) (thread_stack_areas
	+CONFIG_THREAD_STACKSIZE*tid + CONFIG_THREAD_STACKSIZE - 
	 sizeof(context_t));

    for (i=0; i< (int) sizeof(context_t)/4; i++) {
	*(((uint32_t *) thread_table[tid].context) + i) = 0;
    }

    thread_table[tid].user_context = NULL;
    thread_table[tid].pagetable    = NULL;
    thread_table[tid].sleeps_on    = 0;
    thread_table[tid].process_id   = -1;
    thread_table[tid].next         = -1;

    #ifdef CHANGED_ADDITIONAL_1
    /* the change */
    thread_table[tid].priority = THREAD_PRIORITY_NORMAL;
    #endif /* CHANGED_ADDITIONAL_1 */

    /* Make sure that we always have a valid back reference on context chain */
    thread_table[tid].context->prev_context = thread_table[tid].context;

    /* set stack pointer to the end of stack */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_SP] = 
	(uint32_t)thread_stack_areas
	+ (CONFIG_THREAD_STACKSIZE * tid) 
	+ CONFIG_THREAD_STACKSIZE-4-
	sizeof(context_t); /* to the end of stack */

    /* set program counter to the specified function */
    thread_table[tid].context->pc = (uint32_t)func;

    /* set the return address to thread_finish */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_RA] = 
	(uint32_t)thread_finish;    

    /* set the argument register to the specified argument ... */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_A0] = arg;    
    /* ... and reserve space for the argument in the stack (GCC calling
       convention requires this even when the argument is not in the stack) */
    thread_table[tid].context->cpu_regs[MIPS_REGISTER_SP] = 
        thread_table[tid].context->cpu_regs[MIPS_REGISTER_SP] - 4;

    /* enable interrupts for this new thread */
    thread_table[tid].context->status = 
        INTERRUPT_MASK_ALL | INTERRUPT_MASK_MASTER;

    return tid;
}


/** Run a thread. The given thread is added to the scheduler's
 * ready-to-run list. This is really just a wrapper for
 * scheduler_add_ready().
 *
 * @param t The ID of the thread to be run.
 */
void thread_run(TID_t t)
{
    scheduler_add_ready(t);
}



/** Perform voluntary rescheduling. The current (=calling) thread will
 * voluntary end its time slice when this function is called. The
 * thread will remain in a ready state, and if there are no other
 * threads which are ready to run, the calling thread will resume its
 * execution immediately. This should NOT be used as a substitute for
 * sleeping.
 */
void thread_switch(void)
{
      interrupt_status_t intr_status;
      
      intr_status = _interrupt_enable();
      _interrupt_generate_sw0();
      _interrupt_set_state(intr_status);
}

/**
 * Return the TID of the calling thread. 
 * Finds out what is the TID of the thread calling this function.
 *
 * @return Thread ID of the calling thread.
 */

TID_t thread_get_current_thread(void)
{
    TID_t t;
    interrupt_status_t intr_status;
      
    intr_status = _interrupt_disable();

    t = scheduler_current_thread[_interrupt_getcpu()];

    _interrupt_set_state(intr_status);

    return t;
}

/**
 * Return the thread table entry of current thread.
 *
 * @return Thread entry in thread table for current thread. Note that
 * the returned value is the actual entry and all acccess
 * synchronizations must be handled by caller of this function.
 */

thread_table_t *thread_get_current_thread_entry(void)
{
    TID_t t;
    interrupt_status_t intr_status;
      
    intr_status = _interrupt_disable();

    t = scheduler_current_thread[_interrupt_getcpu()];

    _interrupt_set_state(intr_status);

    return &thread_table[t];
}

/**
 * Changes the calling thread to userland thread. This function
 * will never return.
 * 
 * @param usercontext Context in userland into which this thread will
 * enter.
 *
 */

void thread_goto_userland(context_t *usercontext)
{
    /* Set userland bit and enable interrupts before entering userland. */
    usercontext->status = usercontext->status | USERLAND_ENABLE_BIT;
    usercontext->status = usercontext->status | INTERRUPT_MASK_ALL;
    usercontext->status = usercontext->status | INTERRUPT_MASK_MASTER;
    _cswitch_to_userland(usercontext);
    
    KERNEL_PANIC("Userland entering returned for unknown reason.");
}

/** Perform suicide. The calling thread will kill itself by freeing
 * its memory and other resources and marking itself as dying. The
 * scheduler will free the thread table entry when it encounters dying
 * threads.
 */
void thread_finish(void)
{
    TID_t my_tid;

    my_tid = thread_get_current_thread();

    _interrupt_disable();

    /* Check that the page mappings have been cleared. */
    KERNEL_ASSERT(thread_table[my_tid].pagetable == NULL);

    spinlock_acquire(&thread_table_slock);
    thread_table[my_tid].state = THREAD_DYING;
    spinlock_release(&thread_table_slock);

    _interrupt_enable();
    _interrupt_generate_sw0();

    /* not possible without a stack? alternative in assembler? */
    KERNEL_PANIC("thread_finish(): thread was not destroyed");
}


#ifdef CHANGED_1
/**
 * Puts this thread to sleep the given time. Thread has no possibility to wakeup before
 * the given time has elapsed.
 *
 */
void thread_sleep(uint32_t sleep_time_in_milliseconds) {
    // get wakeup time and current tid
    uint32_t wakeup_time = rtc_get_msec() + sleep_time_in_milliseconds;
    TID_t my_tid = thread_get_current_thread();


    // disable interrupts and acquire thread table spinlock
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&thread_table_slock);
    // mark this thread as sleeper and set its wakeup time
    thread_table[my_tid].wakeup_time = wakeup_time;


    // correct our linked list linking (put this thead to list based on wakeup time)
    // the next wakeup thread is first of the list
    if (thread_next_sleeper_id == -1) {
        thread_next_sleeper_id = my_tid;
        thread_table[my_tid].next_sleeper_id = -1;
    } else {
        TID_t it = thread_next_sleeper_id, prev = -1;
        int is_tail = 0, is_top = 1;
        while (1) {
            if (wakeup_time < thread_table[it].wakeup_time) {
                break;
            } else if (thread_table[it].next_sleeper_id == -1) {
                is_tail = 1;
                break;
            }
            prev = it;
            it = thread_table[it].next_sleeper_id;
            is_top = 0;
        }
        // we have found our position, link it!
        if (is_tail) {
            thread_table[it].next_sleeper_id = my_tid;
            thread_table[my_tid].next_sleeper_id = -1;
        } else if (is_top) {
            thread_table[my_tid].next_sleeper_id = thread_next_sleeper_id;
            thread_next_sleeper_id = my_tid;
        } else {
            KERNEL_ASSERT(prev != -1);
            thread_table[my_tid].next_sleeper_id = it;
            thread_table[prev].next_sleeper_id = my_tid;
        }
    }

    // release table lock and put this thread to sleep
    spinlock_release(&thread_table_slock);
    thread_switch();
    // zzzZZZzzz
    // given amount has elapsed, restore interrupt status
    _interrupt_set_state(prev_int_stat);
}
#endif

/** @} */

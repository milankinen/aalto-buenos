/* 
 * created by Jaakko
 */

#include "lib/types.h"
#include "kernel/config.h"
#include "kernel/assert.h"
#include "kernel/spinlock.h"
#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "kernel/sleepq.h"
#include "kernel/panic.h"

#include "kernel/lock_cond.h"

/* definitions for the locks and condition variables */

#define NO_THREAD -1
#define RESERVED_THREAD -2

/* data structures for lock_t and cond_t */

struct lock_s {
    spinlock_t slock;
    TID_t locked_id;
    uint32_t nested_locking_count;
    uint32_t waiting_thread_count;
    int8_t is_used;
};

struct cond_s {
    spinlock_t slock;
    uint32_t waiting_thread_count;
    int8_t is_used;
};

/* spinlocks and tables they cover */

spinlock_t lock_table_slock;
lock_t lock_table[CONFIG_MAX_LOCKS];

spinlock_t cond_table_slock;
cond_t cond_table[CONFIG_MAX_CONDS];

/* init functions */

void lock_init(void) {
    int i;
    /* Start by acquiring the lock_table_slock.
     * Interrupts need to be disabled and the lock initialized.
     */
    interrupt_status_t prev_int_stat;
    prev_int_stat = _interrupt_disable();

    spinlock_reset(&lock_table_slock);
    spinlock_acquire(&lock_table_slock);

    /* initialize all locks unused */
    for (i = 0; i < CONFIG_MAX_LOCKS; i++) {
        spinlock_reset(&lock_table[i].slock);
        lock_table[i].locked_id = NO_THREAD;
        lock_table[i].nested_locking_count = 0;
        lock_table[i].waiting_thread_count = 0;
        lock_table[i].is_used = 0;
    }
    
    /* release spinlock and set interrupt to previous state */
    spinlock_release(&lock_table_slock);
    _interrupt_set_state(prev_int_stat);
}

void cond_init(void) {
    int i;
    /* Start by acquiring the cond_table_slock.
     * Interrupts need to be disabled and the lock initialized.
     */
    interrupt_status_t prev_int_stat;
    prev_int_stat = _interrupt_disable();

    spinlock_reset(&cond_table_slock);
    spinlock_acquire(&cond_table_slock);

    /* initialize all conds unused */
    for (i = 0; i < CONFIG_MAX_CONDS; i++) {
        spinlock_reset(&cond_table[i].slock);
        cond_table[i].waiting_thread_count = 0;
        cond_table[i].is_used = 0;
    }
    
    /* release spinlock and set interrupt to previous state */
    spinlock_release(&cond_table_slock);
    _interrupt_set_state(prev_int_stat);
}

/* lock functions */

lock_t *lock_create(void) {
    int i;
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&lock_table_slock);

    /* critical section: find next unused lock and set it used and
     * return the address
     */
    for (i = 0; i < CONFIG_MAX_LOCKS; i++) {
        if (!(lock_table[i].is_used)) {
            lock_table[i].is_used = 1;
            /* found unused, release lock and return interrupt status
             * back to original */
            spinlock_release(&lock_table_slock);
            _interrupt_set_state(prev_int_stat);
            return &lock_table[i];
        }
    }

    /* Here all locks were used which is bad: panic*/
    KERNEL_PANIC("lock_create(): no free locks");
    return 0; /* need to return something to prevent compiler error */
}

void lock_destroy(lock_t *lock) {
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&lock_table_slock);

    spinlock_acquire(&lock->slock);
    /* critical section: check that lock is open if not panic*/
    KERNEL_ASSERT(lock->locked_id == NO_THREAD);

    /* mark lock unused */
    lock->is_used = 0;

    spinlock_release(&lock->slock);
    spinlock_release(&lock_table_slock);
    _interrupt_set_state(prev_int_stat);
}

void lock_acquire(lock_t *lock) {
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&lock->slock);

    /* if lock was open */
    if (lock->locked_id == NO_THREAD) {
        lock->locked_id = thread_get_current_thread();
        lock->nested_locking_count = 1;
        /* lock is now acquired and spinlock can be released
         * and interrupts returned to original state
         */
        spinlock_release(&lock->slock);
        _interrupt_set_state(prev_int_stat);
        return;
    } else if (lock->locked_id == thread_get_current_thread()) {
        lock->nested_locking_count++;
        /* same thread tried to lock, so function returns and
         * releases the locks after increasing nesting
         */
        spinlock_release(&lock->slock);
        _interrupt_set_state(prev_int_stat);
        return;
    }
    
    /* Now the thread trying to lock must be suspended.
     * When using sleepq interrupts must be disabled and
     * they are at this point.
     */
    lock->waiting_thread_count++;
    sleepq_add(&lock->slock);
    /* before leaving execution of this thread release
     * lock and interrupts can be left as they are.
     */
    spinlock_release(&lock->slock);
    thread_switch();

    /* Here the thread has been woken up.
     * The spinlock must be acquired again 
     */
    spinlock_acquire(&lock->slock);
    lock->waiting_thread_count--;
    lock->locked_id = thread_get_current_thread();
    lock->nested_locking_count = 1;
    spinlock_release(&lock->slock);
}

void lock_release(lock_t *lock) {
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&lock->slock);

    /* releasing some other threads lock is not allowed */
    KERNEL_ASSERT(lock->locked_id == thread_get_current_thread());

    if (lock->nested_locking_count > 1) {
        /* nested locking, do nothing when releasing except
         * decrease nesting and release spinlock and return
         * interrupt status back to original
         */
        lock->nested_locking_count--;
        spinlock_release(&lock->slock);
        _interrupt_set_state(prev_int_stat);
        return;
    }
    /* Here we need to release the lock and wake up a thread if any */
    if (lock->waiting_thread_count > 0) {
        sleepq_wake(&lock->slock);
        /* set locked thread to RESERVED_THREAD instead of
         * NO_THREAD to prevent jumping the sleep queue
         */
        lock->locked_id = RESERVED_THREAD;
    } else { /* lock->waiting_thread_count == 0 */
        /* there are no threads in queue */
        lock->locked_id = NO_THREAD;
    }
    /* release and return again */
    spinlock_release(&lock->slock);
    _interrupt_set_state(prev_int_stat);
    return;
}

/* condition variable functions */

/*cond_t *condition_create(void) {
    return 0;
}

void condition_destroy(cond_t *cond) {
}

void condition_wait(cond_t *cond, lock_t *condition_lock) {
}

void condition_signal(cond_t *cond, lock_t *condition_lock) {
}

void condition_broadcast(cond_t *cond, lock_t *condition_lock) {
}*/

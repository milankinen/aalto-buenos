/* 
 * created by Jaakko
 */

#ifdef CHANGED_1

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




/* spinlocks and tables they cover */

spinlock_t lock_table_slock;
static lock_t lock_table[CONFIG_MAX_LOCKS];

spinlock_t cond_table_slock;
static cond_t cond_table[CONFIG_MAX_CONDITION_VARIABLES];

/**
 * These helper functions are meant for spinlock locking (so that interrupt disabling
 * won't forgot).
 * Proper compilers will optimize these function calls away for performance.
 */
/*
static interrupt_status_t s_acquire(spinlock_t* spin_lock) {
    interrupt_status_t prev_int_stat;
    prev_int_stat = _interrupt_disable();
    spinlock_acquire(spin_lock);
    return prev_int_stat;
}
*/
static void s_release(spinlock_t* spin_lock, interrupt_status_t prev_status) {
    spinlock_release(spin_lock);
    _interrupt_set_state(prev_status);
}

/* init functions */

void lock_table_init(void) {
    uint32_t i;
    /* Start by acquiring the lock_table_slock.
     * Interrupts need to be disabled and the lock initialized.
     */
    interrupt_status_t prev_int_stat;
    prev_int_stat = _interrupt_disable();

    spinlock_reset(&lock_table_slock);
    spinlock_acquire(&lock_table_slock);

    /* initialize all locks unused */
    for (i = 0; i < CONFIG_MAX_LOCKS; i++) {
        spinlock_reset(&(lock_table[i].slock));
        lock_table[i].locked_id = LOCK_NO_THREAD;
        lock_table[i].nested_locking_count = 0;
        lock_table[i].waiting_thread_count = 0;
        lock_table[i].is_used = 0;
    }
    
    /* release spinlock and set interrupt to previous state */
    s_release(&lock_table_slock, prev_int_stat);
}

void cond_table_init(void) {
    int i;
    /* Start by acquiring the cond_table_slock.
     * Interrupts need to be disabled and the lock initialized.
     */
    interrupt_status_t prev_int_stat;
    prev_int_stat = _interrupt_disable();

    spinlock_reset(&cond_table_slock);
    spinlock_acquire(&cond_table_slock);

    /* initialize all conds unused */
    for (i = 0; i < CONFIG_MAX_CONDITION_VARIABLES; i++) {
        spinlock_reset(&cond_table[i].slock);
        cond_table[i].waiting_thread_count = 0;
        cond_table[i].is_used = 0;
    }
    
    /* release spinlock and set interrupt to previous state */
    spinlock_release(&cond_table_slock);
    _interrupt_set_state(prev_int_stat);
}



/* ********************************************* */


/* lock functions */

lock_t *lock_create(void) {
    int i;
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&lock_table_slock);
    lock_t* lock_to_return;

    /* critical section: find next unused lock and set it used and
     * return the address
     */
    for (i = 0; i < CONFIG_MAX_LOCKS; i++) {
        if (!(lock_table[i].is_used)) {
            lock_table[i].is_used = 1;
            /* found unused, release lock and return interrupt status
             * back to original */
            lock_to_return = lock_table + i;
            spinlock_release(&lock_table_slock);
            _interrupt_set_state(prev_int_stat);
            return lock_to_return;
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
    KERNEL_ASSERT(lock->locked_id == LOCK_NO_THREAD);

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
    if (lock->locked_id == LOCK_NO_THREAD) {
        lock->locked_id = thread_get_current_thread();
        //kprintf("acquire %d", lock->locked_id);
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
    _interrupt_set_state(prev_int_stat);
}

void lock_release(lock_t *lock) {
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&lock->slock);

    //kprintf("release %d . %d", lock->locked_id, thread_get_current_thread());

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

    // set to zero if lock is about to be released
    lock->nested_locking_count = 0;

    /* Here we need to release the lock and wake up a thread if any */
    if (lock->waiting_thread_count > 0) {
        sleepq_wake(&lock->slock);
        /* set locked thread to LOCK_RESERVED_THREAD instead of
         * LOCK_NO_THREAD to prevent jumping the sleep queue
         */
        lock->locked_id = LOCK_RESERVED_THREAD;
    } else { /* lock->waiting_thread_count == 0 */
        /* there are no threads in queue */
        lock->locked_id = LOCK_NO_THREAD;
    }
    /* release and return again */
    spinlock_release(&lock->slock);
    _interrupt_set_state(prev_int_stat);
    return;
}




/* condition variable functions */

cond_t *condition_create(void) {
    int i;
    /* this changes the cond_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&cond_table_slock);
    cond_t* cond_to_return;

    /* critical section: find next unused cond and set it used and
     * return the address
     */
    for (i = 0; i < CONFIG_MAX_LOCKS; i++) {
        if (!(cond_table[i].is_used)) {
            cond_table[i].is_used = 1;
            /* found unused, release cond and return interrupt status
             * back to original */
            cond_to_return = cond_table + i;
            spinlock_release(&cond_table_slock);
            _interrupt_set_state(prev_int_stat);
            return cond_to_return;
        }
    }

    /* Here all conds were used which is bad: panic*/
    KERNEL_PANIC("cond_create(): no free condition variables");
    return 0;
}

void condition_destroy(cond_t *cond) {
    KERNEL_ASSERT(cond);
    /* this changes the lock_table and thus
     * requires aquiring a spinlock and disabling interrupts.
     */
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&cond_table_slock);

    spinlock_acquire(&cond->slock);

    /* test that no threads are waiting condition */
    KERNEL_ASSERT(cond->waiting_thread_count == 0);

    /* mark lock unused */
    cond->is_used = 0;

    spinlock_release(&cond->slock);
    spinlock_release(&cond_table_slock);
    _interrupt_set_state(prev_int_stat);
}


void condition_wait(cond_t *cond, lock_t *condition_lock) {
    KERNEL_ASSERT(cond && condition_lock);
    // lock cond variable
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&cond->slock);
    // add this thread to sleepq
    cond->waiting_thread_count++;
    sleepq_add(&cond->slock);
    // release lock for condition
    lock_release(condition_lock);
    // release spinlock and put thread to sleep
    spinlock_release(&cond->slock);
    thread_switch();
    // zzzZZzzz
    // now thread wakes up, it means that it is singaled

    // restore interrupts and try to acquire lock
    _interrupt_set_state(prev_int_stat);
    lock_acquire(condition_lock);
}

void condition_signal(cond_t *cond, lock_t *condition_lock) {
    KERNEL_ASSERT(cond && condition_lock);
    // lock cond variable
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&cond->slock);
    // check that we have threads waiting for signal
    if (cond->waiting_thread_count > 0) {
        // we have 1..n, wake the first one
        cond->waiting_thread_count--;
        sleepq_wake(&cond->slock);
    }
    // we dont need to release condition lock because this is non-blocking cond var
    // just restore interrupts
    spinlock_release(&cond->slock);
    _interrupt_set_state(prev_int_stat);
}

void condition_broadcast(cond_t *cond, lock_t *condition_lock) {
    KERNEL_ASSERT(cond && condition_lock);
    // lock cond variable
    interrupt_status_t prev_int_stat = _interrupt_disable();
    spinlock_acquire(&cond->slock);
    // check that we have threads waiting for signal
    if (cond->waiting_thread_count > 0) {
        // we have 1..n, wake all!
        cond->waiting_thread_count = 0;
        sleepq_wake_all(&cond->slock);
    }
    // we dont need to release condition lock because this is non-blocking cond var
    // just restore interrupts
    spinlock_release(&cond->slock);
    _interrupt_set_state(prev_int_stat);
}


#endif

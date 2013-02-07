/*
 * lock_tests.c
 *
 *  Created on: 7 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_1

#include "lib/types.h"
#include "kernel/config.h"
#include "kernel/assert.h"
#include "kernel/semaphore.h"
#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "kernel/lock_cond.h"

#include "kernel_tests/change_1_tests.h"



/**
 * Helper to wait until we have 'num' threads waiting for lock (after first locked).
 */
static void wait_until_threads(lock_t* lock, uint32_t num) {
    int ok_to_break = 0;
    while(!ok_to_break) {
        interrupt_status_t prev_status = _interrupt_disable();
        spinlock_acquire(&lock->slock);
        if (lock->waiting_thread_count >= num) {
            ok_to_break = 1;
        }
        spinlock_release(&lock->slock);
        _interrupt_set_state(prev_status);
    }
}


static void test_acquire_release() {
    kprintf("Testing basic lock functions... ");
    lock_t* lock;
    lock = lock_create();
    KERNEL_ASSERT(lock != NULL);
    lock_acquire(lock);
    lock_release(lock);
    lock_destroy(lock);
    kprintf("OK!\n");
}



static lock_t* fairness_lock;
static uint32_t fairness_test_value;

static void fairness_thread(uint32_t nth) {
    wait_until_threads(fairness_lock, nth - 1);
    lock_acquire(fairness_lock);
    // tested critical section start
    fairness_test_value++;
    KERNEL_ASSERT(fairness_test_value == nth);
    // critical section ends
    lock_release(fairness_lock);
}

static void test_fairness() {
    kprintf("Testing lock fairness... ");
    const uint32_t thread_num = 10;
    fairness_test_value = 0;
    fairness_lock = lock_create();

    lock_acquire(fairness_lock);
    uint32_t i;
    for (i = 0 ; i < thread_num ; i++) {
        thread_run(thread_create(fairness_thread, i + 1));
    }

    // notify created threads that we can start testing
    wait_until_threads(fairness_lock, thread_num);
    lock_release(fairness_lock);
    // when lock releases then pending threads add their value
    // if the lock is fair then this acquire will be the last of
    // lock thread queue and this line of code acquires lock last
    lock_acquire(fairness_lock);
    KERNEL_ASSERT(fairness_test_value == thread_num);
    lock_release(fairness_lock);
    lock_destroy(fairness_lock);
    kprintf("OK!\n");
}



static void test_reentrant() {
    kprintf("Testing re-entrant locking... ");
    lock_t* lock;
    lock = lock_create();
    //kprintf("nested: %d", lock->nested_locking_count);
    KERNEL_ASSERT(lock->nested_locking_count == 0);
    KERNEL_ASSERT(lock->locked_id != thread_get_current_thread());

    // first locking
    lock_acquire(lock);
    KERNEL_ASSERT(lock->locked_id == thread_get_current_thread());
    KERNEL_ASSERT(lock->nested_locking_count == 1);

    // first nested locking
    lock_acquire(lock);
    KERNEL_ASSERT(lock->locked_id == thread_get_current_thread());
    KERNEL_ASSERT(lock->nested_locking_count == 2);

    // second nested locking
    lock_acquire(lock);
    KERNEL_ASSERT(lock->locked_id == thread_get_current_thread());
    KERNEL_ASSERT(lock->nested_locking_count == 3);

    // releases
    lock_release(lock);
    KERNEL_ASSERT(lock->locked_id == thread_get_current_thread());
    KERNEL_ASSERT(lock->nested_locking_count == 2);
    // releases
    lock_release(lock);
    KERNEL_ASSERT(lock->locked_id == thread_get_current_thread());
    KERNEL_ASSERT(lock->nested_locking_count == 1);

    // releases final time -> lock should be totally released
    lock_release(lock);
    KERNEL_ASSERT(lock->locked_id != thread_get_current_thread());
    KERNEL_ASSERT(lock->nested_locking_count == 0);

    // test ok
    lock_destroy(lock);
    kprintf("OK!\n");
}




void run_lock_tests() {
    test_acquire_release();
    test_fairness();
    test_reentrant();
}



#endif

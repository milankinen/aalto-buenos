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
    KERNEL_ASSERT(lock->is_used);
    lock_acquire(lock);
    lock_release(lock);
    lock_destroy(lock);
    KERNEL_ASSERT(!lock->is_used);
    kprintf("OK!\n");
}

static void test_destroy_resource_free() {
    kprintf("Testing that destroyed locks free their resources for later usage... ");
    uint32_t i;
    lock_t* lock;

    for (i = 0 ; i < CONFIG_MAX_LOCKS * 2 ; i++) {
        lock = lock_create();
        KERNEL_ASSERT(lock->is_used);
        lock_destroy(lock);
        KERNEL_ASSERT(!lock->is_used);
    }
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

static void test_critical_and_fairness() {
    kprintf("Testing lock critical section and fairness... ");
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



static lock_t* interrupt_lock;

static void test_int_create_destroy() {
    lock_t* lock;
    // enabled
    interrupt_status_t start_state = _interrupt_enable();
    interrupt_status_t test_state = _interrupt_get_state();
    lock = lock_create();
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    lock_destroy(lock);
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    // disabled
    _interrupt_disable();
    test_state = _interrupt_get_state();
    lock = lock_create();
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    lock_destroy(lock);
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    // return to original state
    _interrupt_set_state(start_state);
}


static void test_int_enabled(uint32_t re_entrant_recursion) {
    if (!re_entrant_recursion) {
        return;
    }
    interrupt_status_t start_state = _interrupt_enable();
    interrupt_status_t test_state = _interrupt_get_state();
    lock_acquire(interrupt_lock);
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    // re-entrant locking testing also
    test_int_enabled(re_entrant_recursion - 1);
    lock_release(interrupt_lock);
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    // return original state
    _interrupt_set_state(start_state);
}

static void test_int_disabled(uint32_t re_entrant_recursion) {
    if (!re_entrant_recursion) {
        return;
    }
    interrupt_status_t start_state = _interrupt_disable();
    interrupt_status_t test_state = _interrupt_get_state();
    lock_acquire(interrupt_lock);
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    // re-entrant locking testing also
    test_int_disabled(re_entrant_recursion - 1);
    lock_release(interrupt_lock);
    KERNEL_ASSERT(test_state == _interrupt_get_state());
    // return original state
    _interrupt_set_state(start_state);
    // prevent compiler errros
}


static void test_int_enabled_sleep(uint32_t wait_num) {
    wait_until_threads(interrupt_lock, wait_num);
    test_int_enabled(1);
}

static void test_int_disabled_sleep(uint32_t wait_num) {
    wait_until_threads(interrupt_lock, wait_num);
    test_int_enabled(1);
}

static void test_int_enabled_multithread(const uint32_t thread_num) {
    uint32_t i;
    lock_acquire(interrupt_lock);
    for (i = 0 ; i < thread_num ; i++) {
        thread_run(thread_create(test_int_enabled_sleep, 0));
    }
    // wait until our threads are
    wait_until_threads(interrupt_lock, thread_num);
    lock_release(interrupt_lock);
    lock_acquire(interrupt_lock);
    KERNEL_ASSERT(interrupt_lock->waiting_thread_count == 0);
    lock_release(interrupt_lock);
}

static void test_int_disabled_multithread(const uint32_t thread_num) {
    uint32_t i;
    lock_acquire(interrupt_lock);
    for (i = 0 ; i < thread_num ; i++) {
        thread_run(thread_create(test_int_disabled_sleep, 0));
    }
    // wait until our threads are
    wait_until_threads(interrupt_lock, thread_num);
    lock_release(interrupt_lock);
    lock_acquire(interrupt_lock);
    KERNEL_ASSERT(interrupt_lock->waiting_thread_count == 0);
    lock_release(interrupt_lock);
}

static void test_interrupt_preserving() {
    kprintf("Testing lock interrupt state preserving... ");
    test_int_create_destroy();
    interrupt_lock = lock_create();

    // single thread testing
    test_int_enabled(2);
    test_int_disabled(2);
    // multi-thread testing
    test_int_enabled_multithread(3);
    test_int_disabled_multithread(3);

    lock_destroy(interrupt_lock);
    kprintf("OK!\n");
}




void run_lock_tests() {
    test_acquire_release();
    test_destroy_resource_free();
    test_reentrant();
    test_critical_and_fairness();
    test_interrupt_preserving();
}



#endif

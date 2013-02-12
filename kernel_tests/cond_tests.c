
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
static void wait_until_threads_lock(lock_t* lock, uint32_t num) {
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

static void wait_until_threads_cond(cond_t* cond, uint32_t num) {
    int ok_to_break = 0;
    while(!ok_to_break) {
        interrupt_status_t prev_status = _interrupt_disable();
        spinlock_acquire(&cond->slock);
        if (cond->waiting_thread_count >= num) {
            ok_to_break = 1;
        }
        spinlock_release(&cond->slock);
        _interrupt_set_state(prev_status);
    }
}


static void wait_until_lock_acquired(lock_t* lock) {
    int ok_to_break = 0;
        while(!ok_to_break) {
            interrupt_status_t prev_status = _interrupt_disable();
            spinlock_acquire(&lock->slock);
            if (lock->locked_id != LOCK_NO_THREAD) {
                ok_to_break = 1;
            }
            spinlock_release(&lock->slock);
            _interrupt_set_state(prev_status);
        }
}



static void test_basic_operations() {
    kprintf("Testing basic condition functions (no wait)... ");
    lock_t* lock = lock_create();
    cond_t* cond = condition_create();
    KERNEL_ASSERT(cond);
    condition_signal(cond, lock);
    condition_broadcast(cond, lock);
    condition_destroy(cond);
    lock_destroy(lock);
    kprintf("OK!\n");
}



static lock_t* signal_lock;
static cond_t* signal_cond;
static uint32_t signal_test_value;

static void signal_wait_thread(uint32_t val) {
    lock_acquire(signal_lock);
    KERNEL_ASSERT(signal_test_value == val);
    condition_wait(signal_cond, signal_lock);
    // wait until our main thread has reached lock_acquire
    wait_until_threads_lock(signal_lock, 1);
    KERNEL_ASSERT(signal_test_value == val + 1);
    signal_test_value++;
    lock_release(signal_lock);
}

static void test_wait_signal() {
    kprintf("Testing condition waiting and signaling... ");
    signal_test_value = 0;
    signal_lock = lock_create();
    signal_cond = condition_create();
    uint32_t orig_value = signal_test_value;
    thread_run(thread_create(signal_wait_thread, orig_value));
    // wait until our wait thread has reached condition
    wait_until_threads_cond(signal_cond, 1);
    // now increment test value by one
    signal_test_value++;

    // signal other wait thread to move on
    lock_acquire(signal_lock);
    condition_signal(signal_cond, signal_lock);
    lock_release(signal_lock);

    // we must ensure that our woken thread has acquired the lock, otherwise
    // this test fails (there is no guarantee that the woken thread reach the
    // acquiring code before this one if we don't ensure it)
    wait_until_lock_acquired(signal_lock);

    // wait until other thread has finished
    lock_acquire(signal_lock);
    KERNEL_ASSERT(signal_test_value == orig_value + 2);
    lock_release(signal_lock);

    // cleanup
    condition_destroy(signal_cond);
    lock_destroy(signal_lock);
    kprintf("OK!\n");
}


void run_cond_tests() {

    test_basic_operations();
    test_wait_signal();

}


#endif // CHANGED_1

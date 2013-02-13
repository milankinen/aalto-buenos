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
  while (!ok_to_break) {
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
  while (!ok_to_break) {
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
  while (!ok_to_break) {
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

static int threads_finished = 0;
static int test_variable;

static void wait_for_broadcast_t(uint32_t value) {

  value = value;
  lock_acquire(signal_lock);
  KERNEL_ASSERT(test_variable == 0);
  condition_wait(signal_cond, signal_lock);
  KERNEL_ASSERT(test_variable == 1);
  lock_release(signal_lock);
  threads_finished++;

}

static void test_broadcast() {
  kprintf("Testing condition broadcast... ");
  signal_lock = lock_create();
  signal_cond = condition_create();

  thread_run(thread_create(wait_for_broadcast_t, 0));
  thread_run(thread_create(wait_for_broadcast_t, 0));
  thread_run(thread_create(wait_for_broadcast_t, 0));
  thread_run(thread_create(wait_for_broadcast_t, 0));
  thread_run(thread_create(wait_for_broadcast_t, 0));
  wait_until_threads_cond(signal_cond, 5);

  lock_acquire(signal_lock);
  condition_broadcast(signal_cond, signal_lock);
  test_variable = 1;
  lock_release((signal_lock));
  while (threads_finished < 5) {
    thread_switch();
  }
  lock_destroy(signal_lock);
  condition_destroy(signal_cond);
  kprintf("OK!\n");
}

static int thread_number_released = 9999;
static int threads_running = 0;

static void wait_for_release_order_t(uint32_t value) {
  lock_acquire(signal_lock);
  threads_running++;
  condition_wait(signal_cond, signal_lock);
  thread_number_released = value;
  threads_running--;
  lock_release(signal_lock);

}

static void test_signal_release_order() {

  kprintf(
      "Testing signaling condition releases threads in the right order... ");
  signal_lock = lock_create();
  signal_cond = condition_create();

  thread_run(thread_create(wait_for_release_order_t, 0));
  wait_until_threads_cond(signal_cond, 1);
  thread_run(thread_create(wait_for_release_order_t, 1));
  wait_until_threads_cond(signal_cond, 2);
  thread_run(thread_create(wait_for_release_order_t, 2));
  wait_until_threads_cond(signal_cond, 3);
  thread_run(thread_create(wait_for_release_order_t, 3));
  wait_until_threads_cond(signal_cond, 4);
  thread_run(thread_create(wait_for_release_order_t, 4));
  wait_until_threads_cond(signal_cond, 5);

  lock_acquire(signal_lock);
  condition_signal(signal_cond, signal_lock);
  lock_release(signal_lock);
  while (threads_running > 4) {
    thread_switch();
  }
  KERNEL_ASSERT(thread_number_released == 0);

  lock_acquire(signal_lock);
  condition_signal(signal_cond, signal_lock);
  lock_release(signal_lock);
  while (threads_running > 3) {
    thread_switch();
  }
  KERNEL_ASSERT(thread_number_released == 1);

  lock_acquire(signal_lock);
  condition_signal(signal_cond, signal_lock);
  lock_release(signal_lock);
  while (threads_running > 2) {
    thread_switch();
  }
  KERNEL_ASSERT(thread_number_released == 2);

  lock_acquire(signal_lock);
  condition_signal(signal_cond, signal_lock);
  lock_release(signal_lock);
  while (threads_running > 1) {
    thread_switch();
  }
  KERNEL_ASSERT(thread_number_released == 3);

  lock_acquire(signal_lock);
  condition_signal(signal_cond, signal_lock);
  lock_release(signal_lock);
  while (threads_running > 0) {
    thread_switch();
  }
  KERNEL_ASSERT(thread_number_released == 4);

  lock_destroy(signal_lock);
  condition_destroy(signal_cond);
  kprintf("OK!\n");

}

void run_cond_tests() {

  test_basic_operations();
  test_wait_signal();
  test_broadcast();
  test_signal_release_order();
}

#endif // CHANGED_1

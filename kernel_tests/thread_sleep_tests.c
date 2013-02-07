
#ifdef CHANGED_1

#include "lib/libc.h"
#include "drivers/metadev.h"
#include "kernel/thread.h"
#include "kernel/scheduler.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/config.h"
#include "kernel/lock_cond.h"


static void test_sleep_500_ms() {
    kprintf("Testing thread sleep... ");
    uint32_t s, m, f;
    s = rtc_get_msec();
    KERNEL_ASSERT(s > 0);
    m = rtc_get_msec();
    KERNEL_ASSERT(m >= s);
    thread_sleep(500);
    f = rtc_get_msec();
    KERNEL_ASSERT(f - m >= 500);
    kprintf("without wait: %d ms, with wait %d ms... OK\n", m - s, f - m);
}



lock_t* print_lock;
lock_t* finish_lock;
cond_t* finish_cond;
uint32_t threads_finished;

#define TEST_PRINT(str, ...) lock_acquire(print_lock); \
 kprintf(str, __VA_ARGS__); lock_release(print_lock);

static void wait_thread(uint32_t id) {
    uint32_t s, w, f;
    w = _get_rand(500);
    s = rtc_get_msec();

    TEST_PRINT("Thread %d: waiting %d ms\n", id, w);
    thread_sleep(w);
    f = rtc_get_msec();
    TEST_PRINT("Thread %d: continue, took %d ms\n", id, f - s);

    lock_acquire(finish_lock);
    threads_finished++;
    condition_signal(finish_cond, finish_lock);
    lock_release(finish_lock);
}


static void test_sleep_multi_thread(const uint32_t thread_num) {
    kprintf("Testing thread sleep with multiple simultaneous threads... \n");
    _set_rand_seed(0xDEADBEEF);
    threads_finished = 0;
    print_lock = lock_create();
    finish_lock = lock_create();
    finish_cond = condition_create();

    // start test threads
    uint32_t start = rtc_get_msec();
    uint32_t i;
    for (i = 0 ; i < thread_num ; i++) {
        thread_run(thread_create(wait_thread, i));
    }

    // wait until all trhreads are done
    int ok_to_leave = 0;
    while (!ok_to_leave) {
        lock_acquire(finish_lock);
        condition_wait(finish_cond, finish_lock);
        if (threads_finished == thread_num) {
            ok_to_leave = 1;
        }
        lock_release(finish_lock);
    }

    // cleanup
    lock_destroy(print_lock);
    lock_destroy(finish_lock);
    condition_destroy(finish_cond);
    kprintf("OK! Took %d ms total\n", rtc_get_msec() - start);
}


void run_thread_sleep_tests() {
    test_sleep_500_ms();
    test_sleep_multi_thread(5);
}

#undef TEST_PRINT

#endif

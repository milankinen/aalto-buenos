
#ifdef CHANGED_ADDITIONAL_1

#include "lib/libc.h"
#include "drivers/metadev.h"
#include "kernel/thread.h"
#include "kernel/scheduler.h"
#include "kernel/panic.h"
#include "kernel/assert.h"
#include "kernel/config.h"
#include "kernel/lock_cond.h"
#include "kernel/interrupt.h"



#define PRIORITY_TEST_LOOP_COUNT 1500
#define PRIORITY_TEST_THREAD_COUNT (CONFIG_MAX_THREADS-4)

static spinlock_t test_slock;
static uint32_t test_num_ready;
static uint32_t test_num_finished;
static lock_t* print_lock;



#define TEST_PRINT(str, ...) lock_acquire(print_lock); \
 kprintf((str), __VA_ARGS__); lock_release(print_lock);

static void test_thread(uint32_t is_high) {
    if (is_high) {
        TEST_PRINT("-> HIGH (%d)\n", thread_get_current_thread());
    } else {
        TEST_PRINT("-> NORMAL (%d)\n", thread_get_current_thread());
    }
    // wait_until_threads_ready();
    uint32_t i;
    for (i = 0 ; i < PRIORITY_TEST_LOOP_COUNT ; i++) {
        // we must do something so that we can test that our high priority
        // threads are gained more time than low priority ones
        //thread_sleep(1);
        thread_switch();
    }

    if (is_high) {
        TEST_PRINT("HIGH -> (%d)\n", thread_get_current_thread());
    } else {
        TEST_PRINT("NORMAL -> (%d)\n", thread_get_current_thread());
    }
    test_num_finished++;
}

#undef TEST_PRINT


void run_thread_priority_tests() {
    kprintf("Testing thread priorities...\n");
    uint32_t i;
    spinlock_reset(&test_slock);
    print_lock = lock_create();
    test_num_ready = 0;
    test_num_finished = 0;

    for (i = 0 ; i < PRIORITY_TEST_THREAD_COUNT ; i++) {
        uint32_t is_high = i % 2;
        thread_run(thread_create_with_priority(test_thread, is_high, is_high ? THREAD_PRIORITY_HIGH
                : THREAD_PRIORITY_NORMAL));
    }

    // wait until our threads are finished
    while(test_num_finished < PRIORITY_TEST_THREAD_COUNT) {
        thread_switch();
    }
    kprintf("...test ended.\n");
    lock_destroy(print_lock);
}



#endif

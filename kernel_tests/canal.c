/*
 * canal.c
 *
 *  Created on: 11 Feb 2013
 */

#include "lib/types.h"
#include "kernel/config.h"
#include "kernel/assert.h"
#include "kernel/semaphore.h"
#include "kernel/thread.h"
#include "kernel/interrupt.h"
#include "kernel/lock_cond.h"
#include "kernel_tests/canal.h"

#ifdef CHANGED_1

#define FIXED_BOATS_BASSED 4
#define TRAVEL_TIME 10000

#define TESTFLOW1 7
#define TESTFLOW2 4
#define TESTFLOW3 11

#define STARVATION1 10
#define STARVATION2 1
#define STARVATION3 8

lock_t *lock;
int traffic_west = 0;
int boats_in_canal = 0;
int boats_in_queue = 0;

int canal_test_finished = 0;
//boats passed in the current direction
//will be reset as direction changes
int boats_passed = 0;
//total number of boats passed in the canal
int total_boats_passed = 0;
cond_t *go_west;
cond_t *go_east;

static void boat_t(uint32_t is_west) {
  canal_enter(is_west);
}

void createBoats(int num, int is_w) {
  for (; num > 0; num--) {
    thread_sleep(2000);
    TID_t tid = thread_create(&boat_t, is_w);
    thread_run(tid);
  }
}

void start_canal() {
  lock = lock_create();
  go_west = condition_create();
  go_east = condition_create();
  //create some arbitrary pattern of boats
  createBoats(TESTFLOW1, 0);
  createBoats(TESTFLOW2, 1);
  createBoats(TESTFLOW3, 0);
  while (total_boats_passed < TESTFLOW1 + TESTFLOW2 + TESTFLOW3) {
  //  kprintf("TBP: %d reach %d\n",total_boats_passed, TESTFLOW1 + TESTFLOW2 + TESTFLOW3);
    thread_switch();
  }
  //check that starvation does not occur
  //the one boat going to west must not be the last one to enter the canal
  total_boats_passed= 0;
  kprintf("\nStarvation test\n");
  createBoats(STARVATION1, 0);
  createBoats(STARVATION2, 1);
  createBoats(STARVATION3, 0);

  while (total_boats_passed < STARVATION1 + STARVATION2 + STARVATION3) {
    thread_switch();
  }
  KERNEL_ASSERT(traffic_west);

  canal_test_finished = 1;
}

static void print_info() {
  kprintf("boats in canal: %d, ", boats_in_canal);
  if (traffic_west) {
    kprintf("Traffic: west, ");
  } else {
    kprintf("Traffic: east, ");
  }
  kprintf("TID: %d\n", thread_get_current_thread());

}

void canal_enter(int is_west) {
  lock_acquire(lock);

  if (boats_in_canal == 0) {
    traffic_west = is_west;
  }

  if (boats_passed > FIXED_BOATS_BASSED && boats_in_queue > 0) {
    boats_in_queue++;
    if (is_west)
      condition_wait(go_west, lock);
    else
      condition_wait(go_east, lock);
    boats_in_queue--;
  }

  if (traffic_west != is_west) {
    boats_in_queue++;
    if (is_west) {
      //as we use Lampard-Redell monitors the condition
      //must be rechecked before proceeding
      while (!traffic_west) {
        condition_wait(go_west, lock);
      }
    } else {
      while (traffic_west) {
        condition_wait(go_east, lock);
      }
    }
    boats_in_queue--;
  }
  boats_in_canal++;
  kprintf("Entering canal. ");
  print_info();
  lock_release(lock);
  thread_sleep(TRAVEL_TIME);
  canal_exit();
}

void canal_exit() {
  lock_acquire(lock);

  //if we are the last boat to leave the canal change the direction
  //and signal the boats waiting on the other side
  if (boats_in_canal == 1) {
    if (traffic_west) {
      condition_broadcast(go_east, lock);
    } else {
      condition_broadcast(go_west, lock);
    }

    //change the direction
    if (traffic_west == 0) {
      traffic_west = 1;
    } else {
      traffic_west = 0;
    }
    boats_passed = 0;
  }


  boats_in_canal--;
  kprintf("Exiting canal. ");
  print_info();
  total_boats_passed++;
  boats_passed++;
  lock_release(lock);

}

int is_canal_test_finished() {
  return canal_test_finished;
}

#endif

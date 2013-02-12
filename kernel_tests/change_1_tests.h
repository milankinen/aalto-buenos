/*
 * change_1_tests.h
 *
 *  Created on: 7 Feb 2013
 *      Author: matti
 */

#ifndef CHANGE_1_TESTS_H_
#define CHANGE_1_TESTS_H_

#ifdef CHANGED_1


void run_lock_tests();
void run_cond_tests();
void run_thread_sleep_tests();

#ifdef CHANGED_ADDITIONAL_1
void run_thread_priority_tests();
#endif

#endif

#endif /* CHANGE_1_TESTS_H_ */

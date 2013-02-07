/*
 * make_water.c
 */


#include "kernel_tests/make_water.h"
#include "kernel/semaphore.h"
#include "kernel/thread.h"
#include "kernel/halt.h"

semaphore_t *hydrogensReady;
semaphore_t *hydrogensWaiting;
semaphore_t *omutex; //only one oxygen may created water at a given time
int testFinished;
int waterCreated;

void start_make_water(){
	semaphore_init();
	testFinished = 0;
	waterCreated = 0;
	hydrogensReady = semaphore_create(0);
	hydrogensWaiting = semaphore_create(0);
	omutex = semaphore_create(1);
	spawn_atoms();
}

void hydrogen(uint32_t dummy){
	kprintf("CREATED HYDROGEN\n");
	dummy = 0;
	semaphore_V(hydrogensReady);
	semaphore_P(hydrogensWaiting);
}

void oxygen(uint32_t dummy){
	kprintf("CREATED OXYGEN\n");
	dummy = 0;
	semaphore_P(omutex);
	semaphore_P(hydrogensReady);
	semaphore_P(hydrogensReady);
	semaphore_V(omutex);
	make_water();
	semaphore_V(hydrogensWaiting);
	semaphore_V(hydrogensWaiting);

}

void hydrogen_t(){
	TID_t tid = thread_create(&hydrogen, 0);
	thread_run(tid);
}

void oxygen_t(){
	TID_t tid = thread_create(&oxygen, 0);
	thread_run(tid);
}

void make_water(){
	kprintf("WATER CREATED\n");
	waterCreated++;
}

void spawn_atoms(){

	//check that the test does not go to a infite loop if it fails
	int fallback = 0;

	oxygen_t();
	oxygen_t();
	hydrogen_t();
	hydrogen_t();
	while(waterCreated!=1){
		kprintf("Waiting water to be created\n");
		check_fallback(++fallback);
		thread_switch();
	}
	hydrogen_t();
	hydrogen_t();
	while(waterCreated!=2){
		kprintf("Waiting water to be created\n");
		check_fallback(++fallback);
		thread_switch();
	}
	kprintf("Make Water test execution finished succesfully\n");
	testFinished = 1;
}
int is_water_test_finished(void){
	return testFinished;
}

void check_fallback(int i){
	if(i > 100){
		kprintf("makeWater test execution finished with ERRORS, terminating kernel\n");
		halt_kernel();
	}
}

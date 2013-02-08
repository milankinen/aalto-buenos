/*
 * makeWater.c
 *
 *  Created on: 5 Feb 2013
 *      Author: matti
 */


#include "kernel_tests/makeWater.h"
#include "kernel/semaphore.h"
#include "kernel/thread.h"
#include "kernel/halt.h"

semaphore_t *hydrogensReady;
semaphore_t *hydrogensWaiting;
semaphore_t *omutex; //only one oxygen may created water at a given time
int testFinished;
int waterCreated;

void startMakeWater(){
	semaphore_init();
	testFinished = 0;
	waterCreated = 0;
	hydrogensReady = semaphore_create(0);
	hydrogensWaiting = semaphore_create(0);
	omutex = semaphore_create(1);
	spawnAtoms();
}

void hydrogen(uint32_t dummy){
	kprintf("CREATED HYDROGEN\n");
	dummy = dummy;
	semaphore_V(hydrogensReady);
	semaphore_P(hydrogensWaiting);
}

void oxygen(uint32_t dummy){
	kprintf("CREATED OXYGEN\n");
	dummy = dummy;
	semaphore_P(omutex);
	semaphore_P(hydrogensReady);
	semaphore_P(hydrogensReady);
	semaphore_V(omutex);
	makeWater();
	semaphore_V(hydrogensWaiting);
	semaphore_V(hydrogensWaiting);

}

void hydrogenThread(){
	TID_t tid = thread_create(&hydrogen, 0);
	thread_run(tid);
}

void oxygenThread(){
	TID_t tid = thread_create(&oxygen, 0);
	thread_run(tid);
}

void makeWater(){
	kprintf("WATER CREATED\n");
	waterCreated++;
}

void spawnAtoms(){

	//check that the test does not go to a infite loop if it fails
	int fallback = 0;

	oxygenThread();
	oxygenThread();
	hydrogenThread();
	hydrogenThread();
	while(waterCreated!=1){
		kprintf("Waiting water to be created\n");
		checkFallBack(++fallback);
		thread_switch();
	}
	hydrogenThread();
	hydrogenThread();
	while(waterCreated!=2){
		kprintf("Waiting water to be created\n");
		checkFallBack(++fallback);
		thread_switch();
	}
	kprintf("Make Water test execution finished succesfully\n");
	testFinished = 1;
}
int isWaterTestFinished(void){
	return testFinished;
}

void checkFallBack(int i){
	if(i > 100){
		kprintf("makeWater test execution finished with ERRORS, terminating kernel\n");
		halt_kernel();
	}
}

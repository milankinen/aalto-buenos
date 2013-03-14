/*
 * process_table.h
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifndef PROCESS_TABLE_H_
#define PROCESS_TABLE_H_

#ifdef CHANGED_2

#include "proc/process.h"
#include "kernel/thread.h"
#include "kernel/lock_cond.h"

/**
 * Maximum lenght for process name.
 */
#define PROCESS_NAME_MAX_LENGTH 8
/**
 * Maximum number of open files a process can attain
 */
#define MAX_OPEN_FILES_PER_PROCESS 10

#define FILEHANDLE_UNUSED -1

typedef struct {
    /* Owner thread id. -1 if process is not running (associated to any thread) */
    TID_t tid;

    /* Process name, C-style string */
    char name[PROCESS_NAME_MAX_LENGTH];

    /* Link to next not-running process (this is used when allocating new processes. If child
     * process, then this links to next sibling process. */
    PID_t next;

    /* Condition variable which is waited when other process stops to join another process. */
    cond_t* die_cond;

    /* lock for ensuring that dying process handles its child process entries properly and vice versa */
    lock_t* die_lock;

    /* When process finishes, its return value is stored into this variable. */
    int retval;

    /*
     * If the process is other process's child, then this != -1. When process dies, if this value
     * is == -1 then mark the process to ready-to-start list, otherwise dying parent process is
     * responsible for adding the dead child process ready-to-run list again.
     */
    PID_t parent_pid;

    /* If process has child processes then this points to last child (linked list) */
    PID_t last_child_pid;

    /*references to open file handles */
    int filehandle[MAX_OPEN_FILES_PER_PROCESS];

} process_table_t;



void process_table_init();

PID_t get_current_process_pid();
process_table_t* get_current_process_entry();

#endif


#endif /* PROCESS_TABLE_H_ */

/*
 * syscall_handler_fs.c
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_2

#include "proc/syscall.h"


PID_t syscall_handle_exec(const char *filename) {
    // TODO: implementation
    return *((const PID_t*)filename);
}

void syscall_handle_exit(int retval) {
    // TODO: implementation
    retval = retval;
}

int syscall_handle_join(PID_t pid) {
    // TODO: implementation
    return (int)pid;
}


#endif /* CHANGED_2 */

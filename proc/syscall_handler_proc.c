/*
 * syscall_handler_fs.c
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_2

#include "kernel/config.h"
#include "proc/syscall.h"
#include "lib/libc.h"
#include "kernel/interrupt.h"
#include "vm/vm.h"
#include "vm/pagepool.h"
#include "kernel/thread.h"
#include "proc/process_table.h"


extern lock_t* process_table_lock;
extern process_table_t process_table[CONFIG_MAX_PROCESSES];


static void new_process_thread(uint32_t dataptr) {
    child_process_create_data_t* dat;
    dat = (child_process_create_data_t*)dataptr;

}


PID_t syscall_handle_execp(const char *filename, int argc, char** argv) {

    child_process_create_data_t dat;
    TID_t child_thread;
    process_table_t* my_entry;

    memoryset(&dat, 0, sizeof(child_process_create_data_t));
    dat.argc = argc;
    dat.argv = argv;
    dat.filename = filename;
    dat.parent_pid = get_current_process_pid();
    dat.child_pid = PROCESS_NO_PARENT_PID;
    dat.ready = 0;

    child_thread = thread_create(new_process_thread, (uint32_t)&dat);
    if (dat.parent_pid == PROCESS_NO_PARENT_PID || child_thread < 0) {
        return PROCESS_NO_PARENT_PID;
    }

    my_entry = get_current_process_entry();
    // start child thread
    thread_run(child_thread);
    // wait until child process is created
    lock_acquire(my_entry->die_lock);
    while (!dat.ready) {
        condition_wait(my_entry->die_cond, my_entry->die_lock);
    }
    lock_release(my_entry->die_lock);

    // child entry created (or creation failed), return its pid to userland
    return dat.child_pid;
}




void syscall_handle_exit(int retval) {

    process_table_t* my_entry;
    thread_table_t* my_thread;
    interrupt_status_t intr_stat;
    PID_t child_pid;
    uint32_t i;


    if (retval < 0) {
        // no negative return values accepted
        retval = -retval;
    }
    my_entry = get_current_process_entry();
    if (my_entry != NULL) {
        my_thread = thread_get_current_thread_entry();
        intr_stat = _interrupt_disable();
        lock_acquire(my_entry->die_lock);
        lock_acquire(process_table_lock);
        child_pid = my_entry->last_child_pid;
        // set all child processes to ophrans (if child process is alreay zombie then
        // it is freed totally and is ready for new execution)
        while (child_pid != PROCESS_NO_PARENT_PID) {
            process_table[child_pid].parent_pid = PROCESS_NO_PARENT_PID;
            child_pid = process_table[child_pid].next;
        }
        // mark this process as finished
        my_entry->tid = PROCESS_NO_OWNER_TID;
        my_entry->retval = retval;
        // signal joining parent (if any)
        condition_broadcast(my_entry->die_cond, my_entry->die_lock);

        // release page table
        for (i = 0 ; i < my_thread->pagetable->valid_count ; i++) {
            if (my_thread->pagetable->entries[i].V0) {
                pagepool_free_phys_page(my_thread->pagetable->entries[i].PFN0 << 12);
            }
            if (my_thread->pagetable->entries[i].V1) {
                pagepool_free_phys_page(my_thread->pagetable->entries[i].PFN1 << 12);
            }
        }
        vm_destroy_pagetable(my_thread->pagetable);
        my_thread->pagetable = NULL;

        // all done, we can release locks
        lock_release(process_table_lock);
        lock_release(my_entry->die_lock);
        _interrupt_set_state(intr_stat);
    }

    // all resources free, kill this thread
    thread_finish();

}

int syscall_handle_join(PID_t pid) {
    process_table_t* my_entry;
    process_table_t* child_entry;
    int retval;

    my_entry = get_current_process_entry();
    if (my_entry == NULL) {
        return RETVAL_SYSCALL_USERLAND_NOK;
    }
    if (pid < 0 || pid >= CONFIG_MAX_PROCESSES) {
        // entry not found
        return RETVAL_SYSCALL_USERLAND_NOK;
    }
    child_entry = process_table + pid;
    if (child_entry->parent_pid != get_current_process_pid()) {
        // not child process
        return RETVAL_SYSCALL_USERLAND_NOK;
    }

    lock_acquire(child_entry->die_lock);
    while (child_entry->tid != PROCESS_NO_OWNER_TID) {
        condition_wait(child_entry->die_cond, child_entry->die_lock);
    }
    retval = child_entry->retval;
    // return value is read, then mark child process as ready
    child_entry->parent_pid = PROCESS_NO_PARENT_PID;
    lock_release(child_entry->die_lock);

    // return acquired return value from child
    return retval;
}


#endif /* CHANGED_2 */

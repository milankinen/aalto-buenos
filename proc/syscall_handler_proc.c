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

#ifdef CHANGED_4
#include "drivers/yams.h"
#include "vm/pagetable.h"
#define STACK_BOTTOM ((USERLAND_STACK_TOP & PAGE_SIZE_MASK) \
                     - (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE)
#endif


extern lock_t* process_table_lock;
extern process_table_t process_table[CONFIG_MAX_PROCESSES];


static void new_process_thread(uint32_t dataptr) {
    child_process_create_data_t* dat;
    dat = (child_process_create_data_t*)dataptr;
    process_start(dat->filename, dat);
}

static int is_data_ready(child_process_create_data_t* d) {
    return d->ready;
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
    // wait until child process is created
    lock_acquire(my_entry->die_lock);

    // start child thread
    thread_run(child_thread);

    while (!is_data_ready(&dat)) {
        condition_wait(my_entry->die_cond, my_entry->die_lock);
    }
    lock_release(my_entry->die_lock);

    // child entry created (or creation failed), return its pid to userland
    return dat.child_pid;
}



extern void syscall_close_all_filehandles(process_table_t*);

void syscall_handle_exit(int retval) {

    process_table_t* my_entry;
    thread_table_t* my_thread;
    interrupt_status_t intr_stat;
    PID_t child_pid, my_pid;
    uint32_t i;

    if (retval < 0) {
        // no negative return values accepted
        retval = -retval;
    }
    my_pid = get_current_process_pid();
    /* silence the compiler
     * FIXME if necessary
     */
    my_pid = my_pid;

    my_entry = get_current_process_entry();
    if (my_entry != NULL) {
        syscall_close_all_filehandles(my_entry);
        my_thread = thread_get_current_thread_entry();
        intr_stat = _interrupt_disable();
        lock_acquire(my_entry->die_lock);
        lock_acquire(process_table_lock);
#ifdef CHANGED_4
        /* free heap */
        vm_unmap(my_thread->pagetable,my_entry->heap_vaddr);
#endif
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
        my_thread->userland_pid = -1;

        // all done, we can release locks
        lock_release(process_table_lock);
        lock_release(my_entry->die_lock);
        _interrupt_set_state(intr_stat);
    }

    //kprintf("exit %d\n", my_pid);
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
    if (pid <= 0 || pid > CONFIG_MAX_PROCESSES) {
        // entry not found
        return RETVAL_SYSCALL_USERLAND_NOK;
    }
    // -1 because create process returns pid+1 (we must access right table entry!)
    child_entry = process_table + pid - 1;
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

#ifdef CHANGED_4
/* check how many pages apart two virtual addresses are
 * returns 0  if the addresses are on the same page
 *         >0 if page of vaddr_old < page of vaddr_new
 *         <0 if page of vaddr_old > page of vaddr_new
 * note: this corresponds to computing vaddr_new-vaddr_old
 */
int page_diff(uint32_t vaddr_new, uint32_t vaddr_old) {
    uint32_t page_new = vaddr_new & PAGE_SIZE_MASK;
    uint32_t page_old = vaddr_old & PAGE_SIZE_MASK;
    return ((int)(page_new-page_old))/PAGE_SIZE;
}

#ifndef ADDR_IS_ON_EVEN_PAGE
#define ADDR_IS_ON_EVEN_PAGE(addr) (!((addr) & 0x00001000))
#endif
/* check if n pages can be mapped */
int can_be_mapped(int n, uint32_t start_page_vaddr, pagetable_t *pagetable) {
    int i, j, valid = (int)pagetable->valid_count;
    uint32_t vaddr;
    if (n > (PAGETABLE_ENTRIES - valid))
        return 0;
    /* check if some of these n pages is already mapped (error) */
    for (j = 0; j < n; j++) {
        for (i = 0; i<valid; i++) {
            vaddr = start_page_vaddr + j*PAGE_SIZE;
            if(pagetable->entries[i].VPN2 == (vaddr >> 13)) {
                if(ADDR_IS_ON_EVEN_PAGE(vaddr)) {
                    if(pagetable->entries[i].V0 == 1) {
                        /* already mapped */
                        return 0;
                    }
                } else {
                    if(pagetable->entries[i].V1 == 1) {
                        /* already mapped */
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

/* outline for implementation:
 *  current heap_end is calculated heap_start+offset
 *  -> offset is held in a static variable
 *     and heap_start is obtained from the process table
 *  heap_end can be freely returned to be anything requested
 *  if it is in the current page
 *  -> mapping/unmapping pages is needed when the heap_end
 *     is moved out of its page
 *  note:
 *      ensure heap does not reach stack
 *      -> preprocessor macro has the stack bottom information
 */
void * syscall_handle_memlimit(void *heap_end) {
    static uint32_t offset = 0;
    int required_pages, i;

    interrupt_status_t intr_status;
    process_table_t *proc = get_current_process_entry();
    thread_table_t *thread = thread_get_current_thread_entry();

    uint32_t heap_new = (uint32_t)heap_end;
    uint32_t heap_now = proc->heap_vaddr + offset;
    uint32_t phys_page, page_now = heap_now & PAGE_SIZE_MASK;

    if (heap_end) {
        required_pages = page_diff(heap_new, heap_now);
        /* check overlap and going behind heap start
         * Note: mapping and  getting pages needs synchronization
         *       we only have use interrupt disabling since
         *       our virtual memory is only for uniprocessor systems
         */
        intr_status = _interrupt_disable();
        if (heap_new >= STACK_BOTTOM || heap_new < proc->heap_vaddr) {
            _interrupt_set_state(intr_status);
            return NULL;
        } else if (required_pages > 0) {
            /* map pages */
            if ((required_pages > pagepool_get_free_pages())
                    || can_be_mapped(required_pages,page_now,thread->pagetable)) {
                _interrupt_set_state(intr_status);
                return NULL;
            }
            for (i = 1; i <= required_pages; i++) {
                phys_page = pagepool_get_phys_page();
                vm_map(thread->pagetable,phys_page,page_now+i*PAGE_SIZE,1);
            }
        } else if (required_pages < 0) {
            /* unmap */
            for (i = 0; i < -required_pages; i++) {
                vm_unmap(thread->pagetable,page_now-i*PAGE_SIZE);
            }
        }
        _interrupt_set_state(intr_status);
        /* else required_pages == 0
         * no need to manage pages
         */
        offset += (int)(heap_new - heap_now);
        return (void *)heap_new;
    } else {
        /* heap_end == NULL */
        return (void *)heap_now;
    }
}
#endif /*CHANGED_4*/

#endif /* CHANGED_2 */

/*
 * Process startup.
 *
 * Copyright (C) 2003-2005 Juha Aatrokoski, Timo Lilja,
 *       Leena Salmela, Teemu Takanen, Aleksi Virtanen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: process.c,v 1.11 2007/03/07 18:12:00 ttakanen Exp $
 *
 */

#include "proc/process.h"
#include "proc/elf.h"
#include "kernel/thread.h"
#include "kernel/assert.h"
#include "kernel/interrupt.h"
#include "kernel/config.h"
#include "fs/vfs.h"
#include "drivers/yams.h"
#include "vm/vm.h"
#include "vm/pagepool.h"


/** @name Process startup
 *
 * This module contains a function to start a userland process.
 */


#ifdef CHANGED_2

#include "proc/process_table.h"


process_table_t process_table[CONFIG_MAX_PROCESSES];
lock_t* process_table_lock;

static void init_process_filehandle(process_table_t* entry){
	size_t k;
    for(k = 0; k < MAX_OPEN_FILES; k++){
    	entry->filehandle[k] = FILEHANDLE_UNUSED;
    }
}

static void free_process_table_entry(process_table_t* entry) {
    entry->tid                = PROCESS_NO_OWNER_TID;
    entry->parent_pid         = PROCESS_NO_PARENT_PID;
    entry->next               = PROCESS_NO_PARENT_PID;
    entry->last_child_pid     = PROCESS_NO_PARENT_PID;
    entry->retval             = PROCESS_NO_RETVAL;
    init_process_filehandle(entry);
}

void process_table_init() {
    size_t i;
    interrupt_status_t stat;
    stat = _interrupt_disable();
    process_table_lock = lock_create();
    for (i = 0 ; i < CONFIG_MAX_PROCESSES ; i++) {
        free_process_table_entry(process_table + i);
        process_table[i].die_lock           = lock_create();
        process_table[i].die_cond           = condition_create();
        init_process_filehandle(&process_table[i]);
    }
    _interrupt_set_state(stat);
}



PID_t get_current_process_pid() {
    // we can loop table without lock because we are in read-only state and
    // entry's "tid" doesn't change during read
    PID_t found = PROCESS_NO_PARENT_PID;
    TID_t tid;
    uint32_t i;
    interrupt_status_t stat = _interrupt_disable();
    tid = thread_get_current_thread();
    for (i = 0 ; i < CONFIG_MAX_PROCESSES ; i++) {
        if (process_table[i].tid == tid && tid != PROCESS_NO_OWNER_TID) {
            found = (PID_t)i;
            break;
        }
    }
    _interrupt_set_state(stat);
    return found;
}

process_table_t* get_current_process_entry() {
    // we can loop table without lock because we are in read-only state and
    // entry's "tid" doesn't change during read
    process_table_t* found = NULL;
    TID_t tid;
    uint32_t i;
    interrupt_status_t stat = _interrupt_disable();
    tid = thread_get_current_thread();
    for (i = 0 ; i < CONFIG_MAX_PROCESSES ; i++) {
        if (process_table[i].tid == tid && tid != PROCESS_NO_OWNER_TID) {
            found = process_table + i;
            break;
        }
    }
    _interrupt_set_state(stat);
    return found;
}



static void restore_process_state(child_process_create_data_t* data, process_table_t* entry,
        int release_page_table, openfile_t executable_filehandle) {
    interrupt_status_t intr_stat;
    uint32_t i;
    process_table_t* parent_entry;
    thread_table_t* thread_entry;

    thread_entry = thread_get_current_thread_entry();
    if (entry != NULL) {
        intr_stat = _interrupt_disable();
        // process table entry has been created, we must free it
        free_process_table_entry(entry);
        _interrupt_set_state(intr_stat);
    }
    if (thread_entry && thread_entry->pagetable != NULL && release_page_table) {
        intr_stat = _interrupt_disable();
        // release page table if exists
        for (i = 0 ; i < thread_entry->pagetable->valid_count ; i++) {
            if (thread_entry->pagetable->entries[i].V0) {
                pagepool_free_phys_page(thread_entry->pagetable->entries[i].PFN0 << 12);
            }
            if (thread_entry->pagetable->entries[i].V1) {
                pagepool_free_phys_page(thread_entry->pagetable->entries[i].PFN1 << 12);
            }
        }
        vm_destroy_pagetable(thread_entry->pagetable);
        thread_entry->pagetable = NULL;
        _interrupt_set_state(intr_stat);
    }
    if (data != NULL) {
        parent_entry = process_table + data->parent_pid;
        data->ready = 1;
        // we have parent process waiting, we must inform the failure to our parent
        condition_signal(parent_entry->die_cond, parent_entry->die_lock);
        lock_release(parent_entry->die_lock);
    }
    if (executable_filehandle >= 0) {
        vfs_close(executable_filehandle);
    }
}

#endif





/**
 * Starts one userland process. The thread calling this function will
 * be used to run the process and will therefore never return from
 * this function. This function asserts that no errors occur in
 * process startup (the executable file exists and is a valid ecoff
 * file, enough memory is available, file operations succeed...).
 * Therefore this function is not suitable to allow startup of
 * arbitrary processes.
 *
 * @executable The name of the executable to be run in the userland
 * process
 */
#ifdef CHANGED_2
void process_start(const char *executable, child_process_create_data_t* data)
{
    thread_table_t *my_entry;
    pagetable_t *pagetable;
    uint32_t phys_page;
    context_t user_context;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;

    int i;

    interrupt_status_t intr_status;

    char** argv;
    char* virtual_argv[CONFIG_SYSCALL_MAX_ARGC + 1]; // +1 for filename
    int argc;
    PID_t parent_pid, my_pid;
    process_table_t* parent_proc_entry;
    process_table_t* my_proc_entry;
    int arglen;

    file = -1;
    my_pid = PROCESS_NO_PARENT_PID;
    my_proc_entry = NULL;
    if (data != NULL) {
        argc = data->argc;
        argv = data->argv;
        parent_pid = data->parent_pid;
        parent_proc_entry = process_table + parent_pid;
        // parent process is waiting until child is created
        lock_acquire(parent_proc_entry->die_lock);
    } else {
        argc = 0;
        argv = NULL;
        parent_pid = PROCESS_NO_PARENT_PID;
        parent_proc_entry = NULL;
    }

    intr_status = _interrupt_disable();
    lock_acquire(process_table_lock);

    for (i = 0 ; i < CONFIG_MAX_PROCESSES ; i++) {
        if (process_table[i].tid == PROCESS_NO_OWNER_TID &&
                process_table[i].parent_pid == PROCESS_NO_PARENT_PID) {
            // found a free process entry, fill it properly
            my_proc_entry                   = process_table + i;
            my_proc_entry->tid              = thread_get_current_thread();
            my_proc_entry->parent_pid       = parent_pid;
            my_proc_entry->next             = PROCESS_NO_PARENT_PID;
            my_proc_entry->last_child_pid   = PROCESS_NO_PARENT_PID;
            my_proc_entry->retval           = PROCESS_NO_RETVAL;
            stringcopy(my_proc_entry->name, "foobar", PROCESS_NAME_MAX_LENGTH);
            my_pid = (PID_t)i;
            if (parent_proc_entry != NULL) {
                // correct child process linked list
                if (parent_proc_entry->last_child_pid != PROCESS_NO_PARENT_PID) {
                    my_proc_entry->next = parent_proc_entry->last_child_pid;
                }
                parent_proc_entry->last_child_pid = my_pid;
            }
            break;
        }
    }

    lock_release(process_table_lock);
    _interrupt_set_state(intr_status);

    if (my_proc_entry == NULL) {
        // no free process entries found, restore state and release possible locks
        restore_process_state(data, my_proc_entry, 1, file);
        return;
    }

    my_entry = thread_get_current_thread_entry();


    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    if(my_entry->pagetable != NULL) {
        restore_process_state(data, my_proc_entry, 1, file);
        return;
    }

    pagetable = vm_create_pagetable(thread_get_current_thread());
    if(pagetable == NULL) {
        restore_process_state(data, my_proc_entry, 1, file);
        return;
    }

    intr_status = _interrupt_disable();
    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);

    /* Make sure the file existed and was a valid ELF file */
    if (file < 0 || !elf_parse_header(&elf, file)) {
        restore_process_state(data, my_proc_entry, 1, file);
        return;
    }

    /* Trivial and naive sanity check for entry point: */
    if(elf.entry_point < PAGE_SIZE) {
        restore_process_state(data, my_proc_entry, 1, file);
        return;
    }

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    if(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
		  > _tlb_get_maxindex() + 1) {
        restore_process_state(data, my_proc_entry, 1, file);
        return;
    }

    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        if(phys_page == 0) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if (vm_map(my_entry->pagetable, phys_page,
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1) < 0) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        if(phys_page == 0) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if (vm_map(my_entry->pagetable, phys_page,
               elf.ro_vaddr + i*PAGE_SIZE, 1) < 0) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        if(phys_page == 0) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if (vm_map(my_entry->pagetable, phys_page,
               elf.rw_vaddr + i*PAGE_SIZE, 1) < 0) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
    }

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *)elf.ro_vaddr, 0, elf.ro_pages*PAGE_SIZE);
    memoryset((void *)elf.rw_vaddr, 0, elf.rw_pages*PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) -
        (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE;
    memoryset((void *)stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE*PAGE_SIZE);

    /* Copy segments */

    if (elf.ro_size > 0) {
	/* Make sure that the segment is in proper place. */
        if(elf.ro_vaddr < PAGE_SIZE) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if(vfs_seek(file, elf.ro_location) != VFS_OK) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if(vfs_read(file, (void *)elf.ro_vaddr, elf.ro_size) != (int)elf.ro_size) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
    }

    if (elf.rw_size > 0) {
	/* Make sure that the segment is in proper place. */
        if(elf.rw_vaddr < PAGE_SIZE) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if(vfs_seek(file, elf.rw_location) != VFS_OK) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
        if(vfs_read(file, (void *)elf.rw_vaddr, elf.rw_size) != (int)elf.rw_size) {
            restore_process_state(data, my_proc_entry, 1, file);
            return;
        }
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        vm_set_dirty(my_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    /* Insert page mappings again to TLB to take read-only bits into use */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = elf.entry_point;

    vfs_close(file);

    // add main arguments
    for (i = 0 ; i < argc ; i++) {
        arglen = strlen(argv[i]) + 1;
        // reserve space from stack
        user_context.cpu_regs[MIPS_REGISTER_SP] -= arglen;
        // ensure that stack is 4-bytes aligned
        user_context.cpu_regs[MIPS_REGISTER_SP] -= user_context.cpu_regs[MIPS_REGISTER_SP] % sizeof(uint32_t);

        // copy our string to it
        memcopy(arglen, (void*)(user_context.cpu_regs[MIPS_REGISTER_SP]), argv[i]);
        virtual_argv[i + 1] = (char*)(user_context.cpu_regs[MIPS_REGISTER_SP]);
    }

    // filename is ALWAYS first argument
    arglen = strlen(executable) + 1;
    user_context.cpu_regs[MIPS_REGISTER_SP] -= arglen;
    // ensure that stack is 4-bytes aligned
    user_context.cpu_regs[MIPS_REGISTER_SP] -= user_context.cpu_regs[MIPS_REGISTER_SP] % sizeof(uint32_t);
    // copy executable name to free space
    memcopy(arglen, (void*)(user_context.cpu_regs[MIPS_REGISTER_SP]), executable);
    virtual_argv[0] = (char*)(user_context.cpu_regs[MIPS_REGISTER_SP]);
    argc++;


    // construct char**
    for (i = argc - 1 ; i >= 0 ; i--) {
        // reserve space from stack
        user_context.cpu_regs[MIPS_REGISTER_SP] -= sizeof(char*);
        // write argument address to space
        memcopy(sizeof(char*), (void*)(user_context.cpu_regs[MIPS_REGISTER_SP]), virtual_argv + i);
    }

    user_context.cpu_regs[MIPS_REGISTER_A0] = argc;
    user_context.cpu_regs[MIPS_REGISTER_A1] = argc > 0 ? user_context.cpu_regs[MIPS_REGISTER_SP] : 0;


    // decrement stack pointer by two parameters (argc, argv)
    user_context.cpu_regs[MIPS_REGISTER_SP] -= 2 * sizeof(uint32_t);

    if (data != NULL) {
        //kprintf("start: %d\n", my_pid);
        // all ok, let parent process to resume (+1 so that pid is never 0)
        data->child_pid = my_pid + 1;
        data->ready = 1;
        condition_broadcast(parent_proc_entry->die_cond, parent_proc_entry->die_lock);
        lock_release(parent_proc_entry->die_lock);
    }

    thread_goto_userland(&user_context);

    // no shit.. no can do..
    //KERNEL_PANIC("thread_goto_userland failed.");
}

#else
void process_start(const char *executable)
{
    thread_table_t *my_entry;
    pagetable_t *pagetable;
    uint32_t phys_page;
    context_t user_context;
    uint32_t stack_bottom;
    elf_info_t elf;
    openfile_t file;

    int i;

    interrupt_status_t intr_status;

    my_entry = thread_get_current_thread_entry();


    /* If the pagetable of this thread is not NULL, we are trying to
       run a userland process for a second time in the same thread.
       This is not possible. */
    KERNEL_ASSERT(my_entry->pagetable == NULL);

    pagetable = vm_create_pagetable(thread_get_current_thread());
    KERNEL_ASSERT(pagetable != NULL);

    intr_status = _interrupt_disable();
    my_entry->pagetable = pagetable;
    _interrupt_set_state(intr_status);

    file = vfs_open((char *)executable);

    /* Make sure the file existed and was a valid ELF file */
    KERNEL_ASSERT(file >= 0);
    KERNEL_ASSERT(elf_parse_header(&elf, file));

    /* Trivial and naive sanity check for entry point: */
    KERNEL_ASSERT(elf.entry_point >= PAGE_SIZE);

    /* Calculate the number of pages needed by the whole process
       (including userland stack). Since we don't have proper tlb
       handling code, all these pages must fit into TLB. */
    KERNEL_ASSERT(elf.ro_pages + elf.rw_pages + CONFIG_USERLAND_STACK_SIZE
          <= _tlb_get_maxindex() + 1);

    /* Allocate and map stack */
    for(i = 0; i < CONFIG_USERLAND_STACK_SIZE; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - i*PAGE_SIZE, 1);
    }

    /* Allocate and map pages for the segments. We assume that
       segments begin at page boundary. (The linker script in tests
       directory creates this kind of segments) */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.ro_vaddr + i*PAGE_SIZE, 1);
    }

    for(i = 0; i < (int)elf.rw_pages; i++) {
        phys_page = pagepool_get_phys_page();
        KERNEL_ASSERT(phys_page != 0);
        vm_map(my_entry->pagetable, phys_page, 
               elf.rw_vaddr + i*PAGE_SIZE, 1);
    }

    /* Put the mapped pages into TLB. Here we again assume that the
       pages fit into the TLB. After writing proper TLB exception
       handling this call should be skipped. */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);
    
    /* Now we may use the virtual addresses of the segments. */

    /* Zero the pages. */
    memoryset((void *)elf.ro_vaddr, 0, elf.ro_pages*PAGE_SIZE);
    memoryset((void *)elf.rw_vaddr, 0, elf.rw_pages*PAGE_SIZE);

    stack_bottom = (USERLAND_STACK_TOP & PAGE_SIZE_MASK) - 
        (CONFIG_USERLAND_STACK_SIZE-1)*PAGE_SIZE;
    memoryset((void *)stack_bottom, 0, CONFIG_USERLAND_STACK_SIZE*PAGE_SIZE);

    /* Copy segments */

    if (elf.ro_size > 0) {
    /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.ro_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.ro_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.ro_vaddr, elf.ro_size)
              == (int)elf.ro_size);
    }

    if (elf.rw_size > 0) {
    /* Make sure that the segment is in proper place. */
        KERNEL_ASSERT(elf.rw_vaddr >= PAGE_SIZE);
        KERNEL_ASSERT(vfs_seek(file, elf.rw_location) == VFS_OK);
        KERNEL_ASSERT(vfs_read(file, (void *)elf.rw_vaddr, elf.rw_size)
              == (int)elf.rw_size);
    }


    /* Set the dirty bit to zero (read-only) on read-only pages. */
    for(i = 0; i < (int)elf.ro_pages; i++) {
        vm_set_dirty(my_entry->pagetable, elf.ro_vaddr + i*PAGE_SIZE, 0);
    }

    /* Insert page mappings again to TLB to take read-only bits into use */
    intr_status = _interrupt_disable();
    tlb_fill(my_entry->pagetable);
    _interrupt_set_state(intr_status);

    /* Initialize the user context. (Status register is handled by
       thread_goto_userland) */
    memoryset(&user_context, 0, sizeof(user_context));
    user_context.cpu_regs[MIPS_REGISTER_SP] = USERLAND_STACK_TOP;
    user_context.pc = elf.entry_point;

    thread_goto_userland(&user_context);

    KERNEL_PANIC("thread_goto_userland failed.");
}
#endif


/** @} */

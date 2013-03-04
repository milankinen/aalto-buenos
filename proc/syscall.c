/*
 * System calls.
 *
 * Copyright (C) 2003 Juha Aatrokoski, Timo Lilja,
 *   Leena Salmela, Teemu Takanen, Aleksi Virtanen.
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
 * $Id: syscall.c,v 1.3 2004/01/13 11:10:05 ttakanen Exp $
 *
 */
#include "kernel/cswitch.h"
#include "proc/syscall.h"
#include "kernel/halt.h"
#include "kernel/panic.h"
#include "lib/libc.h"
#include "kernel/assert.h"
#include "kernel/thread.h"

#ifdef CHANGED_2
#   include "kernel/config.h"
#   include "vm/vm.h"
#endif



#ifdef CHANGED_2

static void handle_execp(context_t *user_context, thread_table_t *my_entry) {
    char filename[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    // for argv data
    char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    // actual pointer array to string arguments
    char* argv[CONFIG_SYSCALL_MAX_ARGC];
    int used, total_used, i, argc, readed_args;
    char** virt_argv;
    char* virt_filename;
    // this is not actually used, vm_get_vaddr_page_offsets just needs it
    uint32_t dummy;
    PID_t created_child;


    // ===== FILENAME ======

    virt_filename = (char*)user_context->cpu_regs[MIPS_REGISTER_A1];
    if (!vm_get_vaddr_page_offsets(my_entry->pagetable, (uint32_t)virt_filename, &dummy, &dummy)) {
        // filename address not inside userland memory
        user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)RETVAL_SYSCALL_USERLAND_NOK;
        return;
    }

    if (read_string_from_vm(my_entry->pagetable, virt_filename, filename,
            CONFIG_SYSCALL_MAX_BUFFER_SIZE) == RETVAL_SYSCALL_HELPERS_NOK) {
        // too long filename
        user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)RETVAL_SYSCALL_USERLAND_NOK;
        return;
    }

    // ===== EXECP ARGUMENTS =====

    argc = (int)user_context->cpu_regs[MIPS_REGISTER_A2];
    virt_argv = (char**)user_context->cpu_regs[MIPS_REGISTER_A3];
    if (argc < 0 || (argc > 0 && !vm_get_vaddr_page_offsets(my_entry->pagetable, (uint32_t)virt_argv,
            &dummy, &dummy)) || argc >= CONFIG_SYSCALL_MAX_ARGC) {
        // negative number of arguments or string argument array is not inside userland memory
        // or too many arguments
        user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)RETVAL_SYSCALL_USERLAND_NOK;
        return;
    }
    total_used = 0;
    readed_args = 0;

    for (i = 0 ; i < argc ; i++) {
        if (!vm_get_vaddr_page_offsets(my_entry->pagetable, (uint32_t)virt_argv + i, &dummy, &dummy)) {
            // i'th argument pointer not inside userland memory
            user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)RETVAL_SYSCALL_USERLAND_NOK;
            return;
        }
        used = read_string_from_vm(my_entry->pagetable, virt_argv[i], temp + total_used,
                CONFIG_SYSCALL_MAX_BUFFER_SIZE - total_used);

        if (used == RETVAL_SYSCALL_HELPERS_NOK) {
            // overflow
            user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)RETVAL_SYSCALL_USERLAND_NOK;
            return;
        }
        // argument readed successfully, add pointer to it
        argv[readed_args++] = temp + total_used;
        total_used += used;
    }

    // all data readed successfully, we can start our the child process
    created_child = syscall_handle_execp(filename, argc, argv);
    if (created_child < 0) {
        // child process creation failed for some reason
        user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)RETVAL_SYSCALL_USERLAND_NOK;
        return;
    }

    // return child processes id
    user_context->cpu_regs[MIPS_REGISTER_V0] = (uint32_t)created_child;
}

#endif


/**
 * Handle system calls. Interrupts are enabled when this function is
 * called.
 *
 * @param user_context The userland context (CPU registers as they
 * where when system call instruction was called in userland)
 */
void syscall_handle(context_t *user_context)
{
    /* When a syscall is executed in userland, register a0 contains
     * the number of the syscall. Registers a1, a2 and a3 contain the
     * arguments of the syscall. The userland code expects that after
     * returning from the syscall instruction the return value of the
     * syscall is found in register v0. Before entering this function
     * the userland context has been saved to user_context and after
     * returning from this function the userland context will be
     * restored from user_context.
     */
#ifdef CHANGED_2

    thread_table_t *my_entry;
    my_entry = thread_get_current_thread_entry();

    switch(user_context->cpu_regs[MIPS_REGISTER_A0]) {
    case SYSCALL_HALT:
        halt_kernel();
        break;
    case SYSCALL_EXEC:
        handle_execp(user_context, my_entry);
        break;
    case SYSCALL_EXIT:
        syscall_handle_exit((int)user_context->cpu_regs[MIPS_REGISTER_A1]);
        break;
    case SYSCALL_JOIN:
        user_context->cpu_regs[MIPS_REGISTER_V0] =
                (uint32_t)syscall_handle_join((PID_t)user_context->cpu_regs[MIPS_REGISTER_A1]);
        break;
    default:
        KERNEL_PANIC("Unhandled system call\n");
    }

#else
    switch(user_context->cpu_regs[MIPS_REGISTER_A0]) {
    case SYSCALL_HALT:
        halt_kernel();
        break;
    default: 
        KERNEL_PANIC("Unhandled system call\n");
    }
#endif

    /* Move to next instruction after system call */
    user_context->pc += 4;
}




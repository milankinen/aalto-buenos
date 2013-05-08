/*
 * TLB handling
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
 * $Id: tlb.c,v 1.6 2004/04/16 10:54:29 ttakanen Exp $
 *
 */

#include "kernel/panic.h"
#include "kernel/assert.h"
#include "vm/tlb.h"
#include "vm/pagetable.h"

#ifdef CHANGED_4
#   include "kernel/interrupt.h"
#   include "vm/vm.h"
#   include "kernel/thread.h"
#   include "proc/syscall.h"
#endif


#ifdef CHANGED_4

#ifndef ADDR_IS_ON_EVEN_PAGE
#define ADDR_IS_ON_EVEN_PAGE(addr) (!((addr) & 0x00001000))
#endif

static void kill() {
    kprintf("KILL!\n");
    if (thread_get_current_thread_entry()->pagetable != NULL) {
        // process
        syscall_handle_exit(PROCESS_EXIT_STATUS_EXCEPTION_TLBS);
    } else {
        // kernel thread (wtf, is this possible??)
        thread_finish();
    }
}

static int try_to_fill(int check_dirty) {
    pagetable_t* pagetable;
    tlb_entry_t* entry;
    tlb_exception_state_t state;
    _tlb_get_exception_state(&state);

    pagetable = thread_get_current_thread_entry()->pagetable;
    KERNEL_ASSERT(pagetable != NULL);

    // get tlb entry for bad address
    entry = vm_get_entry_by_vaddr(pagetable, state.badvaddr);
    if (entry != NULL ) {
        // there is entry already, check that it has D-bit on
        if (ADDR_IS_ON_EVEN_PAGE(state.badvaddr)) {
            if (entry->V0 == 1 && (!check_dirty || entry->D0 == 1)) {
                // address is inside active page that has D-bit enabled
                // --> we can fill proper entry to TLB and try again
                _tlb_write_random(entry);
                _tlb_set_asid(pagetable->ASID);
                return 1;
            }
        } else {
            if (entry->V1 == 1 && (!check_dirty || entry->D1 == 1)) {
                // address is inside active page that has D-bit enabled
                // --> we can fill proper entry to TLB and try again
                _tlb_write_random(entry);
                _tlb_set_asid(pagetable->ASID);
                return 1;
            }
        }
    }
    return 0;
}

void tlb_modified_exception(void)
{
    // nothing to do
    //kprintf("MODIFIED\n");
    kill();
}

void tlb_load_exception(void)
{
    //kprintf("LOAD\n");
    if (!try_to_fill(0)) {
        // there is no way to correct TLB --> terminate thread / exit process
        kill();
    }
}

void tlb_store_exception(void)
{
    //kprintf("STORE\n");
    if (!try_to_fill(1)) {
        // there is no way to correct TLB --> terminate thread / exit process
        kill();
    }
}

#else

void tlb_modified_exception(void)
{
    KERNEL_PANIC("Unhandled TLB modified exception");
}

void tlb_load_exception(void)
{
    KERNEL_PANIC("Unhandled TLB load exception");
}

void tlb_store_exception(void)
{
    KERNEL_PANIC("Unhandled TLB store exception");
}

#endif /* CHANGED_4 */

/**
 * Fill TLB with given pagetable. This function is used to set memory
 * mappings in CP0's TLB before we have a proper TLB handling system.
 * This approach limits the maximum mapping size to 128kB.
 *
 * @param pagetable Mappings to write to TLB.
 *
 */

void tlb_fill(pagetable_t *pagetable)
{
    if(pagetable == NULL)
	return;

    /* Check that the pagetable can fit into TLB. This is needed until
     we have proper VM system, because the whole pagetable must fit
     into TLB. */
    KERNEL_ASSERT(pagetable->valid_count <= (_tlb_get_maxindex()+1));

    _tlb_write(pagetable->entries, 0, pagetable->valid_count);

    /* Set ASID field in Co-Processor 0 to match thread ID so that
       only entries with the ASID of the current thread will match in
       the TLB hardware. */
    _tlb_set_asid(pagetable->ASID);
}


#ifdef CHANGED_4

void tlb_replace_entry_if_exists(tlb_entry_t* old, tlb_entry_t* new) {
    int index;
    interrupt_status_t st;

    st = _interrupt_disable();
    index = _tlb_probe(old);
    if (index >= 0) {
        // tlb entry found, replace it
        _tlb_write(new, index, 1);
    }
    _interrupt_set_state(st);
}

void tlb_write_if_not_exists(tlb_entry_t* entry) {
    int index;
    interrupt_status_t st;

    st = _interrupt_disable();
    index = _tlb_probe(entry);
    if (index < 0) {
        // write new entry using random policy
        _tlb_write_random(entry);
    }
    _interrupt_set_state(st);
}

#endif /* CHANGED_4 */

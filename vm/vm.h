/*
 * Virtual memory initialization
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
 * $Id: vm.h,v 1.5 2003/05/21 10:42:29 lsalmela Exp $
 *
 */

#ifndef BUENOS_VM_VM_H
#define BUENOS_VM_VM_H

#include "vm/pagetable.h"

void vm_init(void);

pagetable_t *vm_create_pagetable(uint32_t asid);
void vm_destroy_pagetable(pagetable_t *pagetable);

#ifdef CHANGED_2
int vm_map(pagetable_t *pagetable, uint32_t physaddr,
	    uint32_t vaddr, int dirty);
#else
void vm_map(pagetable_t *pagetable, uint32_t physaddr, 
        uint32_t vaddr, int dirty);
#endif

void vm_unmap(pagetable_t *pagetable, uint32_t vaddr);

void vm_set_dirty(pagetable_t *pagetable, uint32_t vaddr, int dirty);

#ifdef CHANGED_2

/**
 * Gets physical and virtual page start offsets from given virtual address and pagetable.
 * This function should be called only after pagetable is properly filled.
 *
 * @param pagetable
 *          Page table where the offsets are fetched.
 * @param vaddr
 *          Virtual address whose page offsets are checked.
 * @param p_physpageoff
 *          Pointer where the fetched physical page offset is placed. Set to 0 if virtual address
 *          doesn't belong to given page table.
 * @param p_virtpageoff
 *          Pointer where the fetched virtual page offset is placed. Set to 0 if virtual address
 *          doesn't belong to given page table.
 * @returns
 *          1 if virtual address belongs to the given page table. 0 if not.
 */
int vm_get_vaddr_page_offsets(pagetable_t *pagetable, uint32_t vaddr,
        uint32_t* p_physpageoff, uint32_t* p_virtpageoff);

#endif /* CHANGED_2 */

#endif /* BUENOS_VM_VM_H */

/*
 * syscall_handler_fs.c
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_2

#include "proc/syscall.h"
#include "vm/vm.h"


int read_string_from_vm(pagetable_t* pagetable, const char* virtual_source,
        char* physical_target, int max_length) {

    uint32_t i, j, virtual_page_start, virtual_page_end, vaddr, phys_page_start;
    int inside_pagetable;

    i = 0;
    while (1) {
        vaddr = (uint32_t)(virtual_source + i);
        inside_pagetable = vm_get_vaddr_page_offsets(pagetable, vaddr, &phys_page_start, &virtual_page_start);
        if (!inside_pagetable) {
            // not inside caller processes virtual page table
            return RETVAL_SYSCALL_HELPERS_NOK;
        }
        virtual_page_end = virtual_page_start + PAGE_SIZE;
        for (j = 0 ; vaddr + i < virtual_page_end ; i++, j++) {
            if (i >= (uint32_t)max_length) {
                // buffer overflow: too long string to read
                return RETVAL_SYSCALL_HELPERS_NOK;
            }
            physical_target[i] = virtual_source[i];
            if (physical_target[i] == '\0') {
                // found line ending, return number of bytes used (=chars readed)
                return i + 1;
            }
        }

    }
    // never reach this far, we must add this to prevent compiler error
    return RETVAL_SYSCALL_HELPERS_NOK;
}

int read_data_from_vm(pagetable_t* pagetable, const void* virtual_source,
        void* physical_target, int size) {
    uint32_t i, j, virtual_page_start, virtual_page_end, vaddr, phys_page_start;
    int inside_pagetable;

    i = 0;
    while (i < (uint32_t)size) {
        vaddr = (uint32_t)(virtual_source + i);
        inside_pagetable = vm_get_vaddr_page_offsets(pagetable, vaddr, &phys_page_start, &virtual_page_start);
        if (!inside_pagetable) {
            // not inside caller processes virtual page table
            return RETVAL_SYSCALL_HELPERS_NOK;
        }
        virtual_page_end = virtual_page_start + PAGE_SIZE;
        for (j = 0 ; vaddr + i < virtual_page_end ; i++, j++) {
            ((char*)physical_target)[i] = ((char*)virtual_source)[vaddr + j];
        }

    }
    // block with given size was read successfully
    return RETVAL_SYSCALL_HELPERS_OK;
}


int write_data_to_vm(pagetable_t* pagetable, const void* physical_source,
        void* virtual_target, int size) {
    uint32_t i, j, virtual_page_start, virtual_page_end, vaddr, phys_page_start;
    int inside_pagetable;

    i = 0;
    while (i < (uint32_t)size) {
        vaddr = (uint32_t)(virtual_target + i);
        inside_pagetable = vm_get_vaddr_page_offsets(pagetable, vaddr, &phys_page_start, &virtual_page_start);
        if (!inside_pagetable) {
            // not inside caller processes virtual page table
            return RETVAL_SYSCALL_HELPERS_NOK;
        }
        virtual_page_end = virtual_page_start + PAGE_SIZE;
        for (j = 0 ; vaddr + i < virtual_page_end ; i++, j++) {
            ((char*)virtual_target)[vaddr + j] = ((char*)physical_source)[i];
        }

    }
    // block with given size was written successfully
    return RETVAL_SYSCALL_HELPERS_OK;
}


#endif /* CHANGED_2 */

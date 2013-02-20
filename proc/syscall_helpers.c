/*
 * syscall_handler_fs.c
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_2

#include "proc/syscall.h"


void read_string_from_vm(pagetable_t* pagetable, const char* virtual_source,
        char* physical_target, uint32_t max_length) {
    // TODO: implementation
    max_length = *((const uint32_t*)virtual_source);
    max_length = max_length;
    physical_target = physical_target;
    pagetable = pagetable;
}

void read_data_from_vm(pagetable_t* pagetable, const void* virtual_source,
        void* physical_target, uint32_t size) {
    // TODO: implementation
    size = *((const uint32_t*)virtual_source);
    size = size;
    physical_target = physical_target;
    pagetable = pagetable;
}

void write_data_to_vm(pagetable_t* pagetable, const void* physical_source,
        void* virtual_target, uint32_t size) {
    size = *((const uint32_t*)physical_source);
    size = size;
    virtual_target = virtual_target;
    pagetable = pagetable;
}


#endif /* CHANGED_2 */

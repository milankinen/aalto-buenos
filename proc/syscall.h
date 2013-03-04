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
 * $Id: syscall.h,v 1.5 2005/02/23 09:34:34 lsalmela Exp $
 *
 */

#ifndef BUENOS_PROC_SYSCALL
#define BUENOS_PROC_SYSCALL


#ifdef CHANGED_2

#include "proc/process.h"
#include "proc/process_table.h"
#include "fs/vfs.h"

/** Return values for internal helpers */
#define RETVAL_SYSCALL_HELPERS_NOK -1
#define RETVAL_SYSCALL_HELPERS_OK 1

/* Return value for userland when system call fails but doesn't kill process */
#define RETVAL_SYSCALL_USERLAND_NOK -1

/**
 * Reads the system call string parameter (from process) to a given memory address.
 * This method performs conversions from virtual address to physical address and vice versa.
 *
 * @param pagetable Page table for virtual address conversions.
 * @param virtual_source
 *         Pointer to a VIRTUAL MEMORY address of calling process, which contains the
 *         source string from caller process. (This points to the first character and terminates
 *         to '\0')
 * @param physical_target
 *         Pointer to a PHYSICAL address where the readed string is stored. Kernel code
 *         must allocate memory for this data.
 * @param max_length
 *         Maximum length which can be readed to the target_string parameter.
 */
int read_string_from_vm(pagetable_t* pagetable, const char* virtual_source,
        char* physical_target, int max_length);

/**
 * Reads the system call provided data (from process) to a given memory adress.
 * This method performs conversions from virtual address to physical address and vice versa.
 *
 * @param pagetable Page table for virtual address conversions.
 * @param virtual_source
 *        Pointer to a VIRTUAL MEMORY address of calling process, which contains the
 *        source data.
 * @param physical_target
 *        Pointer to a PHYSICAL address where the readed source data is stored. Kernel code
 *        must ensure that it's allowed to write the given size to this memory block.
 * @params size
 *        Number of bytes to read.
 */
int read_data_from_vm(pagetable_t* pagetable, const void* virtual_source,
        void* physical_target, int size);

/**
 * Writes the given data block to the processes
 * This method performs conversions from virtual address to physical address and vice versa.
 *
 * @param pagetable Page table for virtual address conversions.
 * @param physical_source
 *        Pointer to a PHYSICAL address where the source data is readed.
 * @param virtual_target
 *        Pointer to a VIRTUAL MEMORY address of calling process, which contains the target
 *        address where written data is stored.
 * @params size
 *        Number of bytes to write.
 */
int write_data_to_vm(pagetable_t* pagetable, const void* physical_source,
        void* virtual_target, int size);



/* SYSCALL HANDLER DEFINITIONS */

PID_t syscall_handle_execp(const char *filename, int argc, char** argv);

void syscall_handle_exit(int retval);

int syscall_handle_join(PID_t pid);

openfile_t syscall_handle_open(const char *filename);

int syscall_handle_close(openfile_t filehandle);

int syscall_handle_seek(openfile_t filehandle, int offset);

int syscall_handle_read(openfile_t filehandle, void *buffer, int length);

int syscall_handle_write(openfile_t filehandle, const void *buffer, int length);

int syscall_handle_create(const char *filename, int size);

int syscall_handle_delete(const char *filename);


#endif /* CHANGED_2 */



/* Syscall function numbers. You may add to this list but do not
 * modify the existing ones.
 */
#define SYSCALL_HALT 0x001
#define SYSCALL_EXEC 0x101
#define SYSCALL_EXIT 0x102
#define SYSCALL_JOIN 0x103
#define SYSCALL_FORK 0x104
#define SYSCALL_MEMLIMIT 0x105
#define SYSCALL_OPEN 0x201
#define SYSCALL_CLOSE 0x202
#define SYSCALL_SEEK 0x203
#define SYSCALL_READ 0x204
#define SYSCALL_WRITE 0x205
#define SYSCALL_CREATE 0x206
#define SYSCALL_DELETE 0x207



/* When userland program reads or writes these already open files it
 * actually accesses the console.
 */
#define FILEHANDLE_STDIN 0
#define FILEHANDLE_STDOUT 1
#define FILEHANDLE_STDERR 2

#endif

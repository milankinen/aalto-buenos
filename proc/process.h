/*
 * Process startup.
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
 * $Id: process.h,v 1.4 2003/05/16 10:13:55 ttakanen Exp $
 *
 */

#ifndef BUENOS_PROC_PROCESS
#define BUENOS_PROC_PROCESS


typedef int process_id_t;

#ifdef CHANGED_2


/** These values indicates different exist statuses for process. */
#define PROCESS_EXIT_STATUS_EXCEPTION_TLBM      653010
#define PROCESS_EXIT_STATUS_EXCEPTION_TLBL      653011
#define PROCESS_EXIT_STATUS_EXCEPTION_TLBS      653012
#define PROCESS_EXIT_STATUS_EXCEPTION_ADDRL     653013
#define PROCESS_EXIT_STATUS_EXCEPTION_ADDRS     653014
#define PROCESS_EXIT_STATUS_EXCEPTION_BUSI      653015
#define PROCESS_EXIT_STATUS_EXCEPTION_BUSD      653016
#define PROCESS_EXIT_STATUS_EXCEPTION_BREAK     653017
#define PROCESS_EXIT_STATUS_EXCEPTION_RESVI     653018
#define PROCESS_EXIT_STATUS_EXCEPTION_COPROC    653019
#define PROCESS_EXIT_STATUS_EXCEPTION_AOFLOW    653020
#define PROCESS_EXIT_STATUS_EXCEPTION_TRAP      653021
#define PROCESS_EXIT_STATUS_EXCEPTION_UNKNOWN   653022



/** This constant indicates that process has no parent. */
#define PROCESS_NO_PARENT_PID -1
/** This constant indicates that process has no owner thread so it is not in the execution atm. */
#define PROCESS_NO_OWNER_TID -1
/** This constant indicates that process has not exited yet. */
#define PROCESS_NO_RETVAL -1


typedef process_id_t PID_t;

/**
 * This struct is passed as an pointer to created child process. And it transfers
 * all needed data for execp syscall.
 */
typedef struct {
    PID_t parent_pid;
    PID_t child_pid;
    int ready;
    const char* filename;
    int argc;
    char** argv;
} child_process_create_data_t;


void process_start(const char *executable, child_process_create_data_t* data);


#else

void process_start(const char *executable);

#endif



#define USERLAND_STACK_TOP 0x7fffeffc

#endif

/*
 * syscall_handler_fs.c
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_2

#include "proc/syscall.h"


openfile_t syscall_handle_open(const char *filename) {
    // TODO: implementation
    return *((const openfile_t*)filename);
}

int syscall_handle_close(openfile_t filehandle) {
    // TODO: implementation
    filehandle = filehandle;
    return 0;
}

int syscall_handle_seek(openfile_t filehandle, int offset) {
    // TODO: implementation
    filehandle = filehandle;
    offset = offset;
    return 0;
}

int syscall_handle_read(openfile_t filehandle, void *buffer, int length) {
    // TODO: implementation
    filehandle = filehandle;
    length = length;
    buffer = buffer;
    return 0;
}

int syscall_handle_write(openfile_t filehandle, const void *buffer, int length) {
    // TODO: implementation
    filehandle = filehandle;
    length = *((const int*)buffer);
    return 0;
}

int syscall_handle_create(const char *filename, int size) {
    // TODO: implementation
    size = *((const int*)filename);
    return 0;
}

int syscall_handle_delete(const char *filename) {
    // TODO: implementation
    return *((const int*)filename);;
}

#endif /* CHANGED_2 */

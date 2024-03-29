/*
 * syscall_handler_fs.c
 *
 *  Created on: 20 Feb 2013
 *      Author: matti
 */

#ifdef CHANGED_2

#include "proc/syscall.h"
#include "kernel/thread.h"
#include "vm/pagetable.h"
#include "drivers/gcd.h"
#include "kernel/config.h"

/*close all filehandles attached to the process*/
void syscall_close_all_filehandles(process_table_t *pt) {
    int i;
    /*try to find the filehandle and close it */
    for (i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        int filehandle = pt->filehandle[i];
        if (filehandle > 0) {
            vfs_close(filehandle);
        }
        pt->filehandle[i] = -1;
    }
}

/*check that the process has the given filehandle */
static int check_filehandle(openfile_t filehandle, process_table_t* pt) {
    int i;
    int filehandle_found = -1;
    for (i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        if (pt->filehandle[i] == filehandle) {
            filehandle_found = 1;
            break;
        }
    }
    if (filehandle_found == -1) {
        return VFS_NOT_OPEN;
    }
    return 1;
}

openfile_t syscall_handle_open(const char *filename) {

    process_table_t *pt = get_current_process_entry();
    int i;
    thread_table_t *t = thread_get_current_thread_entry();
    pagetable_t *p = t->pagetable;
    char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    if (read_string_from_vm(p, filename, temp,
            VFS_PATH_LENGTH)==RETVAL_SYSCALL_HELPERS_NOK) {
        kprintf("Process was killed\n");
        syscall_handle_exit(0);
        return -1;
    }
    for (i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        if (pt->filehandle[i] == FILEHANDLE_UNUSED) {
            int filehandle = vfs_open((char*) temp);
            if (filehandle < 0) {
                return filehandle;
            }
            pt->filehandle[i] = filehandle;
            return filehandle;
        }
    }
    return VFS_ERROR;
}

int syscall_handle_close(openfile_t filehandle) {
    int i;
    int filehandle_found = -1;
    /* illegal filehandle */
    if (filehandle < 0)
        return FILEHANDLE_ILLEGAL;
    process_table_t *pt = get_current_process_entry();
    /*try to find the filehandle and close it */
    for (i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        if (pt->filehandle[i] == filehandle) {
            vfs_close(filehandle);
            filehandle_found = 1;
            pt->filehandle[i] = -1;
            break;
        }
    }
    if (filehandle_found == -1) {
        return VFS_NOT_OPEN;
    }
    return 0;
}

int syscall_handle_seek(openfile_t filehandle, int offset) {

    /* illegal filehandle */
    if (filehandle < 0)
        return FILEHANDLE_ILLEGAL;
    process_table_t *pt = get_current_process_entry();

    if (check_filehandle(filehandle, pt) == VFS_NOT_OPEN) {
        return VFS_NOT_FOUND;
    }
    return vfs_seek(filehandle, offset);

}

int syscall_handle_read(openfile_t filehandle, void *buffer, int length) {
    /* illegal filehandle */
    if (filehandle < 0)
        return FILEHANDLE_ILLEGAL;
    char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    thread_table_t *t = thread_get_current_thread_entry();
    pagetable_t *p = t->pagetable;
    process_table_t *pt = get_current_process_entry();

    if (check_filehandle(filehandle, pt) == VFS_NOT_OPEN
            && filehandle != FILEHANDLE_STDIN) {
        return VFS_NOT_FOUND;
    }
    /*read from file in blocks and write to vm */
    int count = 0;
    int to_be_read;
    int readed;
    while (count < length) {
        to_be_read = MIN(length-count, CONFIG_SYSCALL_MAX_BUFFER_SIZE);

        if (filehandle == FILEHANDLE_STDIN) {
            device_t *dev = device_get(YAMS_TYPECODE_TTY, 0);
            gcd_t *gcd = (gcd_t *) dev->generic_device;
            readed = gcd->read(gcd, temp, to_be_read);
        }

        else {
            readed = vfs_read(filehandle, temp, to_be_read);

            /*check for exceptions */
            if (readed < 0) {
                return VFS_ERROR;
            }

        }

        if (write_data_to_vm(p, temp, buffer + count,
                readed)==RETVAL_SYSCALL_HELPERS_NOK) {
            kprintf("Process was killed\n");
            syscall_handle_exit(1);
            return -1;
        }

        count = count + readed;

        if (readed < to_be_read) {
            break;
        }
    }
    return count;
}

int syscall_handle_write(openfile_t filehandle, const void *buffer, int length) {
    /* illegal filehandle */
    if (filehandle < 0)
        return FILEHANDLE_ILLEGAL;
    thread_table_t *t = thread_get_current_thread_entry();
    pagetable_t *p = t->pagetable;
    process_table_t *pt = get_current_process_entry();
    char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    int count = 0;
    int readed;
    int written;

    if (check_filehandle(filehandle, pt) == VFS_NOT_OPEN
            && filehandle != FILEHANDLE_STDOUT) {
        return VFS_NOT_FOUND;
    }

    while (count < length) {
        readed = MIN(length-count, CONFIG_SYSCALL_MAX_BUFFER_SIZE);
        if (read_data_from_vm(p, buffer + count, temp,
                readed)==RETVAL_SYSCALL_HELPERS_NOK) {
            kprintf("Process was killed\n");
            syscall_handle_exit(1);
            return -1;
        }
        if (filehandle == FILEHANDLE_STDOUT) {
            device_t *dev = device_get(YAMS_TYPECODE_TTY, 0);
            gcd_t *gcd = (gcd_t *) dev->generic_device;
            gcd->write(gcd, temp, readed);
        } else {
            written = vfs_write(filehandle, temp, readed);

            /*check if "file is full" as files have fixed sizes */
            if (written < readed) {
                count = count + written;
                break;
            }
        }
        count = count + readed;
    }

    return count;
}

int syscall_handle_create(const char *filename, int size) {
    thread_table_t *t = thread_get_current_thread_entry();
    pagetable_t *p = t->pagetable;
    char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    if (read_string_from_vm(p, filename, temp,
            VFS_PATH_LENGTH)==RETVAL_SYSCALL_HELPERS_NOK) {
        kprintf("Process was killed\n");
        syscall_handle_exit(0);
        return -1;
    }
    if (strlen(filename) > VFS_PATH_LENGTH) {
        return VFS_INVALID_PARAMS;
    }
    return vfs_create(temp, size);
}

int syscall_handle_delete(const char *filename) {
    thread_table_t *t = thread_get_current_thread_entry();
    pagetable_t *p = t->pagetable;
    char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
    if (read_string_from_vm(p, filename, temp,
            VFS_PATH_LENGTH)==RETVAL_SYSCALL_HELPERS_NOK) {
        kprintf("Process was killed\n");
        syscall_handle_exit(0);
        return -1;
    }
    if (strlen(filename) > VFS_PATH_LENGTH) {
        return VFS_INVALID_PARAMS;
    }

    return vfs_remove(temp);
}

#endif /* CHANGED_2 */

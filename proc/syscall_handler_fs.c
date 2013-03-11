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

openfile_t syscall_handle_open(const char *filename) {
	int filehandle = vfs_open((char*) filename);
	process_table_t *pt = get_current_process_entry();
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (pt->filehandle[i] == FILEHANDLE_UNUSED) {
			pt->filehandle[i] = filehandle;
			break;
		}
	}
	return filehandle;
}

int syscall_handle_close(openfile_t filehandle) {
	int i;
	int filehandle_found = -1;
	process_table_t *pt = get_current_process_entry();
	/*try to find the filehandle and close it */
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (pt->filehandle[i] == filehandle) {
			vfs_close(filehandle);
			filehandle_found = 1;
			break;
		}
	}
	if (filehandle_found == -1) {
		return VFS_NOT_OPEN;
	}
	return 0;
}

int syscall_handle_seek(openfile_t filehandle, int offset) {
	/*TODO EXCEPTION HANDLING*/
	vfs_seek(filehandle, offset);

	return 0;
}

int syscall_handle_read(openfile_t filehandle, void *buffer, int length) {
	int i;
	int filehandle_found = -1;
	char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];
	thread_table_t *t = thread_get_current_thread_entry();
	pagetable_t *p = t->pagetable;
	process_table_t *pt = get_current_process_entry();

	/*check that the process has the given filehandle */
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (pt->filehandle[i] == filehandle) {
			filehandle_found = 1;
			break;
		}
	}

	if (filehandle_found == -1 && filehandle != FILEHANDLE_STDIN) {
		return VFS_NOT_OPEN;
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
				return readed;
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
	thread_table_t *t = thread_get_current_thread_entry();
	pagetable_t *p = t->pagetable;
	char temp[CONFIG_SYSCALL_MAX_BUFFER_SIZE];

	int count = 0;
	int readed;

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
		}
		else{
			//kprintf("readed before: %d", readed);
			readed = vfs_write(filehandle,temp,readed);
			//kprintf("readed after: %d", readed);

		}
		count = count + readed;
	}

	return count;
}

int syscall_handle_create(const char *filename, int size) {
	// TODO: implementation
	size = size;
	size = *((const int*) filename);
	return 0;
}

int syscall_handle_delete(const char *filename) {
	// TODO: implementation
	return *((const int*) filename);;
}

#endif /* CHANGED_2 */

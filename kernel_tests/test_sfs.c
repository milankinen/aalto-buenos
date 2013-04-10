/*
 * sfs_tests.c
 *
 *  Created on: 10 Apr 2013
 *      Author: matti
 */

#ifdef CHANGED_3

#include "kernel_tests/test_filesystem.h"
#include "fs/vfs.h"
#include "fs/sfs.h"
#include "kernel/lock_cond.h"
#include "kernel/assert.h"
#include "kernel/thread.h"



static void test_create() {
    kprintf("Running SFS create tests..\n");

    kprintf("* Testing to create a valid directory '/foobar/'.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/", 0) == VFS_OK);
    kprintf("OK!\n* Testing to create '/foobar/' again, should fail.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/", 0) != VFS_OK);
    kprintf("OK!\n* Testing to create directory '/foobar/asd/'.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/asd/", 0) == VFS_OK);
    kprintf("OK!\n* Testing to create directory '/foobar/asd/' again, should fail.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/asd/", 0) != VFS_OK);
    kprintf("OK!\n* Testing to create '/asd/foobar/' again, should fail because '/asd/' doesn't exist.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/asd/foobar/", 0) != VFS_OK);

    kprintf("OK!\n* Testing to create file '/foobar/bar1', size 512.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/bar1", 512) == VFS_OK);
    kprintf("OK!\n* Testing to create file '/foobar/bar1' again, should fail.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/bar1", 512) != VFS_OK);
    kprintf("OK!\n* Testing to create file '/foobar/bar2', size 4.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/bar2", 4) == VFS_OK);
    kprintf("OK!\n* Testing to create file '/foobar/bar3', size %d, should fail.. ", SFS_MAX_FILESIZE + 1);
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/bar3", SFS_MAX_FILESIZE + 1) != VFS_OK);

    kprintf("OK!\nSFS create tests OK!\n");
}


static void test_delete() {
    kprintf("Running SFS directory tests..\n");

    kprintf("* Testing to delete '/', should fail.. ");
    KERNEL_ASSERT(vfs_remove("[disk1]/") != VFS_OK);
    kprintf("OK!\n* Testing to delete '/foobar/asd/'.. ");
    KERNEL_ASSERT(vfs_remove("[disk1]/foobar/asd/") == VFS_OK);
    kprintf("OK!\n* Testing to delete '/foobar/asd/' again, should fail.. ");
    KERNEL_ASSERT(vfs_remove("[disk1]/foobar/asd/") != VFS_OK);
    kprintf("OK!\n* Testing to re-create directory '/foobar/asd/'.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/asd/", 0) == VFS_OK);

    kprintf("OK!\n* Testing to delete '/foobar/bar2'.. ");
    KERNEL_ASSERT(vfs_remove("[disk1]/foobar/bar2") == VFS_OK);

    kprintf("OK!\nSFS delete tests OK!\n");
}


static void test_read_write() {
    char buffer[16];
    kprintf("Running SFS read/write tests..\n");

    kprintf("OK!\n* Testing to read/write from deleted file '/foobar/bar1'.. ");
    int handle = vfs_open("[disk1]/foobar/bar1");
    KERNEL_ASSERT(handle > 0);
    KERNEL_ASSERT(vfs_remove("[disk1]/foobar/bar1") == VFS_OK);
    kprintf("\n** deleted file.. ");
    KERNEL_ASSERT(vfs_open("[disk1]/foobar/bar1") <= 0);
    kprintf("\n** file opening not possible anymore!");
    KERNEL_ASSERT(vfs_write(handle, "foofoo", strlen("foofoo") + 1) == strlen("foofoo") + 1);
    kprintf("\n** wrote 'foofoo'..");
    vfs_seek(handle, 0);
    KERNEL_ASSERT(vfs_read(handle, buffer, strlen("foofoo") + 1) == strlen("foofoo") + 1);
    KERNEL_ASSERT(stringcmp("foofoo", buffer) == 0);
    kprintf("\n** managed to read '%s'", buffer);
    vfs_close(handle);
    kprintf("\n** file closed\n");
    KERNEL_ASSERT(vfs_open("[disk1]/foobar/bar1") <= 0);

    kprintf("OK!\nSFS read/write tests OK!\n");
}


void run_sfs_tests() {
    kprintf("Running SFS filesystem tests...\n=============================\n");
    test_create();
    test_delete();
    test_read_write();
    kprintf("=============================\nSFS filesystem tests OK!\n");
}



#endif /* CHANGED_3 */

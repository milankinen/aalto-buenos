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

static lock_t *sync_lock_main;
static cond_t *sync_cond_main;

static lock_t *sync_lock_child;
static cond_t *sync_cond_child;
static int children_ready;



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

    kprintf("OK!\n* Testing to create file '/foobar/as,;sad', should fail because using illegal characters.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/foobar/as,;sad", 512) != VFS_OK);
    kprintf("OK!\n* Testing to create file '/foobar/12345678901234567', should fail because too long filename.. ");
    KERNEL_ASSERT(vfs_create("[disk1]/foobar/foobar/12345678901234567", 512) != VFS_OK);
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
    char dirbuffer[512];
    kprintf("Running SFS read/write tests..\n");

    KERNEL_ASSERT(vfs_create("[disk1]/ww/", 1) == VFS_OK);
    KERNEL_ASSERT(vfs_create("[disk1]/ww/a", 1) == VFS_OK);
    KERNEL_ASSERT(vfs_create("[disk1]/ww/b", 1) == VFS_OK);
    KERNEL_ASSERT(vfs_create("[disk1]/ww/c/", 1) == VFS_OK);

    kprintf("* Testing to read directory '/ww/'.. ");
    int dir = vfs_open("[disk1]/ww/");
    KERNEL_ASSERT(dir > 0);
    // too small buffer
    KERNEL_ASSERT(vfs_read(dir, dirbuffer, 128) < 0);
    vfs_seek(dir, 0);
    KERNEL_ASSERT(vfs_read(dir, dirbuffer, SFS_VBLOCK_SIZE) > 0);
    // 3 files / sub-folders
    int i, offset = 0;
    KERNEL_ASSERT(*((uint32_t*)dirbuffer) == 3);
    for (i = 0 ; i < 3 ; i++) {
        char* str = dirbuffer + sizeof(uint32_t) + offset;
        KERNEL_ASSERT(stringcmp(str, "a") == 0 || stringcmp(str, "b") == 0 || stringcmp(str, "c/") == 0);
        offset += strlen(str) + 1;
    }

    kprintf("OK!\n* Testing to write to directory '/ww/', should not be possible.. ");
    // directories can't be modified directly with write
    KERNEL_ASSERT(vfs_write(dir, "asdasd", strlen("asdasd") + 1) < 0);

    kprintf("OK!\n* Testing to read/write file '/foobar/bar1'.. ");
    int handle = vfs_open("[disk1]/foobar/bar1");
    KERNEL_ASSERT(handle > 0);
    KERNEL_ASSERT(vfs_write(handle, "asdasdasd", strlen("asdasdasd") + 1) == strlen("asdasdasd") + 1);
    kprintf("\n** wrote 'asdasdasd'..");
    vfs_seek(handle, 0);
    KERNEL_ASSERT(vfs_read(handle, buffer, strlen("asdasdasd") + 1) == strlen("asdasdasd") + 1);
    KERNEL_ASSERT(stringcmp("asdasdasd", buffer) == 0);
    kprintf("\n** managed to read '%s'", buffer);
    vfs_close(handle);
    kprintf("\n** file closed\n");

    kprintf("OK!\n* Testing to read/write from deleted file '/foobar/bar1'.. ");
    handle = vfs_open("[disk1]/foobar/bar1");
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

#define SYNC_SMALL_BUF_SZ 16
#define SYNC_LARGE_BUF_SZ 2048

static void thread_read_small(uint32_t check){
    char buffer[SYNC_SMALL_BUF_SZ];
    openfile_t f = vfs_open("[disk1]/bigfile.txt");

    // wait until child threads are ready to read
    lock_acquire(sync_lock_main);
    children_ready++;
    condition_signal(sync_cond_main, sync_lock_main);
    lock_acquire(sync_lock_child);
    lock_release(sync_lock_main);

    // wait until main thread gives permission to start
    condition_wait(sync_cond_child, sync_lock_child);
    lock_release(sync_lock_child);

    // large read
    //kprintf("** small thread [%d] starting to read %d bytes\n", thread_get_current_thread(), SYNC_SMALL_BUF_SZ);
    KERNEL_ASSERT(vfs_read(f, buffer, SYNC_SMALL_BUF_SZ) == SYNC_SMALL_BUF_SZ);

    // test that we have finished our work last
    lock_acquire(sync_lock_main);
    //kprintf("** small thread [%d] finished reading\n", thread_get_current_thread());
    KERNEL_ASSERT((int)check == -1 || children_ready == (int)check);
    children_ready--;
    condition_signal(sync_cond_main, sync_lock_main);
    lock_release(sync_lock_main);
}


static void thread_read_large(uint32_t check){
    char buffer[SYNC_LARGE_BUF_SZ];
    openfile_t f = vfs_open("[disk1]/bigfile.txt");

    // wait until child threads are ready to read
    lock_acquire(sync_lock_main);
    children_ready++;
    condition_signal(sync_cond_main, sync_lock_main);
    lock_acquire(sync_lock_child);
    lock_release(sync_lock_main);

    // wait until main thread gives permission to start
    condition_wait(sync_cond_child, sync_lock_child);
    lock_release(sync_lock_child);

    // large read
    //kprintf("** large thread [%d] starting to read %d bytes\n", thread_get_current_thread(), SYNC_LARGE_BUF_SZ);
    KERNEL_ASSERT(vfs_read(f, buffer, SYNC_LARGE_BUF_SZ) == SYNC_LARGE_BUF_SZ);

    // test that we have finished our work last
    lock_acquire(sync_lock_main);
    //kprintf("** large thread [%d] finished reading\n", thread_get_current_thread());
    KERNEL_ASSERT((int)check == -1 || children_ready == (int)check);
    children_ready--;
    condition_signal(sync_cond_main, sync_lock_main);
    lock_release(sync_lock_main);
}


static void thread_write(uint32_t sz) {
    char buffer[SYNC_LARGE_BUF_SZ];
    openfile_t f = vfs_open("[disk1]/bigfile.txt");
    memoryset(buffer, 'a', sz-2);
    buffer[sz-1] = '\0';

    // wait until child threads are ready to read
    lock_acquire(sync_lock_main);
    children_ready++;
    condition_signal(sync_cond_main, sync_lock_main);
    lock_acquire(sync_lock_child);
    lock_release(sync_lock_main);

    // wait until main thread gives permission to start
    condition_wait(sync_cond_child, sync_lock_child);
    lock_release(sync_lock_child);

    // write
    //kprintf("** writer thread [%d] starting to write %d bytes\n", thread_get_current_thread(), sz-1);
    KERNEL_ASSERT(vfs_write(f, buffer, sz-1) == (int)sz-1);
    //kprintf("** writer thread [%d] finished writing\n", thread_get_current_thread());

    // test that we have finished our work last
    lock_acquire(sync_lock_main);
    children_ready--;
    condition_signal(sync_cond_main, sync_lock_main);
    lock_release(sync_lock_main);
}

static void test_basic_concurrent_read(){
    int i;
    TID_t tid, tid2;

    children_ready = 0;
    kprintf("Running SFS concurrent read/write tests..\n");
    for (i = 0 ; i < 4 ; i++) {
        /*tid2 should finish before tid*/
        tid = thread_create(&thread_read_large, 1);
        thread_run(tid);

        tid2 = thread_create(&thread_read_small, 2);
        thread_run(tid2);

        // wait until child threads are ready to read
        lock_acquire(sync_lock_main);
        while (children_ready < 2) condition_wait(sync_cond_main, sync_lock_main);
        lock_release(sync_lock_main);
        // broadcast them to start
        lock_acquire(sync_lock_child);
        condition_broadcast(sync_cond_child, sync_lock_child);
        lock_release(sync_lock_child);

        // wait until child threads are finished
        lock_acquire(sync_lock_main);
        while (children_ready > 0) condition_wait(sync_cond_main, sync_lock_main);
        lock_release(sync_lock_main);
    }
    kprintf("SFS concurrent read/write tests OK!\n");
}


#define CONCURRENT_READ_WRITE_THREADS 15
static void test_read_write_concurrency() {
    int i;
    uint32_t r;
    TID_t tid;

    children_ready = 0;
    kprintf("Running SFS concurrent read tests..\n");
    for (i = 0 ; i < CONCURRENT_READ_WRITE_THREADS ; i++) {
        r = _get_rand(3);
        if (r == 0) {
            tid = thread_create(&thread_write, 100 + _get_rand(SYNC_LARGE_BUF_SZ - 100));
        } else if (r == 1) {
            tid = thread_create(&thread_read_large, (uint32_t)-1);
        } else {
            tid = thread_create(&thread_read_small, (uint32_t)-1);
        }
        thread_run(tid);
    }


    // wait until child threads are ready to read/write
    lock_acquire(sync_lock_main);
    while (children_ready < CONCURRENT_READ_WRITE_THREADS) condition_wait(sync_cond_main, sync_lock_main);
    lock_release(sync_lock_main);
    // broadcast them to start
    lock_acquire(sync_lock_child);
    condition_broadcast(sync_cond_child, sync_lock_child);
    lock_release(sync_lock_child);

    // wait until child threads are finished
    lock_acquire(sync_lock_main);
    while (children_ready > 0) condition_wait(sync_cond_main, sync_lock_main);
    lock_release(sync_lock_main);

    kprintf("SFS concurrent read tests OK!\n");
}


void run_sfs_tests() {
    sync_cond_main = condition_create();
    sync_cond_child = condition_create();
    sync_lock_main = lock_create();
    sync_lock_child = lock_create();


    kprintf("Running SFS filesystem tests...\n=============================\n");
    test_create();
    test_delete();
    test_read_write();
    test_basic_concurrent_read();
    test_read_write_concurrency();

    kprintf("=============================\nSFS filesystem tests OK!\n");

    lock_destroy(sync_lock_child);
    lock_destroy(sync_lock_main);
    condition_destroy(sync_cond_child);
    condition_destroy(sync_cond_main);
}



#endif /* CHANGED_3 */

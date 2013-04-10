#ifdef CHANGED_3

/*
 * Student Filesystem (SFS).
 *
 * Copyright (C) 2013 Juha Matti, Lauri, Jaakko
 *
 */

#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "vm/pagepool.h"
#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "fs/sfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"



typedef struct {
    lock_t*     io_lock;
    cond_t*     io_cond;
    int         times_opened;
    int         readernum;
    TID_t       writer_tid;
    int         fileid;
    int         parentid;
    int         is_folder;
    int         is_deleted;
} sfs_openfile_t;


/* Data structure used internally by SFS filesystem. This data structure
   is used by sfs-functions. it is initialized during sfs_init(). Also
   memory for the buffers is reserved _dynamically_ during init.

   Buffers are used when reading/writing system or data blocks from/to
   disk.
*/
typedef struct {
    /* Total number of blocks of the disk */
    uint32_t       totalblocks;
    /* Total number of virtaul blocks of the disk */
    uint32_t       totalvblocks;

    /* Pointer to gbd device performing sfs */
    gbd_t          *disk;

    /* lock for mutual exclusion of fs-operations (we support only
       one operation at a time in any case) */
    semaphore_t    *lock;

    /* Buffers for read/write operations on disk. */
    sfs_inode_t    *buffer_inode;   /* buffer for inode blocks */
    bitmap_t       *buffer_bat;     /* buffer for allocation block */
    sfs_direntry_t *buffer_md;      /* buffer for directory block */

    /* opened files and their runtime information in SFS filesystem */
    sfs_openfile_t* open_files;
    /* lock for open_files sync */
    lock_t* openfile_lock;
} sfs_t;




static int read_vblock(gbd_t *disk, uint32_t block, uint32_t pbuf) {
    gbd_request_t req;
    int i;
    semaphore_t* sem;
    sem = semaphore_create(0);
    block *= SFS_BLOCKS_PER_VBLOCK;
    for (i = 0 ; i < SFS_BLOCKS_PER_VBLOCK ; i++) {
        req.block = block + i;
        req.sem = sem;
        req.buf = pbuf + (i * SFS_BLOCK_SIZE);
        disk->read_block(disk, &req);
        /* wait here until block has been readed */
        semaphore_P(sem);
        if (req.return_value != 0) {
            /* read failure */
            semaphore_destroy(sem);
            return 0;
        }
    }
    semaphore_destroy(sem);
    return req.return_value == 0;
}

static int write_vblock(gbd_t *disk, uint32_t block, uint32_t pbuf) {
    gbd_request_t req;
    int i;
    semaphore_t* sem;
    sem = semaphore_create(0);
    block *= SFS_BLOCKS_PER_VBLOCK;
    for (i = 0 ; i < SFS_BLOCKS_PER_VBLOCK ; i++) {
        req.block = block + i;
        req.sem = sem;
        req.buf = pbuf + (i * SFS_BLOCK_SIZE);
        disk->write_block(disk, &req);
        /* wait here until block has been writed */
        semaphore_P(sem);
        if (req.return_value != 0) {
            /* write failure */
            return 0;
        }
    }
    semaphore_destroy(sem);
    return req.return_value == 0;
}

static void sfs_reset_entry(sfs_openfile_t* entry) {
    // -1 indicates free
    entry->fileid   = -1;
    entry->parentid = -1;
    // reset sync counters
    entry->times_opened = 0;
    entry->readernum    = 0;
    entry->writer_tid   = -1;
    // reset flags
    entry->is_folder    = 0;
    entry->is_deleted   = 0;
}


static void _delete(sfs_t* sfs, int fileid, int parentid, int is_folder) {
    int r, i;

    // must lock sfs because we are using global buffers
    semaphore_P(sfs->lock);
    //kprintf("SFS: actual delete, %d\n", fileid);

    /* Read allocation block of the device and inode block of the file.
       Free reserved blocks (marked in inode) from allocation block. */
    r = read_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if (r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return;
    }
    bitmap_set(sfs->buffer_bat, fileid, 0);

    if (!is_folder) {
        // FOR DIRECTORIES: free only inode which contains directory files (so this branch does nothing)
        // FOR FILES: free also inodes containing the actual file contents
        r = read_vblock(sfs->disk, fileid, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_inode));
        if (r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return;
        }
        i = 0;
        while(i < (int)SFS_BLOCKS_MAX && sfs->buffer_inode->block[i] != 0) {
            bitmap_set(sfs->buffer_bat, sfs->buffer_inode->block[i], 0);
            i++;
        }
    } else {
        // if deleted inode is a directory then we must notify all open file entries
        // that their parent is destroyed so that they don't try to mark themselves as
        // destroyed to non-existing inode
        for (i = 0 ; i < SFS_MAX_OPEN_FILES ; i++) {
            if (sfs->open_files[i].parentid == fileid) {
                sfs->open_files[i].parentid = -1;
            }
        }
    }

    /* write updated allocation block to disk */
    r = write_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if (r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return;
    }

    // all ok
    parentid = parentid;
    semaphore_V(sfs->lock);
}


static void _delete_from_dirtree(sfs_t* sfs, int fileid, int parentid) {
    int r, i;
    semaphore_P(sfs->lock);
    /* Free entry from parent directory inode (if parent direcotry is not deleted before this operation) */
    if (parentid > 0) {
        //kprintf("SFS: delete from dirtree, %d\n", fileid);
        r = read_vblock(sfs->disk, parentid, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
        if (r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return;
        }
        // mark entries as unused so that new files can be created with same name
        // to the directory (virtually, file is deleted at this point!)
        for (i = 0 ; i < (int)SFS_MAX_FILES ; i++) {
            if ((int)sfs->buffer_md[i].inode == fileid) {
                sfs->buffer_md[i].inode     = 0;
                sfs->buffer_md[i].name[0]   = '\0';
            }
        }
        // update disk block for parent directory
        r = write_vblock(sfs->disk, parentid, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
        if(r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return;
        }
    }
    // all OK
    semaphore_V(sfs->lock);
}


static sfs_openfile_t* sfs_enter_ext(sfs_t* sfs, int fileid, int parentid, int is_open_op, int is_folder, int mark_as_deleted) {
    int i;
    sfs_openfile_t* f;
    if (fileid < 0) {
        return NULL;
    }
    //kprintf("enter: %d, opening %d, deleting %d\n", fileid, is_open_op, mark_as_deleted);
    lock_acquire(sfs->openfile_lock);
    f = NULL;
    for (i = 0 ; i < SFS_MAX_OPEN_FILES ; i++) {
        if (sfs->open_files[i].fileid == fileid) {
            f = sfs->open_files + i;
            break;
        } else if (sfs->open_files[i].fileid == -1 && f == NULL) {
            f = sfs->open_files + i;
        }
    }
    if (f == NULL) {
        // no free slot was found and file is not used atm, operation fails
        lock_release(sfs->openfile_lock);
        return NULL;
    }

    if (f->fileid == -1) {
        if (!is_open_op) {
            lock_release(sfs->openfile_lock);
            return NULL;
        }
        // we are opening new entry
        f->fileid = fileid;
        f->parentid = parentid;
        f->is_folder = is_folder;
    }

    if (is_open_op) {
        // file is being opened
        f->times_opened++;
    }
    if (mark_as_deleted) {
        if (!f->is_deleted) {
            // if we call delete first time, we must remove the file from directory tree
            // so that new files can be created with same name even though the contents
            // of the deleted file remain in the disk until all handles are cleared
            _delete_from_dirtree(sfs, f->fileid, f->parentid);
        }
        f->is_deleted = 1;
    }
    lock_release(sfs->openfile_lock);
    return f;
}

static sfs_openfile_t* sfs_enter(sfs_t* sfs, int fileid) {
    return sfs_enter_ext(sfs, fileid, -1, 0, 0, 0);
}

static void sfs_exit(sfs_t* sfs, sfs_openfile_t* entry, int is_close_op) {
    //kprintf("exit: %d, closing %d\n", entry->fileid, is_close_op);
    lock_acquire(sfs->openfile_lock);
    if (is_close_op) {
        entry->times_opened--;
    }
    if (entry->times_opened == 0) {
        if (entry->is_deleted) {
            _delete(sfs, entry->fileid, entry->parentid, entry->is_folder);
        }
        sfs_reset_entry(entry);
    }
    lock_release(sfs->openfile_lock);
}


static int sfs_is_valid_filename_char(char ch) {
    return  (ch == '_') ||
            (ch == '-') ||
            (ch == '.') ||
            (ch == '~') ||
            (ch == ' ') ||
            (ch >= 48 && ch <= 57) ||   // 0-9
            (ch >= 65 && ch <= 90) ||   // A-Z
            (ch >= 97 && ch <= 122);     // a-z
}

static int sfs_is_valid_path(const char* path) {
    int len, i, elemchars;
    if (path == NULL) {
        return 0;
    }
    len = strlen(path);
    if (len < 1 || path[0] != '/') {
        // path must always start with '/'
        return 0;
    }
    elemchars = 0;
    for (i = 1 ; i < len ; i++) {
        elemchars++;
        if (elemchars >= SFS_FILENAME_MAX) {
            // single path elem (e.g. /foo/barbarbarbarbarbar --> barbarbarbarbarbar) is too long
            return 0;
        }
        if (path[i] == '/') {
            if (path[i-1] == '/') {
                // '//' in path is illegal
                return 0;
            }
            // folders reset path elem character counter
            elemchars = 0;
            continue;
        } else if (sfs_is_valid_filename_char(path[i])) {
            continue;
        } else {
            // found invalid path character
            return 0;
        }
    }
    return 1;
}

static const char* sfs_get_actual_filename(const char* path) {
    int last = -1, i = 0;
    while(1) {
        if (path[i] == '/') {
            if (path[i+1] != '\0') {
                last = i;
            }
        } else if (path[i] == '\0') {
            break;
        }
        i++;
    }
    return path + last + 1;
}

static int sfs_is_folder(const char* path) {
    int len;
    if (path == NULL) {
        return 0;
    }
    len = strlen(path);
    // test that path ends with '/' --> is folder
    if (len >= 1 && path[len-1] == '/') {
       return 1;
    }
    return 0;
}

/* This assumes that path is validated with sfs_is_valid_path befora calling this!! */
static char* sfs_next_path_elem(char* consumed_path, char* buffer) {
    int i = 0;
    while(1) {
        buffer[i] = consumed_path[i];
        if (buffer[i] == '\0') {
            if (i == 0) {
                // no more path elements remaining
                return NULL;
            }
            return consumed_path + i;
        } else if (buffer[i] == '/') {
            buffer[i+1] = '\0';
            // end of folder element, start next time from next entry
            return consumed_path + i + 1;
        }
        i++;
    }
    return NULL;
}


static int sfs_resolve_file_ext(sfs_t* sfs, char* path, int* parent_inode, int* parent_exists) {
    uint32_t i;
    int r, block, ok;
    char* consumed_path;
    // buffer for path elements
    char filename[SFS_FILENAME_MAX];
    //kprintf("resolving %s\n", path);
    if (!sfs_is_valid_path(path)) {
        return VFS_ERROR;
    }
    // path is OK, we can start to consume it, +1 because we want to ignore starting '/' (root)
    consumed_path = path + 1;


    // always starting from root directory
    block = SFS_DIRECTORY_BLOCK;
    *parent_inode = -1;
    semaphore_P(sfs->lock);

    while ((consumed_path = sfs_next_path_elem(consumed_path, filename))) {
        r = read_vblock(sfs->disk, block, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
        *parent_inode = block;

        if(r == 0) {
            // an error occured during read
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }

        ok = 0;
        for(i=0; i < SFS_MAX_FILES; i++) {
            //kprintf("testing %s, %d (%s)\n", sfs->buffer_md[i].name, sfs->buffer_md[i].inode, filename);
            if(sfs->buffer_md[i].inode > 0 && stringcmp(sfs->buffer_md[i].name, filename) == 0) {
                // found next inode
                block = (int)sfs->buffer_md[i].inode;
                ok = 1;
                //kprintf("jmp\n");
                break;
            }
        }
        if (ok) {
            continue;
        }

        if (parent_exists != NULL) {
            // if we are checking the last path elem (=searched file) which is not found
            // then we can inform that its parent folder exists (even though the file doesn't
            // exist in its parent folder)
            *parent_exists = !sfs_next_path_elem(consumed_path, filename) ? 1 : 0;
        }
        // path element was not found, return error
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    // we have travelsed through directory tree and found our final file/folder
    semaphore_V(sfs->lock);
    return block;
}

static int sfs_resolve_file(sfs_t* sfs, char* path, int* parent_inode) {
    return sfs_resolve_file_ext(sfs, path, parent_inode, NULL);
}

/**
 * Initialize student filesystem. Allocates 1 page of memory dynamically for
 * filesystem data structure, sfs data structure and buffers needed.
 * Sets fs_t and sfs_t fields. If initialization is succesful, returns
 * pointer to fs_t data structure. Else NULL pointer is returned.
 *
 * @param Pointer to gbd-device performing sfs.
 *
 * @return Pointer to the filesystem data structure fs_t, if fails
 * return NULL.
 */
fs_t * sfs_init(gbd_t *disk)
{
    uint32_t addr;
    char name[SFS_VOLUMENAME_MAX];
    fs_t *fs;
    sfs_t *sfs;
    int r, i;
    semaphore_t *sem;
    lock_t* lock;

    if(disk->block_size(disk) != SFS_BLOCK_SIZE) {
        return NULL;
    }

    /* check semaphore availability before memory allocation */
    sem = semaphore_create(1);
    if (sem == NULL) {
        //kprintf("sfs_init: could not create a new semaphore.\n");
        return NULL;
    }

    lock = lock_create();
    if (lock == NULL) {
        semaphore_destroy(sem);
        //kprintf("sfs init: could not create a new lock.\n");
        return NULL;
    }

    addr = pagepool_get_phys_page();
    if (addr == 0) {
        semaphore_destroy(sem);
        lock_destroy(lock);
        //kprintf("sfs_init: could not allocate memory.\n");
        return NULL;
    }
    addr = ADDR_PHYS_TO_KERNEL(addr);      /* transform to vm address */


    /* Assert that one page is enough */
    KERNEL_ASSERT(PAGE_SIZE >= (3*SFS_VBLOCK_SIZE+sizeof(sfs_t)+sizeof(fs_t)) + (sizeof(openfile_t) * SFS_MAX_OPEN_FILES));

    /* Read header block, and make sure this is sfs drive */
    r = read_vblock(disk, 0, ADDR_KERNEL_TO_PHYS(addr));
    if (r == 0) {
        semaphore_destroy(sem);
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        //kprintf("sfs_init: Error during disk read. Initialization failed.\n");
        return NULL;
    }


    if(((uint32_t *)addr)[0] != SFS_MAGIC) {
        semaphore_destroy(sem);
        lock_destroy(lock);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        return NULL;
    }

    /* Copy volume name from header block. */
    stringcopy(name, (char *)(addr+4), SFS_VOLUMENAME_MAX);

    /* fs_t, sfs_t and all buffers in sfs_t fit in one page, so obtain
       addresses for each structure and buffer inside the allocated
       memory page. */
    fs  = (fs_t *)addr;
    sfs = (sfs_t *)(addr + sizeof(fs_t));
    sfs->buffer_inode = (sfs_inode_t *)((uint32_t)sfs + sizeof(sfs_t));
    sfs->buffer_bat  = (bitmap_t *)((uint32_t)sfs->buffer_inode +
                    SFS_VBLOCK_SIZE);
    sfs->buffer_md   = (sfs_direntry_t *)((uint32_t)sfs->buffer_bat +
                    SFS_VBLOCK_SIZE);

    /** allocate openfile data block */
    sfs->open_files = (sfs_openfile_t *)((uint32_t)sfs->buffer_md +
            SFS_VBLOCK_SIZE);

    sfs->totalblocks  = MIN(disk->total_blocks(disk), 8*SFS_VBLOCK_SIZE);
    sfs->totalvblocks = MIN(disk->total_blocks(disk) / SFS_BLOCKS_PER_VBLOCK, 8*SFS_VBLOCK_SIZE);
    ////kprintf("blocks: p=%d, v=%d, d=%d\n", 8*SFS_VBLOCK_SIZE, 8*SFS_VBLOCK_SIZE, disk->total_blocks(disk));
    sfs->disk         = disk;


    /* initialize openfile data block */
    for (i = 0 ; i < SFS_MAX_OPEN_FILES ; i++) {
        // allocate locks
        sfs->open_files[i].io_cond = condition_create();
        sfs->open_files[i].io_lock = lock_create();
        // reset other options
        sfs_reset_entry(sfs->open_files + i);
    }

    /* save the semaphore to the sfs_t */
    sfs->lock = sem;
    sfs->openfile_lock = lock;

    fs->internal = (void *)sfs;
    stringcopy(fs->volume_name, name, VFS_NAME_LENGTH);

    fs->unmount = sfs_unmount;
    fs->open    = sfs_open;
    fs->close   = sfs_close;
    fs->create  = sfs_create;
    fs->remove  = sfs_remove;
    fs->read    = sfs_read;
    fs->write   = sfs_write;
    fs->getfree  = sfs_getfree;

    return fs;
}


/**
 * Unmounts sfs filesystem from gbd device. After this TFS-driver and
 * gbd-device are no longer linked together. Implements
 * fs.unmount(). Waits for the current operation(s) to finish, frees
 * reserved memory and returns OK.
 *
 * @param fs Pointer to fs data structure of the device.
 *
 * @return VFS_OK
 */
int sfs_unmount(fs_t *fs)
{
    sfs_t *sfs;
    int i;

    sfs = (sfs_t *)fs->internal;

    // delete files that are marked as deleted
    for (i = 0 ; i < SFS_MAX_OPEN_FILES ; i++) {
        if (sfs->open_files[i].fileid > 0 && sfs->open_files[i].is_deleted) {
            _delete(sfs, sfs->open_files[i].fileid, sfs->open_files[i].parentid, sfs->open_files[i].is_folder);
        }
    }

    // destroy sfs openfile table lock
    lock_destroy(sfs->openfile_lock);

    semaphore_P(sfs->lock); /* The semaphore should be free at this
      point, we get it just in case something has gone wrong. */

    /* free semaphore and allocated memory */
    semaphore_destroy(sfs->lock);
    pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS((uint32_t)fs));
    return VFS_OK;
}


/**
 * Opens file. Implements fs.open(). Reads directory block of sfs
 * device and finds given file. Returns file's inode block number or
 * VFS_NOT_FOUND, if file not found.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param filename Name of the file to be opened.
 *
 * @return If file found, return inode block number as fileid, otherwise
 * return VFS_NOT_FOUND.
 */
int sfs_open(fs_t *fs, char *filename)
{
    sfs_t *sfs;
    sfs_openfile_t* entry;
    int inode, parent_inode;
    int is_deleted;

    sfs = (sfs_t *)fs->internal;

    //kprintf("open %s\n", filename);
    // test that file exists
    inode = sfs_resolve_file(sfs, filename, &parent_inode);
    if (inode == VFS_ERROR) {
        // not found
        return VFS_NOT_FOUND;
    }

    entry = sfs_enter_ext(sfs, inode, parent_inode, 1, sfs_is_folder(filename), 0);
    if (entry == NULL) {
        return VFS_ERROR;
    }
    is_deleted = entry->is_deleted;

    // exit sfs operation, if file is marked as deleted then we want to close
    // file entry immediately and return error to indicate that file is no longer
    // available
    sfs_exit(sfs, entry, is_deleted);
    //kprintf("open-end %s\n", filename);
    return is_deleted ? VFS_NOT_FOUND : inode;
}


/**
 * Closes file. Implements fs.close(). There is nothing to be done, no
 * data strucutures or similar are reserved for file. Returns VFS_OK.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param fileid File id (inode block number) of the file.
 *
 * @return VFS_OK
 */
int sfs_close(fs_t *fs, int fileid)
{
    //kprintf("close %d\n", fileid);
    sfs_t *sfs;
    sfs_openfile_t* entry;

    ////kprintf("qwewerrte\n");
    sfs = (sfs_t *)fs->internal;
    entry = sfs_enter(sfs, fileid);
    if (entry == NULL) {
        return VFS_ERROR;
    }
    // close entry (delete is done in sfs_exit if needed)
    sfs_exit(sfs, entry, 1);
    return VFS_OK;
}


/**
 * Creates file of given size. Implements fs.create(). Checks that
 * file name doesn't allready exist in directory block.Allocates
 * enough blocks from the allocation block for the file (1 for inode
 * and then enough for the file of given size). Reserved blocks are zeroed.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param filename File name of the file to be created
 * @param size Size of the file to be created
 *
 * @return If file allready exists or not enough space return VFS_ERROR,
 * otherwise return VFS_OK.
 */
int sfs_create(fs_t *fs, char *filename, int size) {

    if (!sfs_is_valid_path(filename)) {
        // invalid path
        return VFS_ERROR;
    }

    sfs_t *sfs = (sfs_t *)fs->internal;
    uint32_t i;
    int index = -1;
    int r, file_inode, parent_inode, parent_exists, is_folder;

    is_folder = sfs_is_folder(filename);
    if (is_folder) {
        // folders don't need addition inodes for data storing
        size = 0;
    } else {
        if (size < 0) {
            return VFS_ERROR;
        }
    }
    uint32_t numblocks = (size + SFS_BLOCK_SIZE - 1) / SFS_BLOCK_SIZE;
    uint32_t numvblocks = numblocks / SFS_BLOCKS_PER_VBLOCK + ((numblocks % SFS_BLOCKS_PER_VBLOCK) ? 1 : 0);


    /* check that we don't need more virtual blocks than our inode can contain */
    if (numvblocks > SFS_BLOCKS_MAX) {
        return VFS_ERROR;
    }

    file_inode = sfs_resolve_file_ext(sfs, filename, &parent_inode, &parent_exists);
    if (file_inode != VFS_ERROR || parent_inode < 0 || !parent_exists) {
        // file creation is not possible. either parent folder doesn't exist or file
        // already exists in the given path
        return VFS_ERROR;
    }

    semaphore_P(sfs->lock);
    /* Read parent directory block. Check that file doesn't allready exist and
       there is space left for the file in directory block. */
    r = read_vblock(sfs->disk, parent_inode, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if (r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    for (i=0 ; i < SFS_MAX_FILES ; i++) {
        if(sfs->buffer_md[i].inode == 0) {
            /* found free slot from directory */
            index = i;
        }
    }

    if(index == -1) {
        /* there was no space in directory, because index is not set */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    stringcopy(sfs->buffer_md[index].name, sfs_get_actual_filename(filename), SFS_FILENAME_MAX);

    /* Read allocation block and... */
    r = read_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if (r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }


    /* ...find space for inode... */
    sfs->buffer_md[index].inode = bitmap_findnset(sfs->buffer_bat, sfs->totalvblocks);
    if ((int)sfs->buffer_md[index].inode == -1) {
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    if (!is_folder) {
        /* ...and the rest of the blocks (only files!). Mark found block numbers in inode.*/
        sfs->buffer_inode->filesize = size;
        for (i = 0; i < numvblocks; i++) {
            sfs->buffer_inode->block[i] = bitmap_findnset(sfs->buffer_bat, sfs->totalvblocks);
            if ((int)sfs->buffer_inode->block[i] == -1) {
                /* Disk full. No free block found. */
                semaphore_V(sfs->lock);
                return VFS_ERROR;
            }
        }

        /* Mark rest of the virtual blocks in inode as unused. */
        while(i < SFS_BLOCKS_MAX) {
            sfs->buffer_inode->block[i++] = 0;
        }

    }

    /* Update allocation block so that used blocks (also inode!) are marked as occupied */
    r = write_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if (r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    /* update parent directory block in the disk */
    r = write_vblock(sfs->disk, parent_inode, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if (r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    if (!is_folder) {
        // FOR FILES: write file's inode data to the disk and then zero the blocks for file content

        r = write_vblock(sfs->disk, sfs->buffer_md[index].inode, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_inode));
        if (r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }

        /* Write zeros to the reserved blocks. Buffer for allocation block
           is no longer needed, so lets use it as zero buffer. */
        memoryset(sfs->buffer_bat, 0, SFS_VBLOCK_SIZE);
        for(i=0;i<numvblocks;i++) {
            r = write_vblock(sfs->disk, sfs->buffer_inode->block[i], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
            if (r == 0) {
                /* An error occured. */
                semaphore_V(sfs->lock);
                return VFS_ERROR;
            }

        }
    } else {
        // FOR DIRECTORIES: construct empty directory inode (no files in it) and write it to the disk
        file_inode = sfs->buffer_md[index].inode;
        memoryset(sfs->buffer_md, 0, sizeof(sfs_direntry_t) * SFS_MAX_FILES);
        r = write_vblock(sfs->disk, file_inode, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
        if (r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }
    }

    semaphore_V(sfs->lock);
    return VFS_OK;
}

/**
 * Removes given file. Implements fs.remove(). Frees blocks allocated
 * for the file and directory entry.
 *
 * @param fs Pointer to fs data structure of the device.
 * @param filename file to be removed.
 *
 * @return VFS_OK if file succesfully removed. If file not found
 * VFS_NOT_FOUND.
 */
int sfs_remove(fs_t *fs, char *filename)
{
    sfs_t *sfs = (sfs_t *)fs->internal;
    int file_inode, parent_inode;
    sfs_openfile_t* entry;

    file_inode = sfs_resolve_file(sfs, filename, &parent_inode);
    if (file_inode == VFS_ERROR || stringcmp(filename, "/") == 0) {
        // file was not found or path was incorrect or trying to delete root directory
        return VFS_NOT_FOUND;
    }

    // open file entry and mark it immediately as deleted
    entry = sfs_enter_ext(sfs, file_inode, parent_inode, 1, sfs_is_folder(filename), 1);
    if (entry == NULL) {
        // entry opening failed
        return VFS_ERROR;
    }
    // close it immediately, if entry is not opened elsewhere then the file is deleted
    sfs_exit(sfs, entry, 1);
    return VFS_OK;
}


static void sfs_end_read(sfs_t* sfs, sfs_openfile_t* entry) {
    lock_acquire(entry->io_lock);
    entry->readernum--;
    if (entry->readernum == 0) {
        // signal to the waiting writers that it's allowd to continue
        condition_broadcast(entry->io_cond, entry->io_lock);
    }
    lock_release(entry->io_lock);
    sfs_exit(sfs, entry, 0);
}

/**
 * Reads at most bufsize bytes from file to the buffer starting from
 * the offset. bufsize bytes is always read if possible. Returns
 * number of bytes read. Buffer size must be atleast bufsize.
 * Implements fs.read().
 *
 * @param fs  Pointer to fs data structure of the device.
 * @param fileid Fileid of the file.
 * @param buffer Pointer to the buffer the data is read into.
 * @param bufsize Maximum number of bytes to be read.
 * @param offset Start position of reading.
 *
 * @return Number of bytes read into buffer, or VFS_ERROR if error
 * occured.
 */
int sfs_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset)
{
    sfs_t *sfs = (sfs_t *)fs->internal;
    int b1, b2;
    int read=0;
    int r;
    sfs_openfile_t* entry;
    sfs_inode_t inode;
    char bat_t[SFS_VBLOCK_SIZE];
    char* bat = bat_t;

    //kprintf("asdsad %d\n", fileid);
    /* fileid is blocknum so ensure that we don't read system blocks or outside the disk */
    if(fileid < SFS_DIRECTORY_BLOCK || fileid > (int)sfs->totalvblocks) {
        return VFS_ERROR;
    }

    entry = sfs_enter(sfs, fileid);
    if (entry == NULL) {
        return VFS_ERROR;
    }

    // READ/WRITE sync
    lock_acquire(entry->io_lock);
    while(entry->writer_tid != -1) {
        condition_wait(entry->io_cond, entry->io_lock);
    }
    entry->readernum++;
    lock_release(entry->io_lock);

    // NOW IT'S OK TO READ!
    if (entry->is_folder) {
        // directory reading is a little bit different from normal files:
        // only file/folder names are read from inode so that memory structure is
        // <numfiles> <filename1> <filename2>...
        if (offset != 0 || bufsize < SFS_VBLOCK_SIZE) {
            sfs_end_read(sfs, entry);
            return VFS_ERROR;
        }
        r = read_vblock(sfs->disk, fileid, ADDR_KERNEL_TO_PHYS((uint32_t)bat));
        if (r == 0) {
            sfs_end_read(sfs, entry);
            return VFS_ERROR;
        }
        read = 0;
        b1 = 0; // numfiles
        for (r = 0 ; r < (int)SFS_MAX_FILES ; r++) {
            sfs_direntry_t* e = (((sfs_direntry_t*)bat) + r);
            if (e->inode > 0) {
                stringcopy(((char*)buffer) + sizeof(uint32_t) + read, e->name, SFS_FILENAME_MAX);
                b1++;
                read += strlen(e->name) + 1;
            }
        }
        // finally write number of files found
        *((uint32_t*)buffer) = b1;
        read += sizeof(uint32_t);
    } else {
        r = read_vblock(sfs->disk, fileid, ADDR_KERNEL_TO_PHYS((uint32_t)(&inode)));
        if (r == 0) {
            /* An error occured. */
            sfs_end_read(sfs, entry);
            return VFS_ERROR;
        }
        /* Check that offset is inside the file */
        if (offset < 0 || offset > (int)inode.filesize) {
            sfs_end_read(sfs, entry);
            return VFS_ERROR;
        }

        /* Read at most what is left from the file. */
        bufsize = MIN(bufsize,((int)inode.filesize) - offset);
        if (bufsize == 0) {
            sfs_end_read(sfs, entry);
            return 0;
        }


        /* first block to be read from the disk */
        b1 = offset / SFS_VBLOCK_SIZE;
        /* last block to be read from the disk */
        b2 = (offset+bufsize-1) / SFS_VBLOCK_SIZE;

        /* Read blocks from b1 to b2. First and last are special cases because
         * whole block might not be written to the buffer.
         */
        r = read_vblock(sfs->disk, inode.block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)bat));
        if(r == 0) {
            /* An error occured. */
            sfs_end_read(sfs, entry);
            return VFS_ERROR;
        }

        /* Count the number of the bytes to be read from the block and written to the buffer from the first block. */
        read = MIN(SFS_VBLOCK_SIZE - (offset % SFS_VBLOCK_SIZE), bufsize);
        memcopy(read, buffer, (const uint32_t *)(((uint32_t)bat) + (offset % SFS_VBLOCK_SIZE)));

        buffer = (void *)((uint32_t)buffer + read);
        b1++;
        while (b1 <= b2) {
            r = read_vblock(sfs->disk, inode.block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)bat));
            if (r == 0) {
                sfs_end_read(sfs, entry);
                return VFS_ERROR;
            }

            if(b1 == b2) {
                /* Last block. Whole block might not be read.*/
                memcopy(bufsize - read, buffer, (const uint32_t *)bat);
                read += (bufsize - read);
            } else {
                //kprintf("cxvcxv\n");
                /* Read whole block */
                memcopy(SFS_VBLOCK_SIZE, buffer, (const uint32_t *)bat);
                read += SFS_VBLOCK_SIZE;
                buffer = (void *)((uint32_t)buffer + SFS_VBLOCK_SIZE);
            }
            b1++;
        }
    }

    // END OF READ-PERMITTED SECTION
    sfs_end_read(sfs, entry);
//kprintf("ddsfgsd %d\n", fileid);
    return read;
}



static void sfs_end_write(sfs_t* sfs, sfs_openfile_t* entry) {
    lock_acquire(entry->io_lock);
    entry->writer_tid = -1;
    // signal to the waiting readers/writers that it's allowd to continue
    condition_broadcast(entry->io_cond, entry->io_lock);
    lock_release(entry->io_lock);
    sfs_exit(sfs, entry, 0);
}

/**
 * Write at most datasize bytes from buffer to the file starting from
 * the offset. datasize bytes is always written if possible. Returns
 * number of bytes written. Buffer size must be atleast datasize.
 * Implements fs.read().
 *
 * @param fs  Pointer to fs data structure of the device.
 * @param fileid Fileid of the file.
 * @param buffer Pointer to the buffer the data is written from.
 * @param datasize Maximum number of bytes to be written.
 * @param offset Start position of writing.
 *
 * @return Number of bytes written into buffer, or VFS_ERROR if error
 * occured.
 */
int sfs_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset)
{
    sfs_t *sfs = (sfs_t *)fs->internal;
    int b1, b2;
    int written=0;
    int r;
    sfs_openfile_t* entry;
    sfs_inode_t inode;
    char bat_t[SFS_VBLOCK_SIZE];
    char* bat = bat_t;


    /* fileid is blocknum so ensure that we don't read system blocks or outside the disk */
    if(fileid < SFS_DIRECTORY_BLOCK || fileid > (int)sfs->totalvblocks) {
        return VFS_ERROR;
    }

    entry = sfs_enter(sfs, fileid);
    if (entry == NULL || entry->is_folder) {
        // no entry found or entry is folder (not allowed to write into folders)
        return VFS_ERROR;
    }

    // READ/WRITE sync
    lock_acquire(entry->io_lock);
    while(entry->writer_tid != -1) {
        condition_wait(entry->io_cond, entry->io_lock);
    }
    entry->writer_tid = thread_get_current_thread();
    while(entry->readernum > 0) {
        condition_wait(entry->io_cond, entry->io_lock);
    }
    lock_release(entry->io_lock);


    // NOW IT'S OK TO WRITE!
    r = read_vblock(sfs->disk, fileid, ADDR_KERNEL_TO_PHYS((uint32_t)(&inode)));
    if (r == 0) {
        sfs_end_write(sfs, entry);
        return VFS_ERROR;
    }

    /* check that start position is inside the disk */
    if (offset < 0 || offset > (int)inode.filesize) {
        sfs_end_write(sfs, entry);
        return VFS_ERROR;
    }

    /* write at most the number of bytes left in the file */
    datasize = MIN(datasize, (int)inode.filesize - offset);

    if (datasize == 0) {
        sfs_end_write(sfs, entry);
        return 0;
    }

    /* first block to be written into */
    b1 = offset / SFS_VBLOCK_SIZE;
    /* last block to be written into */
    b2 = (offset+datasize-1) / SFS_VBLOCK_SIZE;


    /* Write data to blocks from b1 to b2. First and last are special
       cases because whole block might not be written. Because of possible
       partial write, first and last block must be read before writing.

       If we write less than block size or start writing in the middle
       of the block, read the block firts. Buffer for allocation block
       is used because it is not needed (for allocation block) in this
       function. */
    written = MIN(SFS_VBLOCK_SIZE - (offset % SFS_VBLOCK_SIZE), datasize);
    if (written < SFS_VBLOCK_SIZE) {
        r = read_vblock(sfs->disk, inode.block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)bat));
        if (r == 0) {
            sfs_end_write(sfs, entry);
            return VFS_ERROR;
        }
    }

    memcopy(written, (uint32_t *)(((uint32_t)bat) + (offset % SFS_VBLOCK_SIZE)), buffer);
    r = write_vblock(sfs->disk, inode.block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)bat));
    if (r == 0) {
        sfs_end_write(sfs, entry);
        return VFS_ERROR;
    }

    buffer = (void *)((uint32_t)buffer + written);
    b1++;
    while (b1 <= b2) {

        if (b1 == b2) {
            /* Last block. If partial write, read the block first.
               Write anyway always to the beginning of the block */
            if((datasize - written)  < SFS_VBLOCK_SIZE) {
                r = read_vblock(sfs->disk, inode.block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)bat));
                if (r == 0) {
                    sfs_end_write(sfs, entry);
                    return VFS_ERROR;
                }
            }

            memcopy(datasize - written, (uint32_t *)bat, buffer);
            written = datasize;
        }
        else {
            /* Write whole block */
            memcopy(SFS_VBLOCK_SIZE, (uint32_t *)bat, buffer);
            written += SFS_VBLOCK_SIZE;
            buffer = (void *)((uint32_t)buffer + SFS_VBLOCK_SIZE);
        }

        r = write_vblock(sfs->disk, inode.block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)bat));
        if (r == 0) {
            sfs_end_write(sfs, entry);
            return VFS_ERROR;
        }
        b1++;
    }

    // END OF READ-PERMITTED SECTION
    sfs_end_write(sfs, entry);
    return written;
}

/**
 * Get number of free bytes on the disk. Implements fs.getfree().
 * Reads allocation blocks and counts number of zeros in the bitmap.
 * Result is multiplied by the block size and returned.
 *
 * @param fs Pointer to the fs data structure of the device.
 *
 * @return Number of free bytes.
 */
int sfs_getfree(fs_t *fs)
{
    sfs_t *sfs = (sfs_t *)fs->internal;
    int allocated = 0;
    uint32_t i;
    int r;

    semaphore_P(sfs->lock);

    r = read_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    for(i=0; i < sfs->totalvblocks; i++) {
        allocated += bitmap_get(sfs->buffer_bat, i);
    }

    semaphore_V(sfs->lock);
    /** Add tail physical blocks */
    return (sfs->totalvblocks - allocated) * SFS_VBLOCK_SIZE + ((sfs->totalblocks % sfs->totalvblocks) * SFS_BLOCK_SIZE);
}


#endif /* CHANGED_3 */

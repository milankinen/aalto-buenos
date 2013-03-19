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
    int r;
    semaphore_t *sem;

    if(disk->block_size(disk) != SFS_BLOCK_SIZE) {
        return NULL;
    }

    /* check semaphore availability before memory allocation */
    sem = semaphore_create(1);
    if (sem == NULL) {
    kprintf("sfs_init: could not create a new semaphore.\n");
    return NULL;
    }

    addr = pagepool_get_phys_page();
    if(addr == 0) {
        semaphore_destroy(sem);
    kprintf("sfs_init: could not allocate memory.\n");
    return NULL;
    }
    addr = ADDR_PHYS_TO_KERNEL(addr);      /* transform to vm address */


    /* Assert that one page is enough */
    KERNEL_ASSERT(PAGE_SIZE >= (3*SFS_VBLOCK_SIZE+sizeof(sfs_t)+sizeof(fs_t)));

    /* Read header block, and make sure this is sfs drive */
    r = read_vblock(disk, 0, ADDR_KERNEL_TO_PHYS(addr));
    if (r == 0) {
        semaphore_destroy(sem);
        pagepool_free_phys_page(ADDR_KERNEL_TO_PHYS(addr));
        kprintf("sfs_init: Error during disk read. Initialization failed.\n");
        return NULL;
    }


    if(((uint32_t *)addr)[0] != SFS_MAGIC) {
        semaphore_destroy(sem);
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

    sfs->totalblocks  = MIN(disk->total_blocks(disk), 8*SFS_VBLOCK_SIZE);
    sfs->totalvblocks = MIN(disk->total_blocks(disk) / SFS_BLOCKS_PER_VBLOCK, 8*SFS_VBLOCK_SIZE);
    kprintf("blocks: p=%d, v=%d, d=%d\n", 8*SFS_VBLOCK_SIZE, 8*SFS_VBLOCK_SIZE, disk->total_blocks(disk));
    sfs->disk         = disk;

    /* save the semaphore to the sfs_t */
    sfs->lock = sem;

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

    sfs = (sfs_t *)fs->internal;

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
    uint32_t i;
    int r;

    sfs = (sfs_t *)fs->internal;

    semaphore_P(sfs->lock);

    r = read_vblock(sfs->disk, SFS_DIRECTORY_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if(r == 0) {
        /* An error occured during read. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    for(i=0;i < SFS_MAX_FILES;i++) {
        //kprintf("scanning file: %s (%d)\n", sfs->buffer_md[i].name, sfs->buffer_md[i].inode);
        if(stringcmp(sfs->buffer_md[i].name, filename) == 0) {
            semaphore_V(sfs->lock);
            //kprintf("found sfs file %d \n", sfs->buffer_md[i].inode);
            return sfs->buffer_md[i].inode;
        }
    }

    semaphore_V(sfs->lock);
    return VFS_NOT_FOUND;
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
    fs = fs;
    fileid = fileid;

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
int sfs_create(fs_t *fs, char *filename, int size)
{
    sfs_t *sfs = (sfs_t *)fs->internal;
    uint32_t i;
    uint32_t numblocks = (size + SFS_BLOCK_SIZE - 1) / SFS_BLOCK_SIZE;
    uint32_t numvblocks = numblocks / SFS_BLOCKS_PER_VBLOCK + ((numblocks % SFS_BLOCKS_PER_VBLOCK) ? 1 : 0);
    int index = -1;
    int r;

    semaphore_P(sfs->lock);

    /* check that we don't need more virtual blocks than our inode can contain */
    if(numvblocks > (SFS_VBLOCK_SIZE / 4 - 1)) {
    semaphore_V(sfs->lock);
    return VFS_ERROR;
    }

    /* Read directory block. Check that file doesn't allready exist and
       there is space left for the file in directory block. */
    r = read_vblock(sfs->disk, SFS_DIRECTORY_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    for(i=0;i<SFS_MAX_FILES;i++) {
    if(stringcmp(sfs->buffer_md[i].name, filename) == 0) {
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

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

    stringcopy(sfs->buffer_md[index].name,filename, SFS_FILENAME_MAX);

    /* Read allocation block and... */
    r = read_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r==0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }


    /* ...find space for inode... */
    sfs->buffer_md[index].inode = bitmap_findnset(sfs->buffer_bat,
                          sfs->totalvblocks);
    if((int)sfs->buffer_md[index].inode == -1) {
    semaphore_V(sfs->lock);
    return VFS_ERROR;
    }

    /* ...and the rest of the blocks. Mark found block numbers in
       inode.*/
    sfs->buffer_inode->filesize = size;
    for(i=0; i<numvblocks; i++) {
        sfs->buffer_inode->block[i] = bitmap_findnset(sfs->buffer_bat,
                                  sfs->totalvblocks);
        if((int)sfs->buffer_inode->block[i] == -1) {
            /* Disk full. No free block found. */
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }
    }

    /* Mark rest of the virtual blocks in inode as unused. */
    while(i < (SFS_VBLOCK_SIZE / 4 - 1)) {
        sfs->buffer_inode->block[i++] = 0;
    }


    r = write_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r==0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    r = write_vblock(sfs->disk, SFS_DIRECTORY_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if(r==0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    r = write_vblock(sfs->disk, sfs->buffer_md[index].inode, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_inode));
    if(r==0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    /* Write zeros to the reserved blocks. Buffer for allocation block
       is no longer needed, so lets use it as zero buffer. */
    memoryset(sfs->buffer_bat, 0, SFS_VBLOCK_SIZE);
    for(i=0;i<numvblocks;i++) {
        r = write_vblock(sfs->disk, sfs->buffer_inode->block[i], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
        if(r==0) {
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
    uint32_t i;
    int index = -1;
    int r;

    semaphore_P(sfs->lock);

    /* Find file and inode block number from directory block.
       If not found return VFS_NOT_FOUND. */
    r = read_vblock(sfs->disk, SFS_DIRECTORY_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    for(i=0;i<SFS_MAX_FILES;i++) {
        if(stringcmp(sfs->buffer_md[i].name, filename) == 0) {
            index = i;
            break;
        }
    }
    if(index == -1) {
        semaphore_V(sfs->lock);
        return VFS_NOT_FOUND;
    }

    /* Read allocation block of the device and inode block of the file.
       Free reserved blocks (marked in inode) from allocation block. */
    r = read_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    r = read_vblock(sfs->disk, sfs->buffer_md[index].inode, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_inode));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }


    bitmap_set(sfs->buffer_bat,sfs->buffer_md[index].inode,0);
    i = 0;
    while(sfs->buffer_inode->block[i] != 0 && i < (SFS_VBLOCK_SIZE / 4 - 1)) {
        bitmap_set(sfs->buffer_bat, sfs->buffer_inode->block[i], 0);
        i++;
    }

    /* Free directory entry. */
    sfs->buffer_md[index].inode   = 0;
    sfs->buffer_md[index].name[0] = 0;

    r = write_vblock(sfs->disk, SFS_ALLOCATION_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    r = write_vblock(sfs->disk, SFS_DIRECTORY_BLOCK, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_md));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    semaphore_V(sfs->lock);
    return VFS_OK;
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

    semaphore_P(sfs->lock);

    /* fileid is blocknum so ensure that we don't read system blocks
       or outside the disk */
    if(fileid < 2 || fileid > (int)sfs->totalvblocks) {
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }
    //kprintf("read file id=%d, sz=%d, offset=%d, tot=%d\n", fileid, bufsize, offset, sfs->totalvblocks);

    r = read_vblock(sfs->disk, fileid, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_inode));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    //kprintf("file size: %d\n", sfs->buffer_inode->filesize);

    /* Check that offset is inside the file */
    if(offset < 0 || offset > (int)sfs->buffer_inode->filesize) {
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    /* Read at most what is left from the file. */
    bufsize = MIN(bufsize,((int)sfs->buffer_inode->filesize) - offset);

    if(bufsize==0) {
        semaphore_V(sfs->lock);
        return 0;
    }

    /* first block to be read from the disk */
    b1 = offset / SFS_VBLOCK_SIZE;

    /* last block to be read from the disk */
    b2 = (offset+bufsize-1) / SFS_VBLOCK_SIZE;

    /* Read blocks from b1 to b2. First and last are
       special cases because whole block might not be written
       to the buffer. */
    r = read_vblock(sfs->disk, sfs->buffer_inode->block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    /* Count the number of the bytes to be read from the block and
       written to the buffer from the first block. */
    read = MIN(SFS_VBLOCK_SIZE - (offset % SFS_VBLOCK_SIZE), bufsize);
    memcopy(read, buffer,
        (const uint32_t *)(((uint32_t)sfs->buffer_bat) +
                   (offset % SFS_VBLOCK_SIZE)));

    buffer = (void *)((uint32_t)buffer + read);
    b1++;
    while(b1 <= b2) {
        r = read_vblock(sfs->disk, sfs->buffer_inode->block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
        if(r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }

        if(b1 == b2) {
            /* Last block. Whole block might not be read.*/
            memcopy(bufsize - read,
                buffer,
                (const uint32_t *)sfs->buffer_bat);
            read += (bufsize - read);
        }
        else {
            /* Read whole block */
            memcopy(SFS_VBLOCK_SIZE,
                buffer,
                (const uint32_t *)sfs->buffer_bat);
            read += SFS_VBLOCK_SIZE;
            buffer = (void *)((uint32_t)buffer + SFS_VBLOCK_SIZE);
        }
        b1++;
    }

    semaphore_V(sfs->lock);
    return read;
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

    semaphore_P(sfs->lock);

    /* fileid is blocknum so ensure that we don't read system blocks
       or outside the disk */
    if(fileid < 2 || fileid > (int)sfs->totalvblocks) {
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    r = read_vblock(sfs->disk, fileid, ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_inode));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    /* check that start position is inside the disk */
    if(offset < 0 || offset > (int)sfs->buffer_inode->filesize) {
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    /* write at most the number of bytes left in the file */
    datasize = MIN(datasize, (int)sfs->buffer_inode->filesize-offset);

    if(datasize==0) {
        semaphore_V(sfs->lock);
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
    if(written < SFS_VBLOCK_SIZE) {
        r = read_vblock(sfs->disk, sfs->buffer_inode->block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
        if(r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }
    }

    memcopy(written, (uint32_t *)(((uint32_t)sfs->buffer_bat) + (offset % SFS_VBLOCK_SIZE)), buffer);

    r = write_vblock(sfs->disk, sfs->buffer_inode->block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
    if(r == 0) {
        /* An error occured. */
        semaphore_V(sfs->lock);
        return VFS_ERROR;
    }

    buffer = (void *)((uint32_t)buffer + written);
    b1++;
    while(b1 <= b2) {

        if(b1 == b2) {
            /* Last block. If partial write, read the block first.
               Write anyway always to the beginning of the block */
            if((datasize - written)  < SFS_VBLOCK_SIZE) {
                r = read_vblock(sfs->disk, sfs->buffer_inode->block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
                if(r == 0) {
                    /* An error occured. */
                    semaphore_V(sfs->lock);
                    return VFS_ERROR;
                }
            }

            memcopy(datasize - written, (uint32_t *)sfs->buffer_bat, buffer);
            written = datasize;
        }
        else {
            /* Write whole block */
            memcopy(SFS_VBLOCK_SIZE, (uint32_t *)sfs->buffer_bat, buffer);
            written += SFS_VBLOCK_SIZE;
            buffer = (void *)((uint32_t)buffer + SFS_VBLOCK_SIZE);
        }

        r = write_vblock(sfs->disk, sfs->buffer_inode->block[b1], ADDR_KERNEL_TO_PHYS((uint32_t)sfs->buffer_bat));
        if(r == 0) {
            /* An error occured. */
            semaphore_V(sfs->lock);
            return VFS_ERROR;
        }

        b1++;
    }

    semaphore_V(sfs->lock);
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

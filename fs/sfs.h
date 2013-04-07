/*
 * sfs.h
 *
 *  Created on: 19 Mar 2013
 *      Author: matti
 */

#ifndef SFS_H_
#define SFS_H_
#ifdef CHANGED_3



#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "lib/libc.h"
#include "lib/bitmap.h"
#include "kernel/lock_cond.h"

/* In SFS block size is 128. This will affect to various other
   features of SFS e.g. maximum file size. */
#define SFS_BLOCK_SIZE 512

#define SFS_BLOCKS_PER_VBLOCK 1

#define SFS_VBLOCK_SIZE (SFS_BLOCK_SIZE*SFS_BLOCKS_PER_VBLOCK)

/* Magic number found on each tfs/sfs filesystem's header block. */
#define SFS_MAGIC 3745

/* Block numbers for system blocks */
#define SFS_HEADER_BLOCK 0
#define SFS_ALLOCATION_BLOCK 1
#define SFS_DIRECTORY_BLOCK  2

/* Names are limited to 16 characters */
#define SFS_VOLUMENAME_MAX 16
#define SFS_FILENAME_MAX 16

#define SFS_MAX_OPEN_FILES 32

/*
   Maximum number of block pointers in one inode. Block pointers
   are of type uint32_t and one pointer "slot" is reserved for
   file size.
*/
#define SFS_BLOCKS_MAX ((SFS_VBLOCK_SIZE/sizeof(uint32_t))-1)

/* Maximum file size. 512-byte Inode can store 127 blocks for a file.
   512*127=65024 */
#define SFS_MAX_FILESIZE (SFS_VBLOCK_SIZE*SFS_BLOCKS_MAX)

/* File inode block. Inode contains the filesize and a table of blocknumbers
   allocated for the file. In TFS files can't have more blocks than fits in
   block table of the inode block.

   One 512 byte block can hold 128 32-bit integers. Therefore the table
   size is limited to 127 and filesize to 127*512=65024.
*/

typedef struct {
    /* filesize in bytes */
    uint32_t filesize;

    /* block numbers allocated for this file, zero
       means unused block. */
    uint32_t block[SFS_BLOCKS_MAX];
} sfs_inode_t;


/* Master directory block entry. If inode is zero, entry is
   unused (free). */
typedef struct {
    /* File's inode block number. */
    uint32_t inode;

    /* File name */
    char     name[SFS_FILENAME_MAX];
} sfs_direntry_t;



#define SFS_MAX_FILES (SFS_VBLOCK_SIZE/sizeof(sfs_direntry_t))

/* functions */
fs_t * sfs_init(gbd_t *disk);

int sfs_unmount(fs_t *fs);
int sfs_open(fs_t *fs, char *filename);
int sfs_close(fs_t *fs, int fileid);
int sfs_create(fs_t *fs, char *filename, int size);
int sfs_remove(fs_t *fs, char *filename);
int sfs_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset);
int sfs_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset);
int sfs_getfree(fs_t *fs);



#endif
#endif /* SFS_H_ */

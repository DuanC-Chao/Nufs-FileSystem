#ifndef STORAGE_H
#define STORAGE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fuse.h>
#include <errno.h>
#include <stdlib.h>

#define BLOCK_SIZE 4096
#define MAX_FILES 128
#define MAX_NAME 256
#define TOTAL_BLOCKS 1024

#define SUPER_BLOCK_START 0
#define INODES_START 1
#define DATA_START 18

// Size:
// 4 + 256 + 4 + 4 + 4 + 256 + 4 = 532 bytes
// There are 128 files
// total: 68136 bytes
// 68136 / 4096 = 17 blocks
typedef struct
{
    int is_used;
    char name[MAX_NAME];
    int size;
    int block;
    int ref_count;
    char parent[MAX_NAME];
    mode_t mode;
} inode_t;

// Size: 4 + 4 + 128 = 136 bytes
// Takes the first block
typedef struct
{
    int total_blocks;                    // The total availiable block number
    int free_blocks;                     // The free block number
    char block_bitmap[TOTAL_BLOCKS / 8]; // The bit map, contains total_block / 8 bytes, each has 8 bit, could represent all blocks
} superblock_t;

void write_inodes_to_disk();
void storage_init(const char *path);
int storage_create(const char *path, mode_t mode);
int storage_delete(const char *path);
int storage_rename(const char *from, const char *to);
int storage_read(const char *path, char *buf, size_t size, off_t offset);
int storage_write(const char *path, const char *buf, size_t size, off_t offset);
int storage_stat(const char *path, struct stat *st);
int storage_chmod(const char *path, mode_t mode);
int storage_unlink(const char *path);
int storage_truncate(const char *path, off_t size);
int storage_lookup(const char *path);
void storage_list(const char *path, void *buf, fuse_fill_dir_t filler);
int storage_is_dir_empty(const char *path);

#endif // STORAGE_H

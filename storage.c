#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

static superblock_t sb; // The super block
static inode_t inodes[MAX_FILES];
static char disk_filename[MAX_NAME]; // The name of the file that stores disk img information

// Helper method, takes a path, a string named parent, a string name fname
// Analyze the given path, and extract current path's parent dir name, and the path itself's name
// If the path is at the root (/), set parent as "~"
static void get_parent_and_name(const char *path, char *parent, char *fname)
{
    // Find the last slash in the path
    const char *slash = strrchr(path, '/');

    // Handle root path "/"
    if (slash == path)
    {
        if (parent != NULL)
        {
            strcpy(parent, "~");
        }
        if (fname != NULL)
        {
            if (strlen(path) > 1)
            {
                strcpy(fname, path + 1);
            }
            else
            {
                strcpy(fname, ".");
            }
        }
        return;
    }

    // For non-root paths
    if (parent != NULL)
    {
        if (slash)
        {
            int plen = slash - path;
            strncpy(parent, path, plen);
            parent[plen] = '\0';
        }
        else
        {
            strcpy(parent, "~");
        }
    }

    if (fname != NULL)
    {
        if (slash)
        {
            strcpy(fname, slash + 1);
        }
        else
        {
            strcpy(fname, path);
        }
    }
}

// Takes a block, which is the index of the block, logically
// And a value, which could rather be 1 or 0, means to set this block used or free
static void set_bitmap(int block, int value)
{

    // Find the index of the byte in the block map
    int byte_index = block / 8;

    // Find the index of the bit in the block map
    int bit_index = block % 8;

    // Set as 0 or 1, as value defined
    // By bit operations
    if (value)
    {
        sb.block_bitmap[byte_index] |= (1 << bit_index);
    }
    else
    {
        sb.block_bitmap[byte_index] &= ~(1 << bit_index);
    }
}

// Take an index of a block, and check in bit map, if the block is used
// If so, return 1, else, return 0
static int is_block_used(int block)
{
    int byte_index = block / 8;
    int bit_index = block % 8;
    return sb.block_bitmap[byte_index] & (1 << bit_index);
}

// Try to find an unused block from the bit map
// If found any, return the index of that block
// If not, return -1 (block index start with 0)
static int allocate_block()
{
    for (int i = 0; i < sb.total_blocks; i++)
    {
        if (!is_block_used(i))
        {
            set_bitmap(i, 1);
            sb.free_blocks--;
            return i;
        }
    }
    return -1;
}

// Takes a block index, and set the block as unused in bitmap.
static void free_block(int block)
{
    if (is_block_used(block))
    {
        set_bitmap(block, 0);
        sb.free_blocks++;
    }
}

// Write the super block structure and inodes array into disk img file
// Should be called each time after inode array are updated
void write_inodes_to_disk()
{

    FILE *fp = fopen(disk_filename, "r+");
    if (!fp)
    {
        printf("Can not open disk image");
        return;
    }

    fseek(fp, 0, SEEK_SET);
    fwrite(&sb, sizeof(superblock_t), 1, fp);

    fseek(fp, BLOCK_SIZE, SEEK_SET);
    fwrite(inodes, sizeof(inodes), 1, fp);

    fclose(fp);
}

// Initialize the storage
// Initialize the super block, inodes array, and mounting
void storage_init(const char *path)
{
    printf("BreakPoint#629\n");
    snprintf(disk_filename, MAX_NAME, "%s", path);

    // Try to open the existing mount path
    FILE *fp = fopen(disk_filename, "r+");
    if (!fp)
    {
        // If the given path do not exist, make one
        fp = fopen(disk_filename, "w+");
        if (!fp)
        {
            perror("Failed creating disk image");
            return;
        }

        // First, set total_blocks and free_blocks to be total block's number
        sb.total_blocks = TOTAL_BLOCKS;
        sb.free_blocks = TOTAL_BLOCKS - 18;
        memset(sb.block_bitmap, 0, sizeof(sb.block_bitmap));

        // Set the first 18 blocks as used
        // (Root takes a block, and will be initialized in this method)
        for (int i = 0; i < 18; i++)
        {
            set_bitmap(i, 1);
        }

        char *root = malloc(sizeof(char) * MAX_NAME);
        root[0] = '/';
        root[1] = '\0';
        storage_create(root, S_IFDIR | 0777);

        printf("BreakPoint#630\n");

        fseek(fp, 0, SEEK_SET);
        fwrite(&sb, sizeof(superblock_t), 1, fp);
        fseek(fp, BLOCK_SIZE, SEEK_SET);
        fwrite(inodes, sizeof(inodes), 1, fp);

        fclose(fp);
    }
    else
    {
        fseek(fp, 0, SEEK_SET);
        fread(&sb, sizeof(superblock_t), 1, fp);
        fseek(fp, BLOCK_SIZE, SEEK_SET);
        fread(inodes, sizeof(inodes), 1, fp);

        fclose(fp);
    }
    printf("BreakPoint#631\n");
}

// Takes a path, and check if the path already exist in mounted file system
// If so, return as -EEXIST, if not, create one
int storage_create(const char *path, mode_t mode)
{

    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);
    printf("BreakPoint#MAKING path is: %s fname is: %s, pname is: %s\n", path, fname, parent_name);

    // If given path is empty, throw error -EINVAL
    if (strlen(path) == 0)
    {
        printf("file path is empty\n");
        return -EINVAL;
    }

    // Check if already exist same name file
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used &&
            strcmp(inodes[i].name, fname) == 0
            // && strcmp(inodes[i].parent, parent_name) == 0
        )
        {
            printf("file of given path already exists\n");
            return -EEXIST;
        }
    }

    // Find avaliable inode and create file
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (!inodes[i].is_used)
        {
            inodes[i].is_used = 1;
            strncpy(inodes[i].name, fname, MAX_NAME - 1);
            inodes[i].name[MAX_NAME - 1] = '\0';

            strncpy(inodes[i].parent, parent_name, MAX_NAME - 1);
            inodes[i].parent[MAX_NAME - 1] = '\0';

            inodes[i].size = 0;
            inodes[i].ref_count = 1;
            inodes[i].mode = mode;
            inodes[i].block = allocate_block();

            // Async meta data between RAM and disk img file
            write_inodes_to_disk();
            printf("successfully created path: %s with inode index: %d \n", path, i);
            return 0;
        }
        else
        {
            printf("inode with index: %d already used\n", i);
        }
    }

    // If no avaliable inode to utilize, return -ENOSPC
    printf("No space left on disk.\n");
    return -ENOSPC;
}

int storage_delete(const char *path)
{

    printf("BreakPoint#456\n");

    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            inodes[i].is_used = 0;
            free_block(inodes[i].block);
            write_inodes_to_disk();
            return 0;
        }
    }
    return -ENOENT;
}

// Change the inode of given path "from"'s content to "to"'s information
int storage_rename(const char *from, const char *to)
{
    char from_parent[MAX_NAME];
    char from_name[MAX_NAME];
    get_parent_and_name(from, from_parent, from_name);

    char to_parent[MAX_NAME];
    char to_name[MAX_NAME];
    get_parent_and_name(to, to_parent, to_name);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, from_name) == 0 && strcmp(inodes[i].parent, from_parent) == 0)
        {
            strcpy(inodes[i].name, to_name);
            strcpy(inodes[i].parent, to_parent);
            write_inodes_to_disk();
            return 0;
        }
    }
    return -ENOENT;
}

// Takes a path to read, a buffer to store content read
// And a size, which to read size bytes from path, and the offset
int storage_read(const char *path, char *buf, size_t size, off_t offset)
{
    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            if (S_ISDIR(inodes[i].mode))
            {
                return -EISDIR;
            }
            if (offset >= inodes[i].size)
            {
                return 0;
            }

            size_t to_read;

            if (offset + size > inodes[i].size)
            {
                to_read = inodes[i].size - offset;
            }
            else
            {
                to_read = size;
            }

            FILE *fp = fopen(disk_filename, "r");
            if (!fp)
            {
                printf("Can't open disk image READ\n");
                return -EIO;
            }
            fseek(fp, DATA_START * BLOCK_SIZE + inodes[i].block * BLOCK_SIZE + offset, SEEK_SET);
            size_t n = fread(buf, 1, to_read, fp);
            fclose(fp);
            return n;
        }
    }
    return -ENOENT;
}

// Takes a path, a buffer, a size, a offsset
//  Write size byte of content from buffer to the file of the path
//  Start from the offset byte
int storage_write(const char *path, const char *buf, size_t size, off_t offset)
{
    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            if (S_ISDIR(inodes[i].mode))
            {
                printf("Can not write to a directory \n");
                return -EISDIR;
            }
            if (offset + size > BLOCK_SIZE)
            {
                return -EFBIG; // Not supporting big file (bigger than 4096) for now
            }
            if (inodes[i].block < DATA_START)
            {
                int block = allocate_block();
                if (block == -1)
                {
                    return -ENOSPC;
                }
                inodes[i].block = block;
            }

            FILE *fp = fopen(disk_filename, "r+");
            if (!fp)
            {
                printf("Can't open disk image WRITE\n");
                return -EIO;
            }

            fseek(fp, DATA_START * BLOCK_SIZE + inodes[i].block * BLOCK_SIZE + offset, SEEK_SET);
            size_t n = fwrite(buf, 1, size, fp);
            fclose(fp);

            if (offset + size > inodes[i].size)
            {
                inodes[i].size = offset + size;
            }
            write_inodes_to_disk();
            return n;
        }
    }
    return -ENOENT;
}

int storage_stat(const char *path, struct stat *st)
{

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used)
        {
            // printf("FROM LS: inode with index %d, has pname: %s, and fname: %s\n", inodes[i].block, inodes[i].parent, inodes[i].name);
        }
    }

    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);
    // printf("storage_stat: path=%s, parent=%s, name=%s\n", path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            memset(st, 0, sizeof(struct stat));
            st->st_uid = getuid();
            st->st_gid = getgid();
            st->st_mode = inodes[i].mode;
            st->st_size = inodes[i].size;
            // printf("storage_stat: found inode, mode=%o, size=%d\n", inodes[i].mode, inodes[i].size);
            return 0;
        }
    }
    return -ENOENT;
}

int storage_chmod(const char *path, mode_t mode)
{
    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            inodes[i].mode = mode;
            write_inodes_to_disk();
            return 0;
        }
    }
    return -ENOENT;
}

int storage_unlink(const char *path)
{
    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            inodes[i].is_used = 0;
            return 0;
        }
    }
    return -ENOENT;
}

int storage_truncate(const char *path, off_t size)
{
    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            if (S_ISDIR(inodes[i].mode))
            {
                return -EISDIR;
            }
            if (size > BLOCK_SIZE)
            {
                return -EFBIG;
            }
            inodes[i].size = size;
            if (size == 0)
            {
                free_block(inodes[i].block);
                inodes[i].block = 0;
            }
            write_inodes_to_disk();
            return 0;
        }
    }
    return -ENOENT;
}

// Check if the given path exists, if so, return the inode index of that path
// If not, return -1
int storage_lookup(const char *path)
{

    printf("BreakPoint#457\n");

    char parent_name[MAX_NAME];
    char fname[MAX_NAME];
    get_parent_and_name(path, parent_name, fname);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].name, fname) == 0 && strcmp(inodes[i].parent, parent_name) == 0)
        {
            return i;
        }
    }

    return -1;
}

// Takes a path, and add name of every file within it to the buffer
void storage_list(const char *path, void *buf, fuse_fill_dir_t filler)
{

    // printf("BreakPoint#458\n");

    char to_find_parent_name[MAX_NAME];

    if (strcmp(path, "/") == 0)
    {
        // printf("BreakPoint#6700 \n");
        to_find_parent_name[0] = '~';
        to_find_parent_name[1] = '\0';
    }
    else
    {
        strcpy(to_find_parent_name, path);
    }
    // printf("Listing: path: %s\n", path);
    printf("Target Pname: %s\n", to_find_parent_name);

    char self_parent_name[MAX_NAME];
    char self_name[MAX_NAME];
    get_parent_and_name(path, self_parent_name, self_name);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used)
        {
            printf("Found pname: %s\n", inodes[i].parent);
            if (strcmp(inodes[i].name, self_name) == 0 && strcmp(inodes[i].parent, self_parent_name) == 0)
            {
                if (S_ISREG(inodes[i].mode))
                {
                    printf("Can not list a regular file \n");
                    return;
                }
            }
        }
    }

    for (int i = 0; i < MAX_FILES; i++)
    {
        // printf("BreakPoint#330 \n");
        if (inodes[i].is_used)
        {
            // printf("BreakPoint#9980 Current Iterating inode parent: %s, to_find_parent_name: %s\n", inodes[i].parent, to_find_parent_name);
            if (strcmp(inodes[i].parent, to_find_parent_name) == 0)
            {
                if (filler(buf, inodes[i].name, NULL, 0) != 0)
                {
                    printf("BreakPoint#3780 \n");
                    return;
                }
                printf("%s ", inodes[i].name);
            }
        }
    }
}

// Takes a path of a firectory, and check if that dir is empty
// If not, return 0, if so, return 1
int storage_is_dir_empty(const char *path)
{

    printf("BreakPoint#CKDIREMTCD\n");

    char pname[MAX_NAME];

    strcpy(pname, path);

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (inodes[i].is_used && strcmp(inodes[i].parent, pname) == 0)
        {
            printf("BreakPoint#NORM\n");
            return 0;
        }
    }
    printf("BreakPoint#YESRM\n");
    return 1;
}

// based on cs3650 starter code

#include <assert.h>
#include <bsd/string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "storage.h"

int nufs_access(const char *path, int mask)
{
  if (storage_lookup(path) >= 0)
  {
    return 0;
  }
  return -ENOENT;
}

int nufs_getattr(const char *path, struct stat *st)
{
  // printf("BreakPoint#0 \n");
  // memset(st, 0, sizeof(struct stat));
  int rv = storage_stat(path, st);
  if (rv < 0)
  {
    return -ENOENT;
  }
  return 0;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi)
{
  /*
  //printf("BreakPoint#1, buffer_d: %d, buffer_s: %p \n", buf, buf);
  if (filler(buf, ".", NULL, 0) != 0)
  {
    //printf("BreakPoint#3778, buffer: %d off: %i \n", buf, off);
    return 0;
  }
  if (filler(buf, "..", NULL, 0) != 0)
  {
    //printf("BreakPoint#3779, buffer: %d off: %i \n", buf, off);
    return 0;
  }
   */

  storage_list(path, buf, filler);
  return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
int nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
  return storage_create(path, mode);
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode)
{
  mode |= S_IFDIR;
  return storage_create(path, mode);
}

int nufs_unlink(const char *path)
{
  return storage_unlink(path);
}

int nufs_link(const char *from, const char *to)
{
  return 0;
}

int nufs_rmdir(const char *path)
{
  if (!storage_is_dir_empty(path))
  {
    return -ENOTEMPTY;
  }
  return storage_delete(path);
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to)
{
  return storage_rename(from, to);
}

int nufs_chmod(const char *path, mode_t mode)
{
  return storage_chmod(path, mode);
}

int nufs_truncate(const char *path, off_t size)
{
  return storage_truncate(path, size);
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char *path, struct fuse_file_info *fi)
{
  if (storage_lookup(path) >= 0)
  {
    return 0;
  }
  return -ENOENT;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi)
{
  return storage_read(path, buf, size, offset);
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi)
{
  return storage_write(path, buf, size, offset);
}

// Not implemented
int nufs_utimens(const char *path, const struct timespec ts[2])
{
  return 0;
}

// Extended operations
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data)
{
  return 0;
}

void nufs_init_ops(struct fuse_operations *ops)
{
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  printf("BreakPoint#2 \n");
  ops->mknod = nufs_mknod;
  // ops->create   = nufs_create; // alternative to mknod
  ops->mkdir = nufs_mkdir;
  ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->utimens = nufs_utimens;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->ioctl = nufs_ioctl;
};

struct fuse_operations nufs_ops;

int main(int argc, char *argv[])
{

  char *fuse_argv[4];
  int fuse_argc = 4;

  fuse_argv[0] = argv[0]; // ./nufs
  fuse_argv[1] = argv[1]; // -s
  fuse_argv[2] = argv[2]; // -f
  fuse_argv[3] = argv[3]; // mnt

  char *diskfile = argv[4]; // data.nufs

  printf("Mounting %s as data file\n", diskfile);
  storage_init(diskfile);

  nufs_init_ops(&nufs_ops);
  return fuse_main(fuse_argc, fuse_argv, &nufs_ops, NULL);
}

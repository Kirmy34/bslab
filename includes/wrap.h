//
//  wrap.h
//  myfs
//
//  Created by Oliver Waldhorst on 02.08.17.
//  Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

// This file is based on an example from https://code.google.com/archive/p/fuse-examplefs/

#ifndef wrap_h
#define wrap_h

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

#ifdef __cplusplus
extern "C" {
#endif
    void setInstance(int onDisk);

    int wrap_getattr(const char *path, struct stat *statbuf);
    int wrap_readlink(const char *path, char *link, size_t size);
    int wrap_mknod(const char *path, mode_t mode, dev_t dev);
    int wrap_mkdir(const char *path, mode_t mode);
    int wrap_unlink(const char *path);
    int wrap_rmdir(const char *path);
    int wrap_symlink(const char *path, const char *link);
    int wrap_rename(const char *path, const char *newpath);
    int wrap_link(const char *path, const char *newpath);
    int wrap_chmod(const char *path, mode_t mode);
    int wrap_chown(const char *path, uid_t uid, gid_t gid);
    int wrap_truncate(const char *path, off_t newSize);
    int wrap_utime(const char *path, struct utimbuf *ubuf);
    int wrap_open(const char *path, struct fuse_file_info *fileInfo);
    int wrap_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
    int wrap_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo);
    int wrap_statfs(const char *path, struct statvfs *statInfo);
    int wrap_flush(const char *path, struct fuse_file_info *fileInfo);
    int wrap_release(const char *path, struct fuse_file_info *fileInfo);
    int wrap_fsync(const char *path, int datasync, struct fuse_file_info *fi);
#ifdef __APPLE__
    int wrap_setxattr(const char *path, const char *name, const char *value, size_t size, int flags, uint32_t x);
    int wrap_getxattr(const char *path, const char *name, char *value, size_t size, uint x);
#else
    int wrap_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
    int wrap_getxattr(const char *path, const char *name, char *value, size_t size);
#endif
    void* wrap_init(struct fuse_conn_info *conn);
    int wrap_listxattr(const char *path, char *list, size_t size);
    int wrap_removexattr(const char *path, const char *name);
    int wrap_opendir(const char *path, struct fuse_file_info *fileInfo);
    int wrap_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo);
    int wrap_releasedir(const char *path, struct fuse_file_info *fileInfo);
    int wrap_fsyncdir(const char *path, int datasync, struct fuse_file_info *fileInfo);
    int wrap_ftruncate(const char *path, off_t offset, struct fuse_file_info *fileInfo);
    int wrap_create(const char *, mode_t, struct fuse_file_info *);
    void wrap_destroy(void *userdata);
    
#ifdef __cplusplus
}
#endif

#endif /* wrap_h */

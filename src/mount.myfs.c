//
//  mount.myfs.c
//  myfs
//
//  Created by Oliver Waldhorst on 02.08.17.
//  Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

// DO NOT EDIT THIS FILE!!!

#include "wrap.h"

#include <fuse.h>
#include <stdio.h>
#include <stddef.h>

#include "myfs-info.h"

#define PACKAGE_VERSION "v0.2"

struct fuse_operations myfs_oper;

struct myfs_config {
    char *containerFileName;
    char *logFileName;
};
enum {
    KEY_HELP,
    KEY_VERSION,
};

#define MYFS_OPT(t, p, v) { t, offsetof(struct myfs_config, p), v }

static struct fuse_opt myfs_opts[] = {
        MYFS_OPT("-c %s",             containerFileName, 0),
        MYFS_OPT("containerfile=%s",  containerFileName, 0),
        MYFS_OPT("-l %s",             logFileName, 0),
        MYFS_OPT("logfile=%s",        logFileName, 0),

        FUSE_OPT_KEY("-V",             KEY_VERSION),
        FUSE_OPT_KEY("--version",      KEY_VERSION),
        FUSE_OPT_KEY("-h",             KEY_HELP),
        FUSE_OPT_KEY("--help",         KEY_HELP),
        FUSE_OPT_END
};

static int myfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    switch (key) {
        case KEY_HELP:
            fuse_opt_add_arg(outargs, "-h");
            fuse_main(outargs->argc, outargs->argv, &myfs_oper, NULL);
            fprintf(stderr,
                    "\n"
                    "Myfs options:\n"
                    "    -o containerfile=FILE\n"
                    "    -c FILE            same as '-o containerfile=FILE'\n"
                    "    -o logfile=FILE\n"
                    "    -l FILE            same as '-o logfile=FILE'\n");
            exit(1);

        case KEY_VERSION:
            fprintf(stderr, "MyFS version %s\n", PACKAGE_VERSION);
            fuse_opt_add_arg(outargs, "--version");
            fuse_main(outargs->argc, outargs->argv, &myfs_oper, NULL);
            exit(0);
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int fuse_stat;

    myfs_oper.getattr = wrap_getattr;
    myfs_oper.readlink = wrap_readlink;
    myfs_oper.getdir = NULL;
    myfs_oper.mknod = wrap_mknod;
    myfs_oper.mkdir = wrap_mkdir;
    myfs_oper.unlink = wrap_unlink;
    myfs_oper.rmdir = wrap_rmdir;
    myfs_oper.symlink = wrap_symlink;
    myfs_oper.rename = wrap_rename;
    myfs_oper.link = wrap_link;
    myfs_oper.chmod = wrap_chmod;
    myfs_oper.chown = wrap_chown;
    myfs_oper.truncate = wrap_truncate;
    myfs_oper.utime = wrap_utime;
    myfs_oper.open = wrap_open;
    myfs_oper.read = wrap_read;
    myfs_oper.write = wrap_write;
    myfs_oper.statfs = wrap_statfs;
    myfs_oper.flush = wrap_flush;
    myfs_oper.release = wrap_release;
    myfs_oper.fsync = wrap_fsync;
    myfs_oper.setxattr = wrap_setxattr;
    myfs_oper.getxattr = wrap_getxattr;
    myfs_oper.listxattr = wrap_listxattr;
    myfs_oper.removexattr = wrap_removexattr;
    myfs_oper.opendir = wrap_opendir;
    myfs_oper.readdir = wrap_readdir;
    myfs_oper.releasedir = wrap_releasedir;
    myfs_oper.fsyncdir = wrap_fsyncdir;
    myfs_oper.init = wrap_init;
    myfs_oper.ftruncate = wrap_ftruncate;
    myfs_oper.destroy = wrap_destroy;

    char* containerFileName= NULL;
    char* logFileName= NULL;

    // parse arguments
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct myfs_config conf;

    memset(&conf, 0, sizeof(conf));

    fuse_opt_parse(&args, &conf, myfs_opts, myfs_opt_proc);

    // FsInfo will be used to pass information to fuse functions
    struct MyFsInfo *FsInfo;
    FsInfo= malloc(sizeof(struct MyFsInfo));
    // check if container file is accessible
    if(conf.containerFileName != NULL) {
        containerFileName= realpath(conf.containerFileName, NULL);

        if(containerFileName == NULL) {
            // container file does not exist, check if path is writable
            char *containerFileNameCpy= malloc(strlen(conf.containerFileName)+1);
            strcpy(containerFileNameCpy, conf.containerFileName);
            char *dirName= dirname(containerFileNameCpy);
            char *containerPathName= realpath(dirName, NULL);
            // free(dirName);
            if (containerPathName == NULL || access(containerPathName, R_OK | W_OK) != 0 ) {
                fprintf(stderr, "Error: Cannot access container directory %s\n", containerPathName == NULL ? "" : containerPathName);
                exit(EXIT_FAILURE);
            }
            containerFileName= (char *) malloc(PATH_MAX);
            strcpy(containerFileNameCpy, conf.containerFileName);
            char *containerBaseName= basename(containerFileNameCpy);
            strcpy(containerFileName, containerPathName);
            strcat(containerFileName, "/");
            strcat(containerFileName, containerBaseName);
            // free(containerBaseName);
            free(containerPathName);
            free(containerFileNameCpy);
        } else {
            // container file does exit, check if it is writable
            if (containerFileName == NULL || access(containerFileName, R_OK | W_OK) != 0 ) {
                fprintf(stderr, "Error: Cannot access container file %s\n", containerFileName);
                exit(EXIT_FAILURE);
            }
        }

        // container file is used, so we are not in memory!
        setInstance(1);
    } else {
        setInstance(0);
    }

    // check if logfile can be accessed
    if(conf.logFileName != NULL) {
        FILE *logFile = fopen(conf.logFileName, "w+");

        if (logFile == NULL || (logFileName = realpath(conf.logFileName, NULL)) == NULL) {
            fprintf(stderr, "Error: Cannot access log file %s\n", conf.logFileName);
            exit(EXIT_FAILURE);
        }

        fclose(logFile);
    } else {
        fprintf(stderr, "Error: No log file given (use -l)\n");
        exit(EXIT_FAILURE);
    }

    // everything ok, lets go
    // container & log file name will be passed to fuse functions
    FsInfo->contFile= containerFileName;
    FsInfo->logFile= logFileName;

    // add additoinal "-s"
    fuse_opt_add_arg(&args, "-s");

    // call fuse initialization method
    fuse_stat = fuse_main(args.argc, args.argv, &myfs_oper, FsInfo);

    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

    // cleanup
    free(FsInfo);
    free(containerFileName);
    free(logFileName);

    return fuse_stat;
}

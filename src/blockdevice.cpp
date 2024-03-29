//
//  blockdevice.cpp
//  myfs
//
//  Created by Oliver Waldhorst on 09.10.17.
//  Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

// DO NOT EDIT THIS FILE!!!

#include <cstdlib>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "macros.h"

#include "blockdevice.h"

#undef DEBUG

BlockDevice::BlockDevice(uint32_t blockSize) {
    assert(blockSize % 512 == 0);
    this->blockSize= blockSize;
}

int BlockDevice::create(const char *path) {

    int ret= 0;

    // Open Container file
    contFile = ::open(path, O_EXCL | O_RDWR | O_CREAT, 0666);
    if (contFile < 0) {
        if (errno == EEXIST) {
            // file already exists, we must open & truncate
            LOG("WARNING: container file already exists, truncating")
            contFile = ::open(path, O_EXCL | O_RDWR | O_TRUNC);
        }

        if(contFile < 0) {
            LOG("ERROR: unable to create container file");
            ret= -errno;
        }
    }
    
//    this->size= 0;
    
    return ret;
}

int BlockDevice::open(const char *path) {

    int ret= 0;

    // Open Container file
    contFile = ::open(path, O_EXCL | O_RDWR);
    if (contFile < 0) {
        if (errno == ENOENT)
            LOG("ERROR: container file does not exists");
        else
            LOGF("ERROR: unknown error %d", errno);

        ret= -errno;

    }

    return ret;
}


int BlockDevice::close() {

    int ret= 0;

    if(::close(this->contFile) < 0)
        ret= -errno;
    
    return ret;
}

// this method returns 0 if successful, -errno otherwise
int BlockDevice::read(uint32_t blockNo, char *buffer) {
#ifdef DEBUG
    fprintf(stderr, "BlockDevice: Reading block %d\n", blockNo);
#endif
    off_t pos = (off_t) blockNo * this->blockSize;
    if (lseek (this->contFile, pos, SEEK_SET) != pos)
        return -errno;
    
    int size = (this->blockSize);
    if (::read (this->contFile, buffer, size) != size)
        return -errno;

    return 0;
}

// this method returns 0 if successful, -errno otherwise
int BlockDevice::write(uint32_t blockNo, char *buffer) {
#ifdef DEBUG
    fprintf(stderr, "BlockDevice: Writing block %d\n", blockNo);
#endif
    off_t pos = (off_t) blockNo * this->blockSize;
    if (lseek (this->contFile, pos, SEEK_SET) != pos)
        return -errno;

    int __size = (this->blockSize);
    if (::write (this->contFile, buffer, __size) != __size)
        return -errno;

    return 0;
}


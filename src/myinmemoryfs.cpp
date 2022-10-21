//
// Created by Oliver Waldhorst on 20.03.20.
//  Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myinmemoryfs.h"

// The functions fuseGettattr(), fuseRead(), and fuseReadDir() are taken from
// an example by Mohammed Q. Hussain. Here are original copyrights & licence:

/**
 * Simple & Stupid Filesystem.
 *
 * Mohammed Q. Hussain - http://www.maastaar.net
 *
 * This is an example of using FUSE to build a simple filesystem. It is a part of a tutorial in MQH Blog with the title
 * "Writing a Simple Filesystem Using FUSE in C":
 * http://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
 *
 * License: GNU GPL
 */

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

// TODO: Comment lines to reduce debug messages
#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#define NAME_LENGTH 255
#define BLOCK_SIZE 512
#define NUM_DIR_ENTRIES 64
#define NUM_OPEN_FILES 64

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "blockdevice.h"

/// @brief Constructor of the in-memory file system class.
///
/// You may add your own constructor code here.
MyInMemoryFS::MyInMemoryFS() : MyFS() {

}

/// @brief Destructor of the in-memory file system class.
///
/// You may add your own destructor code here.
MyInMemoryFS::~MyInMemoryFS() {

    files.clear();
    files.resize(0);

}

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseMknod(const char *path, mode_t mode, dev_t dev) {
    LOGM();

    myFsFile file{
            std::string(path + 1),
            0,
            0,
            mode,
            0,
            0,
            0,
            nullptr,
            0,
            0
    };

    files.push_back(file);

    RETURN(0);
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseUnlink(const char *path) {
    LOGM();

    auto p = std::string(path + 1);
    for (auto i = files.begin(); i != files.end(); i++) {
        if (i->name == p) {
            // release allocated memory
            if (i->data) {
                free(i->data);
            }
            files.erase(i);
            RETURN(0);
        }
    }

    RETURN(-ENOENT);
}

/// @brief Rename a file.
///
/// Rename the file with with a given name to a new name.
/// Note that if a file with the new name already exists it is replaced (i.e., removed
/// before renaming the file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newpath  New name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseRename(const char *path, const char *newpath) {
    LOGM();

    //search if the new path already exists
    myFsFile *file;
    int ret = findFile(newpath, &file);
    if (ret == 0) { /*TODO: REMOVE FILE*/; } //remove file if already existing

    if (ret) {
        RETURN(ret);
    } else {
        file->name = std::string(newpath + 1);
    }

    RETURN(0);
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseGetattr(const char *path, struct stat *statbuf) {
    LOGM();

    if (strcmp(path, "/") == 0) {
        statbuf->st_mode = S_IFDIR | 0755;
        statbuf->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
        RETURN(0);
    }

    myFsFile *file;
    int ret = findFile(path, &file);
    // If file not found return ERRNO
    if (ret) { RETURN(ret); }

    statbuf->st_uid = file->userId;
    statbuf->st_gid = file->groupId;
    statbuf->st_atime = file->accessTime;
    statbuf->st_mtime = file->modTime;
    statbuf->st_mode = file->mode;
    statbuf->st_nlink = 1; // weil wir eine Datei sind und kein Verzeichnis
    statbuf->st_size = file->size;

    RETURN(0);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseChmod(const char *path, mode_t mode) {
    LOGM();

    myFsFile *file;
    int ret = findFile(path, &file);
    if (ret) { RETURN(ret); } // If file not found return ERRNO

    // Change mode of file
    file->mode = mode;
    RETURN(0);
}

/// @brief Change the owner of a file.
///
/// Change the user and group identifier in the meta data of a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] uid New user id.
/// \param [in] gid New group id.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseChown(const char *path, uid_t uid, gid_t gid) {
    LOGM();

    myFsFile *file;
    int ret = findFile(path, &file);
    if (ret) { RETURN(ret); } // If file not found return ERRNO

    // Change user- and groupId
    file->userId = uid;
    file->groupId = gid;

    RETURN(0);
}

/// @brief Open a file.
///
/// Open a file for reading or writing. This includes checking the permissions of the current user and incrementing the
/// open file count.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] fileInfo Can be ignored in Part 1
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();

    myFsFile *file;
    int ret = findFile(path, &file);
    if (ret) {
        RETURN(-ENOENT);
    }

    RETURN(0);
}

/// @brief Read from a file.
///
/// Read a given number of bytes from a file starting form a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] buf The data read from the file is stored in this array. You can assume that the size of buffer is at
/// least 'size'
/// \param [in] size Number of bytes to read
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file
/// \param [in] fileInfo Can be ignored in Part 1
/// \return The Number of bytes read on success. This may be less than size if the file does not contain sufficient bytes.
/// -ERRNO on failure.
int MyInMemoryFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    // Find file
    myFsFile *file;
    int ret = findFile(path, &file);
    if (ret) {
        // file not found
        RETURN(-ENOENT);
    }

    // Check if offset in range
    if (offset > file->size) {
        RETURN(-EINVAL)
    }

    // Check if we can read the whole request or just until end of file
    off_t size2read = std::min(file->size - offset, (off_t) size);

    // Fill buffer with read data
    memcpy(buf, file->data + offset, size2read);

    // Return nr of Bytes read.
    RETURN(size2read)
}

/// @brief Write to a file.
///
/// Write a given number of bytes to a file starting at a given position.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// Note that the file content is an array of bytes, not a string. I.e., it is not (!) necessarily terminated by '\0'
/// and may contain an arbitrary number of '\0'at any position. Thus, you should not use strlen(), strcpy(), strcmp(),
/// ... on both the file content and buf, but explicitly store the length of the file and all buffers somewhere and use
/// memcpy(), memcmp(), ... to process the content.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] buf An array containing the bytes that should be written.
/// \param [in] size Number of bytes to write.
/// \param [in] offset Starting position in the file, i.e., number of the first byte to read relative to the first byte of
/// the file.
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return Number of bytes written on success, -ERRNO on failure.
int
MyInMemoryFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    // Get file
    myFsFile *file;
    int ret = findFile(path, &file);

    // Check if file exists
    if (ret) {
        RETURN(-EBADF);
    }

    // If offset + size of new data extends our file data, realloc file.data
    ret = resizeFile(file, std::max(offset + (off_t) size, file->size));

    // Check if resizeFile failed
    if (ret) {
        RETURN(-ENOSPC);
    }

    memcpy(file->data + offset, buf, size);
    RETURN(size);
}

/// @brief Close a file.
///
/// In Part 1 this includes decrementing the open file count.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] fileInfo Can be ignored in Part 1 .
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();


    myFsFile *file;
    int ret = findFile(path, &file);
    if (ret) {
        RETURN(-ENOENT);
    }

    RETURN(0);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseTruncate(const char *path, off_t newSize) {
    LOGM();

    // Get File
    myFsFile *file;
    int ret = findFile(path, &file);
    if (ret) {
        RETURN(-ENOENT);
    }

    // Try resize file
    ret = resizeFile(file, newSize);
    if (ret) {
        RETURN(-EIO);
    }

    RETURN(0);
}

/// @brief Truncate a file.
///
/// Set the size of a file to the new size. If the new size is smaller than the old size, spare bytes are removed. If
/// the new size is larger than the old size, the new bytes may be random. This function is called for files that are
/// open.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] newSize New size of the file.
/// \param [in] fileInfo Can be ignored in Part 1.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseTruncate(const char *path, off_t newSize, struct fuse_file_info *fileInfo) {
    LOGM();

    // ¯\_(ツ)_/¯

    RETURN(fuseTruncate(path, newSize));
}

/// @brief Read a directory.
///
/// Read the content of the (only) directory.
/// You do not have to check file permissions, but can assume that it is always ok to access the directory.
/// \param [in] path Path of the directory. Should be "/" in our case.
/// \param [out] buf A buffer for storing the directory entries.
/// \param [in] filler A function for putting entries into the buffer.
/// \param [in] offset Can be ignored.
/// \param [in] fileInfo Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                              struct fuse_file_info *fileInfo) {
    LOGM();

    filler(buf, ".", NULL, 0); // Current Directory
    filler(buf, "..", NULL, 0); // Parent Directory

    // If the user is trying to show the files/directories of the root directory show the following
    if (strcmp(path, "/") == 0) {

        std::list<myFsFile>::iterator it;
        for (it = files.begin(); it != files.end(); ++it) {
            //TODO: Fill "stat" with file data instead of sending "nullptr"
            filler(buf, it->name.c_str(), nullptr, 0);
        }
        return 0;
    }

    return -ENOTDIR;
}

/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void *MyInMemoryFS::fuseInit(struct fuse_conn_info *conn) {
    // Open logfile
    this->logFile = fopen(((MyFsInfo *) fuse_get_context()->private_data)->logFile, "w+");
    if (this->logFile == NULL) {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *) fuse_get_context()->private_data)->logFile);
    } else {
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);

        LOG("Starting logging...\n");

        LOG("Using in-memory mode");

        // TODO: [PART 1] Implement your initialization methods here

    }

    RETURN(0);
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyInMemoryFS::fuseDestroy() {
    LOGM();


    // For each file in our fs
    for (auto i = files.begin(); i != files.end(); i++) {

        // free memory
        if (i->data) {
            free(i->data);
        }

        // remove file
        files.erase(i);
    }

}

/// @brief Find File in Filesystem
///
/// Search and return file from filesystem.
/// \param [in] path Name of the file, starting with "/".
/// \param [in,out] file Reference of file
/// \return 0 on success, -ERRNO on failure.
int MyInMemoryFS::findFile(const char *path, myFsFile **file) {
    LOGF("--> Trying to find %s", path);
    bool found = false;

    auto p = std::string(path + 1);

    // Suche in der liste nach datei mit dem namen pth
    for (auto i = files.begin(); i != files.end(); i++) {
        if (i->name == p) {
            *file = &(*i);
            return 0;
        }
    }
    return -ENOENT;

}

int MyInMemoryFS::resizeFile(myFsFile *file, off_t newsize) {

    // Always round up to next BLOCK_SIZE
    blkcnt_t newblkcnt = (newsize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // if no change in blocksize is necessary, just change file.size
    if (newblkcnt == file->nrBlocks) {
        file->size = newsize;
        return 0;
    }

    // Try to realloc file.data
    char *tmp = (char *) realloc(file->data, newblkcnt * BLOCK_SIZE);
    if (tmp == nullptr) {
        return -ENOMEM;
    }

    // Write back file data
    file->data = tmp;
    file->size = newsize;
    file->nrBlocks = newblkcnt;
    RETURN(0);
}

// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyInMemoryFS::SetInstance() {
    MyFS::_instance = new MyInMemoryFS();
}


//
// Created by Oliver Waldhorst on 20.03.20.
// Copyright © 2017-2020 Oliver Waldhorst. All rights reserved.
//

#include "myondiskfs.h"

// For documentation of FUSE methods see https://libfuse.github.io/doxygen/structfuse__operations.html

#undef DEBUG

#define DEBUG
#define DEBUG_METHODS
#define DEBUG_RETURN_VALUES

#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "macros.h"
#include "myfs.h"
#include "myfs-info.h"
#include "blockdevice.h"

/// @brief Constructor of the on-disk file system class.
///
/// You may add your own constructor code here.
MyOnDiskFS::MyOnDiskFS() : MyFS() {
    // create a block device object
    this->blockDevice = new BlockDevice(BLOCK_SIZE);
}

/// @brief Destructor of the on-disk file system class.
///
/// You may add your own destructor code here.
MyOnDiskFS::~MyOnDiskFS() {

    // free block device object
    delete this->blockDevice;
}

/// @brief Create a new file.
///
/// Create a new file with given name and permissions.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode Permissions for file access.
/// \param [in] dev Can be ignored.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseMknod(const char *path, mode_t mode, dev_t dev) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index >= 0) { RETURN(-EEXIST) }
    // Index should be -ENOENT from here

    char emptyFatEntry[MAX_NAME_LENGTH];
    for (char &i: emptyFatEntry) { i = 0; }

    // Find free slot in FAT
    for (int i = 0; i < 64; i++) {
        if (memcmp(fat[i].filename, emptyFatEntry, 32) == 0) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        RETURN(-ENOSPC);
    }

    // If we've come this far, we can create the entry.
    fatEntry newFile{};

    // Check if new name too long
    int length = strlen(path);
    if (length > MAX_NAME_LENGTH) {
        RETURN(-ENAMETOOLONG)
    }

    memcpy(newFile.filename, path + 1, length);
    newFile.uid = getuid();
    newFile.groupId = getgid();
    newFile.mode = mode;
    newFile.accessTime = time(0);
    newFile.modTime = time(0);
    newFile.changeTime = time(0);
    newFile.startBlock = 0;
    newFile.nrBlocks = 0;
    newFile.size = 0;

    // Add entry to FAT
    fat[index] = newFile;
    writeFat();
    RETURN(0);
}

/// @brief Delete a file.
///
/// Delete a file with given name from the file system.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseUnlink(const char *path) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    // Get number of block-list of file
    int nrBlocks = fat[index].nrBlocks;

    if (nrBlocks > 0) {
        unsigned short blockList[nrBlocks];

        // Create blockList
        blockList[0] = fat[index].startBlock;
        for (int i = 1; i < nrBlocks; ++i) {
            blockList[i] = blt[blockList[i - 1]];
        }

        // Set blocks as free in BLT
        for (auto b: blockList) {
            blt[b] = BLT_FREE;
        }
        writeBlt();
    }

    // Create empty fatEntry
    fatEntry empty = fatEntry();
    for (char &i: empty.filename) { i = 0; }
    empty.uid = getuid();
    empty.groupId = getgid();
    empty.mode = 0;
    empty.accessTime = 0;
    empty.modTime = 0;
    empty.changeTime = 0;
    empty.startBlock = 0;
    empty.nrBlocks = 0;
    empty.size = 0;

    // Delete FAT Entry
    fat[index] = empty;
    writeFat();

    RETURN(0);
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
int MyOnDiskFS::fuseRename(const char *path, const char *newpath) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    // Check if "newpath" exists
    if (getFileIndex(newpath) >= 0) {
        fuseUnlink(newpath);
    }

    // Check if new name too long
    int length = strlen(newpath);
    if (length > MAX_NAME_LENGTH) {
        RETURN(-ENAMETOOLONG)
    }

    memcpy(fat[index].filename, newpath + 1, length);

    int systemTime = time(0);
    fat[index].modTime = systemTime;
    fat[index].changeTime = systemTime;
    writeFat();

    RETURN(0);
}

/// @brief Get file meta data.
///
/// Get the metadata of a file (user & group id, modification times, permissions, ...).
/// \param [in] path Name of the file, starting with "/".
/// \param [out] statbuf Structure containing the meta data, for details type "man 2 stat" in a terminal.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseGetattr(const char *path, struct stat *statbuf) {
    LOGM();

    // catch if trying to get info for root dir
    if (strcmp(path, "/") == 0) {
        statbuf->st_mode = S_IFDIR | 0755;
        statbuf->st_nlink = 2;
        RETURN(0);
    }

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    // Fill statbuf with relevant data
    statbuf->st_uid = fat[index].uid;
    statbuf->st_gid = fat[index].groupId;
    statbuf->st_atime = fat[index].accessTime;
    statbuf->st_mtime = fat[index].modTime;
    statbuf->st_mode = fat[index].mode;
    statbuf->st_nlink = 1; // weil wir eine Datei sind und kein Verzeichnis
    statbuf->st_size = fat[index].size;


    int systemTime = time(0);
    fat[index].accessTime = systemTime;
    fat[index].changeTime = systemTime;
    writeFat();

    RETURN(0);
}

/// @brief Change file permissions.
///
/// Set new permissions for a file.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [in] mode New mode of the file.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseChmod(const char *path, mode_t mode) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    int systemTime = time(0);
    fat[index].mode = mode;
    fat[index].modTime = systemTime;
    fat[index].changeTime = systemTime;

    writeFat();
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
int MyOnDiskFS::fuseChown(const char *path, uid_t uid, gid_t gid) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    int systemTime = time(0);
    fat[index].uid = uid;
    fat[index].groupId = gid;
    fat[index].modTime = systemTime;
    fat[index].changeTime = systemTime;

    writeFat();
    RETURN(0)
}

/// @brief Open a file.
///
/// Open a file for reading or writing. This includes checking the permissions of the current user and incrementing the
/// open file count.
/// You do not have to check file permissions, but can assume that it is always ok to access the file.
/// \param [in] path Name of the file, starting with "/".
/// \param [out] fileInfo Can be ignored in Part 1
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseOpen(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    //check if the user has permissions to access the file
    if (!(fat[index].uid == getuid() || fat[index].groupId == getgid())) {
        RETURN(-ENOENT);
    }

    fileInfo->fh = index;

    int systemTime = time(0);
    fat[index].accessTime = systemTime;
    writeFat();

    RETURN(0)
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
int MyOnDiskFS::fuseRead(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    // Create blockList
    unsigned short blockList[fat[index].nrBlocks];
    blockList[0] = fat[index].startBlock;
    for (int i = 1; i < fat[index].nrBlocks; ++i) {
        blockList[i] = blt[blockList[i - 1]];
    }

    // Read to Block(s)
    char *buffer = new char[BLOCK_SIZE];
    int currentPos, currentBlock, currentBlockOffset, bytesToRead, currentBufPos;
    currentPos = offset;
    currentBufPos = 0;

    // While not at the end
    while (currentBufPos < size) {

        // Get parameters
        currentBlock = (int) currentPos / BLOCK_SIZE;
        currentBlockOffset = currentPos % BLOCK_SIZE;
        bytesToRead = BLOCK_SIZE - currentBlockOffset;

        // Make sure we don't read more that the file
        if (currentPos + bytesToRead > fat[index].size) {
            LOG("Tried to read more than file...");
            bytesToRead = fat[index].size - currentPos;
            if (bytesToRead == 0) {
                // Reached end of file. No more reading happening
                RETURN(currentBufPos);
            }
        }

        // Make sure we don't read more than we need
        if (currentPos + bytesToRead > offset + size) {
            LOG("Tried to read more than needed...");
            bytesToRead = offset + size - currentPos;
        }

        // Read data in case we write on block only partially
        blockDevice->read(blockList[currentBlock], buffer);
        memcpy(buf + currentBufPos, buffer, bytesToRead);

        currentPos += bytesToRead;
        currentBufPos += bytesToRead;
    }


    int systemTime = time(0);
    fat[index].accessTime = systemTime;
    writeFat();

    RETURN(currentBufPos);
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
MyOnDiskFS::fuseWrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    // Enlarge file if necessary
    if (size + offset > fat[index].size) {
        fuseTruncate(path, size + offset);
    }

    // Create blockList for easy lookup
    unsigned short blockList[fat[index].nrBlocks];
    blockList[0] = fat[index].startBlock;
    for (int i = 1; i < fat[index].nrBlocks; ++i) {
        blockList[i] = blt[blockList[i - 1]];
    }

    // Write to Block(s)
    char *buffer = new char[BLOCK_SIZE];
    int currentPos, currentBufPos, currentBlock, currentBlockOffset, bytesToWrite;
    currentPos = offset;
    currentBufPos = 0;

    // While not at the end of the file:
    while (currentBufPos < size) {

        // Get parameters
        currentBlock = (int) currentPos / BLOCK_SIZE;
        currentBlockOffset = currentPos % BLOCK_SIZE;
        bytesToWrite = BLOCK_SIZE - currentBlockOffset;

        // Make sure we don't write more than is stored in @buf
        if (currentPos + bytesToWrite > offset + size) {
            bytesToWrite = offset + size - currentPos;
        }

        // Read data in case we write on block only partially
        blockDevice->read(blockList[currentBlock], buffer);
        memcpy(buffer + currentBlockOffset, buf + currentBufPos, bytesToWrite);
        blockDevice->write(blockList[currentBlock], buffer);

        currentPos += bytesToWrite;
        currentBufPos += bytesToWrite;
    }

    if (fat[index].size < offset + size) {
        fat[index].size = offset + size;
    }


    int systemTime = time(0);
    fat[index].modTime = systemTime;
    fat[index].changeTime = systemTime;
    writeFat();

    RETURN((int) size);
}

/// @brief Close a file.
///
/// \param [in] path Name of the file, starting with "/".
/// \param [in] File handel for the file set by fuseOpen.
/// \return 0 on success, -ERRNO on failure.
int MyOnDiskFS::fuseRelease(const char *path, struct fuse_file_info *fileInfo) {
    LOGM();
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
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize) {
    LOGM();

    // Find file
    int index = getFileIndex(path);
    if (index < 0) { RETURN(index) }

    // CASE: We don't need to change size at all
    if (newSize == fat[index].size) {
        RETURN(0);
    }

    // Get number of Blocks needed
    auto nrBlocks = (newSize + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // CASE: Need to shrink size
    if (newSize < fat[index].size) {

        // Check if we can free some blocks
        if (fat[index].nrBlocks > nrBlocks) {

            // Create blockList for easy lookup
            unsigned short blockList[fat[index].nrBlocks];
            blockList[0] = fat[index].startBlock;
            for (int i = 1; i < fat[index].nrBlocks; ++i) {
                blockList[i] = blt[blockList[i - 1]];
            }

            // Set new EOF block
            blt[blockList[nrBlocks - 1]] = BLT_EOF;

            // Free remaining blocks
            for (int i = nrBlocks; i < fat[index].nrBlocks; ++i) {
                blt[blockList[i]] = BLT_FREE;
            }

            // Save changes to BLT (FAT changes saved later)
            fat[index].nrBlocks = nrBlocks;
            writeBlt();
        }
    }

    // CASE: Need to enlarge file
    if (newSize > fat[index].size) {

        // Check if we need more blocks
        if (nrBlocks > fat[index].nrBlocks) {

            // catch if file has no startBlock yet
            if (fat[index].nrBlocks == 0) {
                findFreeBlock(fat[index].startBlock);
                blt[fat[index].startBlock] = BLT_EOF;
                fat[index].nrBlocks = 1;    // Since we just allocated the first one
            }

            // Address "iterator"
            unsigned short currentAddress = fat[index].startBlock;

            // Go to end of blockList
            while (blt[currentAddress] != BLT_EOF) {
                currentAddress = blt[currentAddress];
            }

            // Allocate new blocks
            unsigned short freeBlock;
            for (int i = 0; i < nrBlocks - fat[index].nrBlocks; i++) {
                findFreeBlock(freeBlock);

                blt[currentAddress] = freeBlock;        // Set last EOF to new free block
                currentAddress = blt[currentAddress];   // Get new EOF
                blt[currentAddress] = BLT_EOF;          // Set current EOF
            }

            // Save changes to BLT (FAT changes saved later)
            fat[index].nrBlocks = nrBlocks;
            writeBlt();
        }
    }

    fat[index].size = newSize;

    int systemTime = time(0);
    fat[index].modTime = systemTime;
    fat[index].changeTime = systemTime;
    writeFat();

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
int MyOnDiskFS::fuseTruncate(const char *path, off_t newSize, struct fuse_file_info *fileInfo) {
    LOGM();

    // ¯\_(ツ)_/¯
    int ret = fuseTruncate(path, newSize);
    RETURN(ret);
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
int MyOnDiskFS::fuseReaddir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                            struct fuse_file_info *fileInfo) {
    LOGM();

    filler(buf, ".", nullptr, 0); // Current Directory
    filler(buf, "..", nullptr, 0); // Parent Directory

    // If the user is trying to show the files/directories of the root directory show the following
    if (strcmp(path, "/") == 0) {

        char emptyFileName[32];
        for (char &i: emptyFileName) {
            i = 0;
        }

        for (int i = 0; i < TOTAL_FAT_ENTRIES; i++) {
            if (memcmp(fat[i].filename, emptyFileName, 32) != 0) {
                filler(buf, fat[i].filename, nullptr, 0);
            }
        }
        RETURN(0)
    }

    RETURN(-ENOTDIR)
}

/// Initialize a file system.
///
/// This function is called when the file system is mounted. You may add some initializing code here.
/// \param [in] conn Can be ignored.
/// \return 0.
void *MyOnDiskFS::fuseInit(struct fuse_conn_info *conn) {
    // Open logfile
    this->logFile = fopen(((MyFsInfo *) fuse_get_context()->private_data)->logFile, "w+");
    if (this->logFile == NULL) {
        fprintf(stderr, "ERROR: Cannot open logfile %s\n", ((MyFsInfo *) fuse_get_context()->private_data)->logFile);
    } else {
        // turn of logfile buffering
        setvbuf(this->logFile, NULL, _IOLBF, 0);

        LOG("Starting logging...\n");

        LOG("Using on-disk mode");

        LOGF("Container file name: %s", ((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        int ret = this->blockDevice->open(((MyFsInfo *) fuse_get_context()->private_data)->contFile);

        if (ret >= 0) {
            LOG("Container file exists, reading...");
            readFat();
            readBlt();

        } else if (ret == -ENOENT) {
            LOG("Container file does not exist, creating a new one...");

            ret = this->blockDevice->create(((MyFsInfo *) fuse_get_context()->private_data)->contFile);

            if (ret >= 0) {

                LOG("Creating FAT");

                // Create empty fatEntry
                fatEntry empty = fatEntry();
                for (char &i: empty.filename) {
                    i = 0;
                }
                empty.uid = getuid();
                empty.groupId = getgid();
                empty.mode = 0;
                empty.accessTime = 0;
                empty.modTime = 0;
                empty.changeTime = 0;
                empty.startBlock = 0;
                empty.nrBlocks = 0;
                empty.size = 0;

                // Set all FAT entries to empty
                for (fatEntry &i: fat) {
                    i = empty;
                }
                writeFat();

                LOG("Creating BLT");
                for (int i = 0; i < TOTAL_BLT_ENTRIES; i++) {
                    if (i < FAT_BLOCKS + BLT_BLOCKS) {
                        blt[i] = BLT_RSV; // Blocks used for FAT and BLT are reserved
                    } else {
                        blt[i] = BLT_FREE; // All other Blocks are free
                    }
                }
                writeBlt();
            }
        }

        if (ret < 0) {
            LOGF("ERROR: Access to container file failed with error %d", ret);
        }
    }

    return EXIT_SUCCESS;
}

/// @brief Clean up a file system.
///
/// This function is called when the file system is unmounted. You may add some cleanup code here.
void MyOnDiskFS::fuseDestroy() {
    LOGM();

    writeFat();
    writeBlt();
}

/// @brief Read FAT from container file and update local FAT
///
/// \return ERRNO on failure, 0 on success
int MyOnDiskFS::readFat() {
    LOGM();

    char *buffer = new char[BLOCK_SIZE];
    char *ptr;
    fatEntry e{};

    for (int blockNo = 0; blockNo < FAT_BLOCKS; blockNo++) {

        // (Re)set Pointer to the same already allocated buffer
        ptr = buffer;
        blockDevice->read(blockNo, ptr);

        for (int i = 0; i < FAT_ENTRIES_PER_BLOCK; i++) {

            // Read filename
            memcpy(e.filename, ptr, MAX_NAME_LENGTH);
            ptr += 32;

            // Read uid
            memcpy(&e.uid, ptr, 4);
            ptr += 4;

            // Read gid
            memcpy(&e.groupId, ptr, 4);
            ptr += 4;

            // Read mode
            memcpy(&e.mode, ptr, 4);
            ptr += 4;

            // Read access Time
            memcpy(&e.accessTime, ptr, 4);
            ptr += 4;

            // Read mod time
            memcpy(&e.modTime, ptr, 4);
            ptr += 4;

            // Read change time
            memcpy(&e.changeTime, ptr, 4);
            ptr += 4;

            // Read startBlock
            memcpy(&e.startBlock, ptr, 2);
            ptr += 2;

            // Read nrBlocks
            memcpy(&e.nrBlocks, ptr, 2);
            ptr += 2;

            // Read size
            memcpy(&e.size, ptr, 4);
            ptr += 4;

            // Set current entry
            fat[(blockNo * FAT_ENTRIES_PER_BLOCK) + i] = e;
        }
    }
    delete[] buffer;
    return EXIT_SUCCESS;
}

/// @brief Write current Fat to Containerfile
///
/// \return ERRNO on failure, 0 on success
int MyOnDiskFS::writeFat() {
    LOGM();

    char *buffer = new char[BLOCK_SIZE];
    char *ptr;

    fatEntry e{};

    for (int blockNumber = 0; blockNumber < FAT_BLOCKS; blockNumber++) {

        // (Re)set Pointer to the same already allocated buffer
        ptr = buffer;

        for (int i = 0; i < FAT_ENTRIES_PER_BLOCK; i++) {

            // Get current Entry
            e = fat[(blockNumber * FAT_ENTRIES_PER_BLOCK) + i];

            // Write Filename
            memcpy(ptr, e.filename, MAX_NAME_LENGTH);
            ptr += MAX_NAME_LENGTH;

            // Write UID
            memcpy(ptr, &e.uid, 4);
            ptr += 4;

            // Write GID
            memcpy(ptr, &e.groupId, 4);
            ptr += 4;

            // Write Mode
            memcpy(ptr, &e.mode, 4);
            ptr += 4;

            // Write access time
            memcpy(ptr, &e.accessTime, 4);
            ptr += 4;

            // Write mode time
            memcpy(ptr, &e.modTime, 4);
            ptr += 4;

            // Write change time
            memcpy(ptr, &e.changeTime, 4);
            ptr += 4;

            // Write startBlock
            memcpy(ptr, &e.startBlock, 2);
            ptr += 2;

            // Write nrBlocks
            memcpy(ptr, &e.nrBlocks, 2);
            ptr += 2;

            // Write size
            memcpy(ptr, &e.size, 4);
            ptr += 4;
        }

        blockDevice->write(blockNumber, buffer);
    }
    delete[] buffer;
    return EXIT_SUCCESS;
}

/// @brief Read BLT from container file and update local BLT
///
/// \return ERRNO on failure, 0 on success
int MyOnDiskFS::readBlt() {
    LOGM();

    char *buffer = new char[BLOCK_SIZE];
    char *ptr;

    for (int blockNr = 0; blockNr < BLT_BLOCKS; ++blockNr) {

        ptr = buffer;
        blockDevice->read(blockNr + FAT_BLOCKS, buffer);

        for (int i = 0; i < BLT_ENTRIES_PER_BLOCK; ++i) {

            memcpy(&blt[(blockNr * BLT_ENTRIES_PER_BLOCK) + i], ptr, 2);
            ptr += 2;
        }
    }
    delete[] buffer;
    return EXIT_SUCCESS;
}

/// @brief Write current Fat to Containerfile
///
/// \return ERRNO on failure, 0 on success
int MyOnDiskFS::writeBlt() {
    LOGM();

    char *buffer = new char[BLOCK_SIZE];
    char *ptr;

    for (int blockNo = 0; blockNo < BLT_BLOCKS; blockNo++) {

        // (Re)set Pointer to the same already allocated buffer
        ptr = buffer;

        for (int i = 0; i < BLT_ENTRIES_PER_BLOCK; i++) {

            memcpy(ptr, &blt[(blockNo * BLT_ENTRIES_PER_BLOCK) + i], 2);
            ptr += 2;

        }
        // BLT is located AFTER FAT, so we offset by total FAT Blocks
        blockDevice->write(blockNo + FAT_BLOCKS, buffer);
    }
    delete[] buffer;
    return EXIT_SUCCESS;
}

/// @brief Find file in fat array.
/// Note that path must include leading '/'.F
/// \param path [in] Filename of file to return
/// \return Index of file if found, -ERRNO otherwise
int MyOnDiskFS::getFileIndex(const char *path) {
    for (int i = 0; i < 64; i++) {
        if (strcmp(fat[i].filename, path + 1) == 0) {
            return i;
        }
    }
    return -ENOENT;
}

int MyOnDiskFS::findFreeBlock(unsigned short &freeBlock) {
    for (int i = 0; i < TOTAL_BLT_ENTRIES; i++) {
        if (blt[i] == BLT_FREE) {
            freeBlock = i;
            return EXIT_SUCCESS;
        }
    }
    return -ENOSPC;
}


// DO NOT EDIT ANYTHING BELOW THIS LINE!!!

/// @brief Set the static instance of the file system.
///
/// Do not edit this method!
void MyOnDiskFS::SetInstance() {
    MyFS::_instance = new MyOnDiskFS();
}

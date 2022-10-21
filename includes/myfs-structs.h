//
//  myfs-structs.h
//  myfs
//
//  Created by Oliver Waldhorst on 07.09.17.
//  Copyright © 2017 Oliver Waldhorst. All rights reserved.
//

#ifndef myfs_structs_h
#define myfs_structs_h

#include <string>
#include <array>

// FS Constants
const int BLOCK_SIZE = 512;

// BLT Constants
const int TOTAL_BLT_ENTRIES = 0x10000;
const int BLT_BLOCKS = 256;
const int BLT_ENTRIES_PER_BLOCK = 256;

const unsigned short BLT_FREE = 0x0000; // Free Block
const unsigned short BLT_EOF = 0x0001;  // End of File
const unsigned short BLT_RSV = 0x0002;  // Reserved

// FAT Constants
const int TOTAL_FAT_ENTRIES = 64;
const int FAT_BLOCKS = 8;
const int FAT_ENTRIES_PER_BLOCK = 8;
const int MAX_NAME_LENGTH = 32;

struct myFsFile {
    std::string name;
    uid_t userId;
    gid_t groupId;
    mode_t mode;
    int accessTime; // letzter Zugriff
    int modTime;    // letzte Veränderung
    int changeTime; // letzte Statusänderung
    char *data;     // Pointer auf Daten-anfang
    off_t size;        // Dateigröße, in bytes
    blkcnt_t nrBlocks; // Number of 512B blocks allocated
};

struct fatEntry {
    char filename[MAX_NAME_LENGTH];
    uid_t uid;                          // 4 Byte
    gid_t groupId;                      // 4 Byte
    mode_t mode;                        // 4 Byte
    int accessTime;                     // 4 Byte
    int modTime;                        // 4 Byte
    int changeTime;                     // 4 Byte
    unsigned short startBlock;          // 2 Byte
    unsigned short nrBlocks;            // 2 Byte
    off_t size;                         // 4 Byte
};

#endif /* myfs_structs_h */

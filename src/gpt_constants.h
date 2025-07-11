#ifndef GPT_CONSTANTS_H
#define GPT_CONSTANTS_H


enum {
    LBA_SIZE = 512,
    GPT_PARTITION_ENTRY_SIZE = 128,
    NUMBER_OF_GPT_ENTRIES = 128,
    GPT_TABLE_SIZE = 16384,         // 128 * 128 entries
    ALIGNMENT = 1048576,            // 1 MiB alignment
};

#endif
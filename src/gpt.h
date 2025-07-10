#ifndef GPT_H
#define GPT_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define LBA_SIZE 512

// Globally Unique Identifier
typedef struct {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_ver;
    uint8_t clock_seq_hi_and_res;
    uint8_t clock_seq_low;
    uint8_t node[6];
} __attribute__ ((packed)) Guid;

// GPT Header
typedef struct {
    uint8_t signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved_1;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    Guid disk_guid;
    uint64_t partition_table_lba;
    uint32_t number_of_entries;
    uint32_t size_of_entry;
    uint32_t partition_table_crc32;

    uint8_t reserved_2[512-92];
} __attribute__ ((packed)) Gpt_Header;

// Functions
Guid new_guid(void);

bool write_gpt(FILE* image, uint64_t image_size_lbas);

#endif
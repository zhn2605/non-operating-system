#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "structures.h"

// Globals
extern char *image_name;
extern uint64_t esp_size;       // 33 MiB
extern uint64_t data_size;      // 1 MiB
extern uint64_t image_size;
extern uint64_t image_size_lbas, esp_size_lbas, data_size_lbas, gpt_table_lbas;
extern uint64_t align_lba, esp_lba, data_lba;
extern uint32_t crc_table[256];

// Convert bytes to LBAs
uint64_t bytes_to_lbas(const uint64_t bytes);
// Pad out 0s for bigger lbas
void write_full_lba_size(FILE* image);
uint64_t next_aligned_lba(uint64_t lba);
void get_fat_dir_entry_time_date(uint16_t *in_time, uint16_t *in_date);
// Create CRC32 table values
void create_crc32_table(void);
uint32_t calculate_crc32(const void *buf, int32_t len);
// Create vers 4 Variant 2 GUID
Guid new_guid(void);

#endif
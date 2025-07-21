#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"
#include "gpt_constants.h"

char *image_name = "test.img";

uint64_t    esp_size = 1024*1024*33;    // 33 MiB
uint64_t    data_size = 1024*1024*1;    // 1 MiB
uint64_t    image_size = 0;
uint64_t    image_size_lbas = 0, esp_size_lbas = 0, data_size_lbas = 0,
            gpt_table_lbas = 0;                                 // Sizes in lbas
uint64_t    align_lba = 0, esp_lba = 0, data_lba = 0, 
            fat32_fat_lba = 0, fat32_data_lba = 0;             // Starting LBA values

            uint32_t crc_table[256];

// Convert bytes to LBAs
uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes / LBA_SIZE) + (bytes % LBA_SIZE > 0 ? 1 : 0); 
    // add extra lba in case of partial byte count
}

void write_full_lba_size(FILE* image) {
    uint64_t lba_size = LBA_SIZE;
    uint8_t zero_sector[512] = { 0 };
    for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++) {
        fwrite(zero_sector, sizeof zero_sector, 1, image);
    }
}

uint64_t next_aligned_lba(uint64_t lba) {
    return lba - (lba % align_lba) + align_lba;
}

void get_fat_dir_entry_time_date(uint16_t *in_time, uint16_t *in_date) {
    time_t curr_time;
    curr_time = time(NULL);
    struct tm tm = *localtime(&curr_time);

    // FAT32 relative to 1980, 
    // tm relative to 1900, 
    // thus subtract 80
    *in_date = ((tm.tm_year - 80) << 9) | (tm.tm_mon + 1) << 5 | (tm.tm_mday);      
    
    // Seconds is 2 second count, 0-29
    if (tm.tm_sec == 60) tm.tm_sec = 59;
    *in_time = (tm.tm_hour) << 11 | (tm.tm_min) << 5 | (tm.tm_sec) / 2;
}

void create_crc32_table(void) {
    uint32_t c;

    for(int32_t n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for (int32_t k = 0; k < 8; k++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1); // magic constant
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
}

uint32_t calculate_crc32(const void *buf, int32_t len) {
    static bool made_crc_table = false;

    const uint8_t *bufp = buf;
    uint32_t c = 0xFFFFFFFFL;

    if (!made_crc_table) {
        create_crc32_table();
        made_crc_table = true;
    }

    for (int32_t n = 0; n < len; n++) {
        c = crc_table[(c ^ bufp[n]) & 0xFF] ^ (c >> 8);
    }

    // invert bits to return value
    return c ^ 0xFFFFFFFFL;
}

Guid new_guid(void) {
    uint8_t rand_arr[16] = { 0 };

    for (uint8_t i = 0; i < sizeof rand_arr; i++) {
        rand_arr[i] = rand() % (UINT8_MAX + 1);
    }

    Guid result = {
        .time_low = *(uint32_t *)&rand_arr[0],
        .time_mid = *(uint16_t *)&rand_arr[4],
        .time_hi_and_ver = *(uint16_t *)&rand_arr[6],
        .clock_seq_hi_and_res = rand_arr[7],
        .clock_seq_low = rand_arr[8],
        .node = { rand_arr[10], rand_arr[11], rand_arr[12], rand_arr[13],
            rand_arr[14], rand_arr[15] },
    };

    // Format GUID (UUID vers 4) to be recognized
    // Set version to 4 (0100)
    result.time_hi_and_ver &= 0x0FFF;
    result.time_hi_and_ver |= 0x4000;

    // Set variant to RFC 4122 (10xx)
    result.clock_seq_hi_and_res &= 0x3F;
    result.clock_seq_hi_and_res |= 0x80;

    return result;
}
#include "gpt.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t crc_table[256];

// Create vers 4 Variant 2 GUID
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

// Write GPT headers & tables, primary and alternate
bool write_gpt(FILE *image, uint64_t image_size_lbas) {
    Gpt_Header primary_gpt = {
        .signature = { "EFI PART" },
        .revision = 0x00010000,
        .header_size = 92,  // vers 1
        .header_crc32 = 0,  // calculated later
        .reserved_1 = 0,
        .my_lba = 1,
        .alternate_lba = image_size_lbas - 1,
        .first_usable_lba = 34, // MBR + GPT + Primary GPT table
        .last_usable_lba = image_size_lbas - 34, // 2nd GPT header, table
        .disk_guid = new_guid(),
        .partition_table_lba = 2, // After MBR, GPT Header
        .number_of_entries = 128,
        .size_of_entry = 128,
        .partition_table_crc32 = 0,
        .reserved_2 = { 0 },
    };

    // TODO: 
    // Fill out primary table entries
    // Fill out CRC
    // write gpt header to file
    // write gpt table to file
    // write alternate header and table

    return true;
}

// Create CRC32 table values
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
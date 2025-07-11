#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gpt.h"
#include "gpt_constants.h"

static uint32_t crc_table[256];

uint64_t align_lba = 0;

// Constants and enums
const Guid ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B,
    { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } 
}; 
const Guid LINUX_DATA_GUID = {
    0x0FC63DAF, 0x8483, 0x4772, 0x8E, 0x79,
    { 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 } 
};

// Pad out 0s for bigger lbas
void write_full_lba_size(FILE* image, uint64_t lba_size) {
    uint8_t zero_sector[512];
    for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++) {
        fwrite(zero_sector, sizeof zero_sector, 1, image);
    }
}

uint64_t next_aligned_lba(uint64_t lba) {
    return lba - (lba % align_lba) + align_lba;
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
bool write_gpt(FILE *image, uint64_t image_size_lbas, uint64_t esp_size_lbas, uint64_t data_size_lbas) {
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

    align_lba = ALIGNMENT / LBA_SIZE;
    uint64_t esp_lba = align_lba;
    uint64_t data_lba = next_aligned_lba(esp_lba + esp_size_lbas);
    // TODO: 
    // Fill out primary table entries
    Gpt_Partition_Entry gpt_table[NUMBER_OF_GPT_ENTRIES] = {
        // EFI System Partition
        {
            .partition_type_guid = ESP_GUID,
            .unique_guid = new_guid(),
            .starting_lba = esp_lba,
            .ending_lba = esp_lba + esp_size_lbas,
            .attributes = 0,
            .name = u"EFI SYSTEM" 
        },

        // Basic Data Partition
        {
            .partition_type_guid = LINUX_DATA_GUID,
            .unique_guid = new_guid(),
            .starting_lba = data_lba,
            .ending_lba = data_lba + data_size_lbas,
            .attributes = 0,
            .name = u"LINUX DATA" 
        },
    };

    // Fill out CRC
    primary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    primary_gpt.header_crc32 = calculate_crc32(&primary_gpt, primary_gpt.header_size);

    // write gpt header to file
    if (fwrite(&primary_gpt, 1, sizeof primary_gpt, image) != sizeof primary_gpt) {
        return false;
    }
    write_full_lba_size(image, LBA_SIZE);

    // write gpt table to file
    if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table) {
        return false;
    }

    // Fill out secondary gpt
    Gpt_Header secondary_gpt = primary_gpt;
    secondary_gpt.header_crc32 = 0;
    secondary_gpt.partition_table_crc32 = 0;
    secondary_gpt.my_lba = primary_gpt.alternate_lba;
    secondary_gpt.alternate_lba = primary_gpt.my_lba;
    secondary_gpt.partition_table_lba = image_size_lbas - 1 - 32; // - 32 entries and one before last

    // fill crc32 for secondary
    secondary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    secondary_gpt.header_crc32 = calculate_crc32(&secondary_gpt, secondary_gpt.header_size);

    // write alternate header and table
    fseek(image, secondary_gpt.partition_table_lba * LBA_SIZE, SEEK_SET);

    // write secondary gpt table to file
    if (fwrite(&gpt_table, 1, sizeof gpt_table, image) != sizeof gpt_table) {
        return false;
    }

    // write secondary gpt header to file
    if (fwrite(&secondary_gpt, 1, sizeof secondary_gpt, image) != sizeof secondary_gpt) {
        return false;
    }
    write_full_lba_size(image, LBA_SIZE);

    return true;
}
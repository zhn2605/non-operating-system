#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <uchar.h>
#include "gpt.h"
#include "structures.h"
#include "utils.h"
#include "gpt_constants.h"

// Constants and enums
const Guid ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B,
    { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } 
};
const Guid LINUX_DATA_GUID = {
    0x0FC63DAF, 0x8483, 0x4772, 0x8E, 0x79,
    { 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 } 
};

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

    align_lba = ALIGNMENT / LBA_SIZE;
    esp_lba = align_lba;
    data_lba = next_aligned_lba(esp_lba + esp_size_lbas);

    // TODO: 
    // Fill out primary table entries
    Gpt_Partition_Entry gpt_table[NUMBER_OF_GPT_ENTRIES] = {
        // EFI System Partition
        {
            .partition_type_guid = ESP_GUID,
            .unique_guid = new_guid(),
            .starting_lba = esp_lba,
            .ending_lba = esp_lba + esp_size_lbas - 1,
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
    primary_gpt.partition_table_crc32 = 0;
    primary_gpt.header_crc32 = 0;
    primary_gpt.partition_table_crc32 = calculate_crc32(gpt_table, sizeof gpt_table);
    primary_gpt.header_crc32 = calculate_crc32(&primary_gpt, primary_gpt.header_size);

    // write gpt header to file
    if (fwrite(&primary_gpt, 1, sizeof primary_gpt, image) != sizeof primary_gpt) {
        return false;
    }
    write_full_lba_size(image);

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
    write_full_lba_size(image);

    return true;
}
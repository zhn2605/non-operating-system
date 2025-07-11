#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "gpt.h"
#include "gpt_constants.h"

// Globals
char *image_name = "test.img";
uint64_t esp_size = 1024*1024*33;       // 33 MiB
uint64_t data_size = 1024*1024*1;       // 1 MiB
uint64_t image_size = 0;
uint64_t image_size_lbas = 0, esp_size_lbas = 0, data_size_lbas = 0;
uint64_t esp_lba = 0, data_lba = 0;
uint32_t crc_table[256];

// 16 byte MBR partition
typedef struct {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_lba;
} __attribute__ ((packed)) Mbr_Partition;

// Master Boot Record
typedef struct {
    uint8_t boot_code[440];
    uint32_t mbr_signature;
    uint16_t unkown; // 2 bytes unkown
    Mbr_Partition partition[4]; // define first partition
    uint16_t boot_signature;
} __attribute__ ((packed)) Mbr;


//===================================================================================
//===================================================================================

// Convert bytes to LBAs
uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes / LBA_SIZE) + (bytes % LBA_SIZE > 0 ? 1 : 0); 
    // add extra lba in case of partial byte count
}

// Write protective MBR
bool write_mbr(FILE *image) {
    uint64_t mbr_image_lbas = image_size_lbas;
    if (mbr_image_lbas > 0xFFFFFFFF) image_size_lbas = 0x100000000;

    Mbr mbr = {
        .boot_code = { 0 },
        .mbr_signature = 0,
        .unkown = 0,
        .partition[0] = {
            .boot_indicator = 0,
            .starting_chs = { 0x00, 0x02, 0x00 },
            .os_type = 0xEE,        // i.e. GPT Protective
            .ending_chs = { 0xFF, 0xFF, 0xFF },
            .starting_lba = 0x00000001,
            .size_lba = mbr_image_lbas - 1,
        },
        .boot_signature = 0xAA55
    };

    if (fwrite(&mbr, 1, sizeof mbr, image) != sizeof mbr) {
        return false;
    }

    // fill out lba
    write_full_lba_size(image, LBA_SIZE);
    
    return true;
}

int main(void) {
    puts("Hello");

    // img creation
    FILE *image = fopen(image_name, "wb+"); // specify image location and permissions
    if (!image) {
        fprintf(stderr, "Error: could not open file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Set size
    const uint64_t padding = (ALIGNMENT*2 + (LBA_SIZE * 67)); // extra padding for GPTs/MBR
    image_size = esp_size + data_size + (padding); // add padding
    image_size_lbas = bytes_to_lbas(image_size);
    esp_size_lbas = bytes_to_lbas(esp_size);
    data_size_lbas = bytes_to_lbas(data_size);


    // Seed rand
    srand(time(NULL));

    // Write protective MBR
    if (!write_mbr(image)) {
        fprintf(stderr, "Error: could not write protective MBR for file %s\n", image_name);
        return EXIT_FAILURE;
    }    

    // Write GPT headers & tables
    if (!write_gpt(image, image_size_lbas, esp_size_lbas, data_size_lbas)) {
        fprintf(stderr, "Error: could not write GPT headers & tables for file %s\n", image_name);
        return EXIT_FAILURE;
    } 

    return EXIT_SUCCESS;
}
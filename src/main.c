#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "gpt_constants.h"
#include "structures.h"
#include "utils.h"
#include "mbr.h"
#include "gpt.h"
#include "fat32.h"

int main(void) {
    // img creation
    FILE *image = fopen(image_name, "wb+"); // specify image location and permissions
    if (!image) {
        fprintf(stderr, "Error: could not open file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Set size
    gpt_table_lbas = GPT_TABLE_SIZE / LBA_SIZE;

    const uint64_t padding = (ALIGNMENT*2 + (LBA_SIZE * ((gpt_table_lbas*2) + 1 + 2))); // extra padding for GPTs/MBR
    image_size = esp_size + data_size + padding; // add padding
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
    if (!write_gpt(image, image_size_lbas)) {
        fprintf(stderr, "Error: could not write GPT headers & tables for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Write EFI System Partition
    if (!write_esp(image)) {
        fprintf(stderr, "Error: could not write ESP for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // Check if "BOOTx64.EFI" file exists in curr directory and automatically add it to ESP
    // FILE *fp = fopen("BOOTX64.EFI", "rb");
    // if(fp) {
    //     fclose(fp);
    //     char *path[25] = calloc(1, 25);
    //     strcpy(path, "/EFI/BOOT/BOOTX64.EFI");
    //     if (!add_path_to_esp(path, image)) {
    //         fprintf(stderr, "Error: Could not add file '%s'\n", path);
    //     }
    // }

    fclose(image);

    return EXIT_SUCCESS;
}
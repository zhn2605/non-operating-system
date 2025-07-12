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
uint64_t image_size_lbas = 0, esp_size_lbas = 0, data_size_lbas = 0,
        gpt_table_lbas = 0;
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

// FAT32 Volume Boot Record
typedef struct {
    uint8_t     BS_jmpBoot[3];
    uint8_t     BS_OEMName[8];
    uint16_t    BPB_BytesPerSec;
    uint8_t     BPB_SecPerClus;
    uint16_t    BPB_RsvdSecCnt;
    uint8_t     BPB_NumFATs;
    uint16_t    BPB_RootEntCnt;
    uint16_t    BPB_TotSec16;
    uint8_t     BPB_Media;
    uint16_t    BPB_FATSz16;
    uint16_t    BPB_SecPerTrk;
    uint16_t    BPB_NumHeads;
    uint32_t    BPB_HiddSec;
    uint32_t    BPB_TotSec32;
    uint32_t    BPB_FATSz32;
    uint16_t    BPB_ExtFlags;
    uint16_t    BPB_FSVer;
    uint32_t    BPB_RootClus;
    uint16_t    BPB_FSInfo;
    uint16_t    BPB_BkBootSec;
    uint8_t     BPB_Reserved[12];
    uint8_t     BS_DrvNum;
    uint8_t     BS_Reserved1;
    uint8_t     BS_BootSig;
    uint8_t     BS_VolID[4];
    uint8_t     BS_VolLab[11];
    uint8_t     BS_FilSysType[8];

    uint8_t     boot_code[510-90];
    uint16_t    bootsect_sig;   // 0xAA55
} __attribute__ ((packed)) Vbr;


// FAT32 File System Info Sector
typedef struct {
    uint32_t FSI_LeadSig;
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Next_Free;
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
} __attribute__ ((packed)) FSInfo;

// FAT32 Directory Entry
typedef struct {

} __attribute__ ((packed)) FAT32_Dir_Entry_Short;

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

// Write esp
bool write_esp(FILE* image) {
    const uint8_t reserved_sectors = 32;
    Vbr vbr = {
        .BS_jmpBoot = { 0xEB, 0x00, 0x90 }, 
        .BS_OEMName  = { "THISDISK" },
        .BPB_BytesPerSec = LBA_SIZE,
        .BPB_SecPerClus = 1,
        .BPB_RsvdSecCnt = reserved_sectors,
        .BPB_NumFATs = 2,
        .BPB_RootEntCnt = 0,
        .BPB_TotSec16 = 0,
        .BPB_Media = 0xF8, // "Fixed" non-removable media
        .BPB_FATSz16 = 0,
        .BPB_SecPerTrk = 0,
        .BPB_NumHeads = 0,
        .BPB_HiddSec = esp_lba - 1,
        .BPB_TotSec32 = esp_size_lbas,
        .BPB_FATSz32 = ((ALIGNMENT / LBA_SIZE) - reserved_sectors) / 2,
        .BPB_ExtFlags = 0,
        .BPB_FSVer = 0,
        .BPB_RootClus = 2,
        .BPB_FSInfo = 1,
        .BPB_BkBootSec = 6,
        .BPB_Reserved = { 0 },
        .BS_DrvNum = 0x80,
        .BS_Reserved1 = 0,
        .BS_BootSig = 0x29,
        .BS_VolID = { 0 },
        .BS_VolLab = { "NO NAME    " },
        .BS_FilSysType = {"FAT32   "},

        .boot_code = { 0 },
        .bootsect_sig = 0xAA55
    };

    //TODO:
    // fill out file system info sector
    FSInfo fsinfo = {
        .FSI_LeadSig = 0x41615252,
        .FSI_Reserved1 = {0},
        .FSI_StrucSig = 0x61417272,
        .FSI_Free_Count = 0xFFFFFFFF,
        .FSI_Next_Free = 0xFFFFFFFF,
        .FSI_Reserved2 = {0},
        .FSI_TrailSig = 0xAA550000,
    };

    // write vbr and fs info
    fseek(image, esp_lba * LBA_SIZE, SEEK_SET);
    if ((fwrite(&vbr, 1, sizeof vbr, image)) != sizeof vbr) {
        printf("Error: Could not write ESP VBR to image\n");
        return false;
    }
    write_full_lba_size(image, LBA_SIZE);

    if ((fwrite(&fsinfo, 1, sizeof fsinfo, image)) != sizeof fsinfo) {
        printf("Error: Could not write ESP FSInfo to image\n");
        return false;
    }
    write_full_lba_size(image, LBA_SIZE);

    // go to backup boot sector location
    fseek(image, (esp_lba  + vbr.BPB_BkBootSec) * LBA_SIZE, SEEK_SET);

    // write vbr abd fs info
    fseek(image, esp_lba * LBA_SIZE, SEEK_SET);
    if ((fwrite(&vbr, 1, sizeof vbr, image)) != sizeof vbr) {
        printf("Error: Could not write VBR to image\n");
        return false;
    }
    write_full_lba_size(image, LBA_SIZE);
    
    // write FATs
    uint32_t fat_lba = esp_lba = vbr.BPB_RsvdSecCnt;
    for (uint8_t i = 0; i < vbr.BPB_NumFATs; i++) {
        fseek(image, (fat_lba + (i*vbr.BPB_FATSz32)) * LBA_SIZE, SEEK_SET); 
        uint32_t cluster = 0;
        
        // cluster 0 reserved; FAT identifier, lowest 8 bits are media type
        cluster = 0xFFFFFF00 | vbr.BPB_Media;
        fwrite(&cluster, sizeof cluster, 1, image);

        // cluster 1; End of Chain marker
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);
        
        // cluster 2; Root dir cluster start
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);

        // cluster 3; '/EFI' dir cluster'
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);
        
        // cluster 4; '/EFI/BOOT' dir cluster
        cluster = 0xFFFFFFFF;
        fwrite(&cluster, sizeof cluster, 1, image);
        
        // cluster 5+; other files
    }

    // write file data
    const uint32_t data_lba = fat_lba + (vbr.BPB_NumFATs * vbr.BPB_FATSz32);
    fseek(image, data_lba * LBA_SIZE, SEEK_SET);

    // root directory
    // /efi directory
    // /boot directory

    return true;
}

//===================================================================================
// Main 
//===================================================================================


int main(void) {
    puts("Hello");

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
    if (!write_gpt(image, image_size_lbas, esp_size_lbas, data_size_lbas)) {
        fprintf(stderr, "Error: could not write GPT headers & tables for file %s\n", image_name);
        return EXIT_FAILURE;
    } 

    // Write EFI System Partition
    if (!write_esp(image)) {
        fprintf(stderr, "Error: could not write ESP for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
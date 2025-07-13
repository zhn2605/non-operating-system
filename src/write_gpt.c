#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <time.h>

#include "gpt_constants.h"

// Globals
char *image_name = "test.img";
uint64_t esp_size = 1024*1024*33;       // 33 MiB
uint64_t data_size = 1024*1024*1;       // 1 MiB
uint64_t image_size = 0;
uint64_t image_size_lbas = 0, esp_size_lbas = 0, data_size_lbas = 0,
        gpt_table_lbas = 0;
uint64_t align_lba = 0, esp_lba = 0, data_lba = 0;
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

typedef struct {
    Guid partition_type_guid;
    Guid unique_guid;
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    char16_t name[36]; // UCS-2 (UTF-16 limited to code points 0x0000 - 0xFFFF)
} __attribute__ ((packed)) Gpt_Partition_Entry;

// Constants and enums
const Guid ESP_GUID = { 0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B,
    { 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } 
};
const Guid LINUX_DATA_GUID = {
    0x0FC63DAF, 0x8483, 0x4772, 0x8E, 0x79,
    { 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 } 
};


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

// FAT32 Directory Entry (Short name ver)
typedef struct {
    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LastAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} __attribute__ ((packed)) FAT32_Dir_Entry_Short;

// FAT32 Directory Entry Attributes
typedef enum {
    ATTR_READ_ONLY  = 0x01,
    ATTR_HIDDEN     = 0x02,
    ATTR_SYSTEM     = 0x04,
    ATTR_VOLUME_ID  = 0x08,
    ATTR_DIRECTORY  = 0x10,
    ATTR_ARCHIVE    = 0x20,
    ATTR_LONG_NAME  = ATTR_READ_ONLY    | ATTR_HIDDEN |
                      ATTR_SYSTEM       | ATTR_VOLUME_ID, 
} FAT32_Dir_Attr;

//===================================================================================
//===================================================================================

// Convert bytes to LBAs
uint64_t bytes_to_lbas(const uint64_t bytes) {
    return (bytes / LBA_SIZE) + (bytes % LBA_SIZE > 0 ? 1 : 0); 
    // add extra lba in case of partial byte count
}

// Pad out 0s for bigger lbas
void write_full_lba_size(FILE* image) {
    uint64_t lba_size = LBA_SIZE;
    uint8_t zero_sector[512] = {0};
    for (uint8_t i = 0; i < (lba_size - sizeof zero_sector) / sizeof zero_sector; i++) {
        fwrite(zero_sector, sizeof zero_sector, 1, image);
    }
}

uint64_t next_aligned_lba(uint64_t lba) {
    return lba - (lba % align_lba) + align_lba;
}

//===================================================================================
//===================================================================================

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
    write_full_lba_size(image);
    
    return true;
}

//===================================================================================
//===================================================================================

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

//===================================================================================
//===================================================================================


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
    if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not write ESP VBR to image\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP FSInfo to image\n");
        return false;
    }
    write_full_lba_size(image);

    // go to backup boot sector location
    // write vbr and fsinfo at back up
    fseek(image, (esp_lba  + vbr.BPB_BkBootSec) * LBA_SIZE, SEEK_SET);
    if (fwrite(&vbr, 1, sizeof vbr, image) != sizeof vbr) {
        fprintf(stderr, "Error: Could not write VBR to image\n");
        return false;
    }
    write_full_lba_size(image);

    if (fwrite(&fsinfo, 1, sizeof fsinfo, image) != sizeof fsinfo) {
        fprintf(stderr, "Error: Could not write ESP FSInfo to image\n");
        return false;
    }
    write_full_lba_size(image);
    
    // write FATs
    const uint32_t fat_lba = esp_lba + vbr.BPB_RsvdSecCnt;
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
    FAT32_Dir_Entry_Short dir_ent = {
        .DIR_Name = { "EFI        " },
        .DIR_Attr = ATTR_DIRECTORY,
        .DIR_NTRes = 0,
        .DIR_CrtTimeTenth = 0,
        .DIR_CrtTime = 0,
        .DIR_CrtDate = 0,
        .DIR_LastAccDate = 0,   // Last access date
        .DIR_FstClusHI = 0,     // First cluster high
        .DIR_WrtTime = 0,
        .DIR_WrtDate = 0,
        .DIR_FstClusLO = 3,     // First cluster low
        .DIR_FileSize = 0,      // Directories have 0 file size
    };

    uint16_t create_time = 0, create_date = 0;
    get_fat_dir_entry_time_date(&create_time, &create_date);

    dir_ent.DIR_CrtTime = create_time;
    dir_ent.DIR_CrtTime = create_time;
    dir_ent.DIR_WrtTime = create_time;
    dir_ent.DIR_WrtDate = create_date;

    fwrite(&dir_ent, sizeof dir_ent, 1, image);
    
    // "/EFI" directory entries
    fseek(image, (data_lba + 1) * LBA_SIZE, SEEK_SET);
    
    memcpy(dir_ent.DIR_Name, ".          ", 11);    // "." dir entry, this directory itself
    fwrite(&dir_ent, sizeof dir_ent, 1, image);
    
    memcpy(dir_ent.DIR_Name, "..         ", 11);    // ".." dir entry, parent dir (ROOT dir)
    dir_ent.DIR_FstClusLO = 0;                      // Root directory does not have a cluster value
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    memcpy(dir_ent.DIR_Name, "BOOT       ", 11);    // /EFI/BOOT directory
    dir_ent.DIR_FstClusLO = 4;
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    // "/EFI/BOOT" directory
    fseek(image, (data_lba + 2) * LBA_SIZE, SEEK_SET);
    
    memcpy(dir_ent.DIR_Name, ".          ", 11);    // "." dir entry, this directory itself
    fwrite(&dir_ent, sizeof dir_ent, 1, image);
    
    memcpy(dir_ent.DIR_Name, "..         ", 11);    // ".." dir entry, parent dir (/EFI dir)
    dir_ent.DIR_FstClusLO = 3;                      // EFI directory cluster
    fwrite(&dir_ent, sizeof dir_ent, 1, image);

    return true;
}

bool add_path_to_esp() {
    return true;
}

//===================================================================================
// Main 
//===================================================================================
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

    // TODO:
    // Better way that uses less paramaters and I don't have to pass in &esp_lba
    // Write GPT headers & tables
    if (!write_gpt(image, image_size_lbas)) {
        fprintf(stderr, "Error: could not write GPT headers & tables for file %s\n", image_name);
        return EXIT_FAILURE;
    }

    // reset esp lba after gpt calculations
    // please fix TODO: this is so trash

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
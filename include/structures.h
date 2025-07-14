#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdint.h>
#include <uchar.h>

typedef struct {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_lba;
} __attribute__ ((packed)) Mbr_Partition;

typedef struct {
    uint8_t boot_code[440];
    uint32_t mbr_signature;
    uint16_t unkown;
    Mbr_Partition partition[4];
    uint16_t boot_signature;
} __attribute__ ((packed)) Mbr;

typedef struct {
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_ver;
    uint8_t clock_seq_hi_and_res;
    uint8_t clock_seq_low;
    uint8_t node[6];
} __attribute__ ((packed)) Guid;

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
    char16_t name[36];
} __attribute__ ((packed)) Gpt_Partition_Entry;

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
    uint16_t    bootsect_sig;
} __attribute__ ((packed)) Vbr;

typedef struct {
    uint32_t FSI_LeadSig;
    uint8_t FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    uint32_t FSI_Next_Free;
    uint8_t FSI_Reserved2[12];
    uint32_t FSI_TrailSig;
} __attribute__ ((packed)) FSInfo;

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

extern const Guid ESP_GUID;
extern const Guid LINUX_DATA_GUID;

#endif
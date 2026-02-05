/* toastOS FAT16 Filesystem */
#ifndef FAT16_H
#define FAT16_H

#include "stdint.h"

/* FAT16 Boot Sector / BIOS Parameter Block */
typedef struct {
    uint8_t  jmp[3];              /* Jump instruction */
    uint8_t  oem_name[8];         /* OEM name */
    uint16_t bytes_per_sector;    /* Usually 512 */
    uint8_t  sectors_per_cluster; /* Usually 1, 2, 4, 8, 16, 32, 64 */
    uint16_t reserved_sectors;    /* Usually 1 for FAT16 */
    uint8_t  fat_count;           /* Number of FATs (usually 2) */
    uint16_t root_entry_count;    /* Max entries in root dir (usually 512) */
    uint16_t total_sectors_16;    /* Total sectors (if < 65536) */
    uint8_t  media_type;          /* Media descriptor */
    uint16_t sectors_per_fat;     /* Sectors per FAT */
    uint16_t sectors_per_track;   /* Sectors per track */
    uint16_t head_count;          /* Number of heads */
    uint32_t hidden_sectors;      /* Hidden sectors before partition */
    uint32_t total_sectors_32;    /* Total sectors (if >= 65536) */
    
    /* Extended Boot Record */
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;      /* 0x29 if following fields are valid */
    uint32_t volume_serial;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];          /* "FAT16   " */
} __attribute__((packed)) fat16_bpb_t;

/* FAT16 Directory Entry (32 bytes) */
typedef struct {
    uint8_t  filename[8];         /* Filename (space padded) */
    uint8_t  extension[3];        /* Extension (space padded) */
    uint8_t  attributes;          /* File attributes */
    uint8_t  reserved;
    uint8_t  create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;    /* Always 0 for FAT16 */
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster;       /* First cluster of file */
    uint32_t file_size;           /* File size in bytes */
} __attribute__((packed)) fat16_dir_entry_t;

/* File attributes */
#define FAT16_ATTR_READ_ONLY  0x01
#define FAT16_ATTR_HIDDEN     0x02
#define FAT16_ATTR_SYSTEM     0x04
#define FAT16_ATTR_VOLUME_ID  0x08
#define FAT16_ATTR_DIRECTORY  0x10
#define FAT16_ATTR_ARCHIVE    0x20
#define FAT16_ATTR_LFN        0x0F  /* Long filename entry */

/* FAT16 cluster values */
#define FAT16_FREE_CLUSTER    0x0000
#define FAT16_RESERVED        0x0001
#define FAT16_BAD_CLUSTER     0xFFF7
#define FAT16_END_OF_CHAIN    0xFFF8  /* 0xFFF8-0xFFFF = end of chain */

/* toastOS FAT16 configuration */
#define FAT16_PARTITION_LBA   2048    /* Start at sector 2048 (1MB offset) */
#define FAT16_MAX_FILENAME    12      /* 8.3 format + null terminator */

/* File handle structure */
typedef struct {
    uint16_t first_cluster;
    uint32_t file_size;
    uint32_t current_pos;
    uint16_t current_cluster;
    uint8_t  is_open;
    uint8_t  is_dir;
    char     name[FAT16_MAX_FILENAME];
} fat16_file_t;

/* Function declarations */
int fat16_init(void);
int fat16_format(void);
int fat16_create_file(const char* filename, const char* content);
int fat16_read_file(const char* filename, char* buffer, uint32_t max_size);
int fat16_delete_file(const char* filename);
int fat16_list_files(void);
int fat16_file_exists(const char* filename);

#endif /* FAT16_H */

/* toastOS Bootloader Definitions */
#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "stdint.h"

#define SECTOR_SIZE       512
#define MBR_SECTOR        0
#define STAGE2_SECTOR     1
#define BOOT_INFO_SECTOR  2
#define KERNEL_START_SECT 64
#define FS_START_SECT     2048
#define KERNEL_LOAD_ADDR  0x100000

/* Boot info structure stored at sector 2 */
typedef struct {
    uint32_t magic;          /* 0x544F4153 = "TOAS" */
    uint32_t kernel_sectors; /* Number of sectors kernel occupies */
    uint32_t kernel_size;    /* Kernel size in bytes */
    uint32_t kernel_entry;   /* Entry point (0x100000) */
    uint32_t checksum;       /* Simple checksum of kernel */
    char     version[32];    /* "toastOS v1.x" */
} __attribute__((packed)) boot_info_t;

/* Bootloader functions */
void create_mbr_bootloader(uint8_t* buffer);
void create_stage2_bootloader(uint8_t* buffer);
uint32_t calculate_checksum(const uint8_t* data, uint32_t size);

#endif /* BOOTLOADER_H */

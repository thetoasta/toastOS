/* toastOS ATA/IDE Disk Driver */
#ifndef ATA_H
#define ATA_H

#include "stdint.h"

/* ATA I/O Ports (Primary Bus) */
#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERROR        0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_CTRL         0x3F6

/* ATA Commands */
#define ATA_CMD_READ_SECTORS     0x20
#define ATA_CMD_WRITE_SECTORS    0x30
#define ATA_CMD_IDENTIFY         0xEC
#define ATA_CMD_FLUSH            0xE7

/* ATA Status Bits */
#define ATA_SR_BSY               0x80    /* Busy */
#define ATA_SR_DRDY              0x40    /* Drive ready */
#define ATA_SR_DRQ               0x08    /* Data request ready */
#define ATA_SR_ERR               0x01    /* Error */

/* Drive selection */
#define ATA_MASTER               0xE0
#define ATA_SLAVE                0xF0

/* Sector size */
#define ATA_SECTOR_SIZE          512

/* Disk information returned by ata_get_disk_info() */
typedef struct {
    char     model[41];      /* Model string (40 chars + NUL) */
    char     type[16];       /* "ATA" or "Unknown" */
    uint32_t size_mb;        /* Total size in MB */
    uint32_t total_sectors;  /* LBA28 sector count */
} disk_info_t;

/* Function declarations */
int ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t sector_count, void* buffer);
int ata_write_sectors(uint32_t lba, uint8_t sector_count, const void* buffer);
int ata_identify(void);
int ata_erase_sectors(uint32_t start_lba, uint32_t count);

/* Query disk model, type and capacity.  Returns 0 on success, -1 if no drive. */
int ata_get_disk_info(disk_info_t *info);

#endif /* ATA_H */

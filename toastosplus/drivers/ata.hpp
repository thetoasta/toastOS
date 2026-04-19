/*
 * toastOS++ ATA/IDE Disk Driver
 * Namespace: toast::disk
 */

#ifndef ATA_HPP
#define ATA_HPP

#include "stdint.hpp"

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
#define ATA_SR_BSY               0x80
#define ATA_SR_DRDY              0x40
#define ATA_SR_DRQ               0x08
#define ATA_SR_ERR               0x01

/* Drive selection */
#define ATA_MASTER               0xE0
#define ATA_SLAVE                0xF0

/* Sector size */
#define ATA_SECTOR_SIZE          512

namespace toast {
namespace disk {

struct Info {
    char     model[41];
    char     type[16];
    uint32_t size_mb;
    uint32_t total_sectors;
};

int init();
int identify();
int read(uint32_t lba, uint8_t sector_count, void* buffer);
int write(uint32_t lba, uint8_t sector_count, const void* buffer);
int erase(uint32_t start_lba, uint32_t count);
int info(Info* out);

} // namespace disk
} // namespace toast

/* Legacy C-style type alias */
typedef toast::disk::Info disk_info_t;

/* Legacy C-style aliases */
inline int ata_init() { return toast::disk::init(); }
inline int ata_identify() { return toast::disk::identify(); }
inline int ata_read_sectors(uint32_t lba, uint8_t cnt, void* buf) { return toast::disk::read(lba, cnt, buf); }
inline int ata_write_sectors(uint32_t lba, uint8_t cnt, const void* buf) { return toast::disk::write(lba, cnt, buf); }
inline int ata_erase_sectors(uint32_t lba, uint32_t cnt) { return toast::disk::erase(lba, cnt); }
inline int ata_get_disk_info(disk_info_t* i) { return toast::disk::info(i); }

#endif /* ATA_HPP */

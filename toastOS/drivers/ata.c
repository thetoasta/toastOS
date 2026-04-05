/* toastOS ATA/IDE Disk Driver */

#include "ata.h"
#include "kio.h"
#include "funcs.h"

/* Port I/O functions */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Wait for drive to be ready (not busy) */
static int ata_wait_bsy(void) {
    int timeout = 100000;
    while ((inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) && timeout > 0) {
        timeout--;
    }
    return (timeout > 0) ? 0 : -1;
}

/* Wait for drive to be ready for data transfer */
static int ata_wait_drq(void) {
    int timeout = 100000;
    uint8_t status;
    while (timeout > 0) {
        status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DRQ) return 0;
        timeout--;
    }
    return -1;
}

/* 400ns delay by reading status port */
static void ata_delay(void) {
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
}

/* Initialize ATA driver */
int ata_init(void) {
    /* Soft reset */
    outb(ATA_PRIMARY_CTRL, 0x04);
    ata_delay();
    outb(ATA_PRIMARY_CTRL, 0x00);
    ata_delay();
    
    if (ata_wait_bsy() < 0) {
        kprint("[ATA] Timeout waiting for drive");
        kprint_newline();
        return -1;
    }
    
    kprint("[ATA] ATA driver initialized");
    kprint_newline();
    return 0;
}

/* Identify drive - check if drive exists */
int ata_identify(void) {
    /* Select master drive */
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER);
    ata_delay();
    
    /* Clear sector count and LBA registers */
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    
    /* Send IDENTIFY command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay();
    
    /* Check if drive exists */
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        kprint("[ATA] No drive detected");
        kprint_newline();
        return -1;
    }
    
    /* Wait for BSY to clear */
    if (ata_wait_bsy() < 0) {
        kprint("[ATA] Timeout on IDENTIFY");
        kprint_newline();
        return -1;
    }
    
    /* Check for ATAPI (not supported) */
    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) {
        kprint("[ATA] ATAPI device (not supported)");
        kprint_newline();
        return -1;
    }
    
    /* Wait for DRQ or ERR */
    if (ata_wait_drq() < 0) {
        kprint("[ATA] Error on IDENTIFY");
        kprint_newline();
        return -1;
    }
    
    /* Read and discard identify data (256 words) */
    for (int i = 0; i < 256; i++) {
        inw(ATA_PRIMARY_DATA);
    }
    
    kprint("[ATA] Drive detected and ready");
    kprint_newline();
    return 0;
}

/* Read sectors from disk using LBA28 */
int ata_read_sectors(uint32_t lba, uint8_t sector_count, void* buffer) {
    if (sector_count == 0) return -1;
    
    uint16_t* buf = (uint16_t*)buffer;
    
    /* Wait for drive */
    if (ata_wait_bsy() < 0) return -1;
    
    /* Select drive and set high LBA bits */
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER | ((lba >> 24) & 0x0F));
    ata_delay();
    
    /* Set sector count */
    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    
    /* Set LBA address */
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    
    /* Send read command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    /* Read each sector */
    for (int s = 0; s < sector_count; s++) {
        /* Wait for data */
        if (ata_wait_drq() < 0) return -1;
        
        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ATA_PRIMARY_DATA);
        }
        
        ata_delay();
    }
    
    return 0;
}

/* Write sectors to disk using LBA28 */
int ata_write_sectors(uint32_t lba, uint8_t sector_count, const void* buffer) {
    if (sector_count == 0) return -1;
    
    const uint16_t* buf = (const uint16_t*)buffer;
    
    /* Wait for drive */
    if (ata_wait_bsy() < 0) return -1;
    
    /* Select drive and set high LBA bits */
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER | ((lba >> 24) & 0x0F));
    ata_delay();
    
    /* Set sector count */
    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    
    /* Set LBA address */
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    
    /* Send write command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    /* Write each sector */
    for (int s = 0; s < sector_count; s++) {
        /* Wait for drive ready */
        if (ata_wait_drq() < 0) return -1;
        
        /* Write 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, buf[s * 256 + i]);
        }
        
        ata_delay();
    }
    
    /* Flush cache */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_FLUSH);
    if (ata_wait_bsy() < 0) return -1;
    
    return 0;
}

/* Erase disk by writing zeros to specified sectors */
int ata_erase_sectors(uint32_t start_lba, uint32_t count) {
    static uint8_t zero_buffer[512];
    
    /* Create zero buffer */
    for (int i = 0; i < 512; i++) {
        zero_buffer[i] = 0;
    }
    
    /* Write zeros to each sector */
    for (uint32_t i = 0; i < count; i++) {
        if (ata_write_sectors(start_lba + i, 1, zero_buffer) < 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Query disk model name, type and capacity via ATA IDENTIFY */
int ata_get_disk_info(disk_info_t *info) {
    /* Zero out the struct */
    uint8_t *p = (uint8_t *)info;
    for (int i = 0; i < (int)sizeof(disk_info_t); i++) p[i] = 0;

    /* Select master drive */
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER);
    ata_delay();
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) return -1;
    if (ata_wait_bsy() < 0) return -1;
    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) return -1;
    if (ata_wait_drq() < 0) return -1;

    /* Read the 256-word IDENTIFY block */
    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(ATA_PRIMARY_DATA);

    /* Words 27-46: model string (40 ASCII chars, byte-swapped per word) */
    for (int i = 0; i < 20; i++) {
        info->model[i * 2]     = (char)(id[27 + i] >> 8);
        info->model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    info->model[40] = '\0';
    /* Trim trailing spaces */
    for (int i = 39; i >= 0 && info->model[i] == ' '; i--)
        info->model[i] = '\0';

    /* Disk type */
    info->type[0] = 'A'; info->type[1] = 'T'; info->type[2] = 'A';
    info->type[3] = '\0';

    /* Words 60-61: total addressable LBA28 sectors */
    info->total_sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    info->size_mb = info->total_sectors / 2048; /* sectors * 512 / 1048576 */

    return 0;
}

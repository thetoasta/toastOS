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

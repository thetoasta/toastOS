/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#include "disk.h"
#include "kio.h"
#include "funcs.h"

// ATA PIO Mode ports
#define ATA_PRIMARY_DATA       0x1F0
#define ATA_PRIMARY_ERROR      0x1F1
#define ATA_PRIMARY_SECCOUNT   0x1F2
#define ATA_PRIMARY_LBA_LOW    0x1F3
#define ATA_PRIMARY_LBA_MID    0x1F4
#define ATA_PRIMARY_LBA_HIGH   0x1F5
#define ATA_PRIMARY_DRIVE_HEAD 0x1F6
#define ATA_PRIMARY_STATUS     0x1F7
#define ATA_PRIMARY_COMMAND    0x1F7

// ATA commands
#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30

// ATA status bits
#define ATA_SR_BSY  0x80  // Busy
#define ATA_SR_DRDY 0x40  // Drive ready
#define ATA_SR_DRQ  0x08  // Data request ready
#define ATA_SR_ERR  0x01  // Error

static int disk_available = 0;

// Wait for disk to be ready
static int disk_wait_ready(void) {
    uint8_t status;
    int timeout = 100000;
    
    while (timeout-- > 0) {
        status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) {
            return 0;
        }
    }
    return -1;
}

// Wait for data request
static int disk_wait_drq(void) {
    uint8_t status;
    int timeout = 100000;
    
    while (timeout-- > 0) {
        status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_SR_DRQ) {
            return 0;
        }
        if (status & ATA_SR_ERR) {
            return -1;
        }
    }
    return -1;
}

void disk_init(void) {
    // Try to detect disk
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0); // Select master drive
    
    if (disk_wait_ready() == 0) {
        disk_available = 1;
        kprint("[DISK] Primary ATA disk detected");
        kprint_newline();
    } else {
        disk_available = 0;
        kprint("[DISK] No disk detected - persistence disabled");
        kprint_newline();
    }
}

int disk_read_sector(uint32_t sector, uint8_t* buffer) {
    if (!disk_available) {
        return -1;
    }
    
    // Wait for disk to be ready
    if (disk_wait_ready() != 0) {
        return -1;
    }
    
    // Send read command
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((sector >> 24) & 0x0F)); // LBA mode, master, high 4 bits of LBA
    outb(ATA_PRIMARY_SECCOUNT, 1);                                  // Read 1 sector
    outb(ATA_PRIMARY_LBA_LOW, sector & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (sector >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HIGH, (sector >> 16) & 0xFF);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    
    // Wait for data to be ready
    if (disk_wait_drq() != 0) {
        return -1;
    }
    
    // Read data (SECTOR_SIZE bytes = 256 words)
    uint16_t* word_buffer = (uint16_t*)buffer;
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        word_buffer[i] = inw(ATA_PRIMARY_DATA);
    }
    
    return 0;
}

int disk_write_sector(uint32_t sector, const uint8_t* buffer) {
    if (!disk_available) {
        return -1;
    }
    
    // Wait for disk to be ready
    if (disk_wait_ready() != 0) {
        return -1;
    }
    
    // Send write command
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((sector >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LOW, sector & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (sector >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HIGH, (sector >> 16) & 0xFF);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);
    
    // Wait for data request
    if (disk_wait_drq() != 0) {
        return -1;
    }
    
    // Write data (SECTOR_SIZE bytes = 256 words)
    const uint16_t* word_buffer = (const uint16_t*)buffer;
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        outw(ATA_PRIMARY_DATA, word_buffer[i]);
    }
    
    // Flush cache
    outb(ATA_PRIMARY_COMMAND, 0xE7); // Cache flush command
    disk_wait_ready();
    
    return 0;
}

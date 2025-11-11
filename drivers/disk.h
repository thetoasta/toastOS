/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#ifndef DISK_H
#define DISK_H

#include "stdint.h"

#define SECTOR_SIZE 512
#define REGISTRY_SECTOR 1       // Sector 1 for registry data
#define FS_START_SECTOR 2       // Sectors 2+ for filesystem

// Initialize disk driver
void disk_init(void);

// Read a sector from disk
// sector: LBA sector number
// buffer: buffer to read into (must be at least SECTOR_SIZE bytes)
// returns: 0 on success, -1 on error
int disk_read_sector(uint32_t sector, uint8_t* buffer);

// Write a sector to disk
// sector: LBA sector number
// buffer: buffer to write from (must be at least SECTOR_SIZE bytes)
// returns: 0 on success, -1 on error
int disk_write_sector(uint32_t sector, const uint8_t* buffer);

#endif // DISK_H

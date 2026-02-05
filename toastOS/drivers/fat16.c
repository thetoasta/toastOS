/* toastOS FAT16 Filesystem Implementation */

#include "fat16.h"
#include "ata.h"
#include "kio.h"
#include "funcs.h"
#include "string.h"

/* Cached BPB info */
static fat16_bpb_t bpb;
static uint32_t fat_start_lba;
static uint32_t root_dir_start_lba;
static uint32_t data_start_lba;
static uint32_t root_dir_sectors;
static int fat16_initialized = 0;

/* Sector buffer */
static uint8_t sector_buffer[512];

/* Convert filename to FAT16 8.3 format */
static void to_fat16_name(const char* filename, uint8_t* fat_name) {
    int i, j;
    
    /* Initialize with spaces */
    for (i = 0; i < 11; i++) {
        fat_name[i] = ' ';
    }
    
    /* Copy name part (up to 8 chars) */
    for (i = 0, j = 0; filename[i] && filename[i] != '.' && j < 8; i++) {
        if (filename[i] >= 'a' && filename[i] <= 'z') {
            fat_name[j++] = filename[i] - 32;  /* To uppercase */
        } else {
            fat_name[j++] = filename[i];
        }
    }
    
    /* Find and copy extension */
    while (filename[i] && filename[i] != '.') i++;
    if (filename[i] == '.') {
        i++;
        for (j = 8; filename[i] && j < 11; i++) {
            if (filename[i] >= 'a' && filename[i] <= 'z') {
                fat_name[j++] = filename[i] - 32;
            } else {
                fat_name[j++] = filename[i];
            }
        }
    }
}

/* Compare FAT16 filename */
static int fat16_name_match(const uint8_t* fat_name, const char* filename) {
    uint8_t target[11];
    to_fat16_name(filename, target);
    for (int i = 0; i < 11; i++) {
        if (fat_name[i] != target[i]) return 0;
    }
    return 1;
}

/* Get cluster's LBA address */
static uint32_t cluster_to_lba(uint16_t cluster) {
    return data_start_lba + (cluster - 2) * bpb.sectors_per_cluster;
}

/* Read FAT entry for a cluster */
static uint16_t fat16_read_fat(uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    if (ata_read_sectors(fat_sector, 1, sector_buffer) < 0) {
        return FAT16_BAD_CLUSTER;
    }
    
    return *(uint16_t*)(&sector_buffer[entry_offset]);
}

/* Write FAT entry */
static int fat16_write_fat(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    /* Read sector */
    if (ata_read_sectors(fat_sector, 1, sector_buffer) < 0) return -1;
    
    /* Modify entry */
    *(uint16_t*)(&sector_buffer[entry_offset]) = value;
    
    /* Write back to both FATs */
    if (ata_write_sectors(fat_sector, 1, sector_buffer) < 0) return -1;
    if (bpb.fat_count > 1) {
        uint32_t fat2_sector = fat_sector + bpb.sectors_per_fat;
        if (ata_write_sectors(fat2_sector, 1, sector_buffer) < 0) return -1;
    }
    
    return 0;
}

/* Find a free cluster */
static uint16_t fat16_find_free_cluster(void) {
    uint32_t total_clusters = (bpb.total_sectors_16 ? bpb.total_sectors_16 : bpb.total_sectors_32) 
                              / bpb.sectors_per_cluster;
    
    for (uint16_t cluster = 2; cluster < total_clusters && cluster < 0xFFF0; cluster++) {
        if (fat16_read_fat(cluster) == FAT16_FREE_CLUSTER) {
            return cluster;
        }
    }
    return 0;  /* No free cluster */
}

/* Initialize FAT16 - read and validate boot sector */
int fat16_init(void) {
    kprint("[FAT16] Initializing filesystem...");
    kprint_newline();
    
    /* Initialize ATA driver */
    if (ata_init() < 0) {
        kprint("[FAT16] ATA init failed");
        kprint_newline();
        return -1;
    }
    
    /* Check for drive */
    if (ata_identify() < 0) {
        kprint("[FAT16] No drive found");
        kprint_newline();
        return -1;
    }
    
    /* Read boot sector */
    if (ata_read_sectors(FAT16_PARTITION_LBA, 1, &bpb) < 0) {
        kprint("[FAT16] Failed to read boot sector");
        kprint_newline();
        return -1;
    }
    
    /* Validate FAT16 signature */
    if (bpb.bytes_per_sector != 512) {
        kprint("[FAT16] Invalid sector size or not formatted");
        kprint_newline();
        return -1;
    }
    
    /* Calculate important LBAs */
    fat_start_lba = FAT16_PARTITION_LBA + bpb.reserved_sectors;
    root_dir_sectors = ((bpb.root_entry_count * 32) + 511) / 512;
    root_dir_start_lba = fat_start_lba + (bpb.fat_count * bpb.sectors_per_fat);
    data_start_lba = root_dir_start_lba + root_dir_sectors;
    
    fat16_initialized = 1;
    
    kprint("[FAT16] Filesystem mounted successfully");
    kprint_newline();
    kprint("[FAT16] Sectors per cluster: ");
    print_num(bpb.sectors_per_cluster);
    kprint_newline();
    
    return 0;
}

/* Format disk as FAT16 */
int fat16_format(void) {
    kprint("[FAT16] Formatting disk...");
    kprint_newline();
    
    /* Initialize ATA if not done */
    if (ata_init() < 0) return -1;
    if (ata_identify() < 0) return -1;
    
    /* Clear sector buffer */
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    
    /* Create boot sector / BPB */
    fat16_bpb_t* new_bpb = (fat16_bpb_t*)sector_buffer;
    
    new_bpb->jmp[0] = 0xEB;
    new_bpb->jmp[1] = 0x3C;
    new_bpb->jmp[2] = 0x90;
    
    /* OEM name */
    const char* oem = "TOASTOS ";
    for (int i = 0; i < 8; i++) new_bpb->oem_name[i] = oem[i];
    
    new_bpb->bytes_per_sector = 512;
    new_bpb->sectors_per_cluster = 4;      /* 2KB clusters */
    new_bpb->reserved_sectors = 1;
    new_bpb->fat_count = 2;
    new_bpb->root_entry_count = 512;       /* 512 root dir entries */
    new_bpb->total_sectors_16 = 65535;     /* ~32MB partition */
    new_bpb->media_type = 0xF8;            /* Fixed disk */
    new_bpb->sectors_per_fat = 256;        /* 256 sectors per FAT */
    new_bpb->sectors_per_track = 63;
    new_bpb->head_count = 16;
    new_bpb->hidden_sectors = FAT16_PARTITION_LBA;
    new_bpb->total_sectors_32 = 0;
    
    new_bpb->drive_number = 0x80;
    new_bpb->reserved = 0;
    new_bpb->boot_signature = 0x29;
    new_bpb->volume_serial = 0x12345678;
    
    const char* label = "TOASTOS    ";
    for (int i = 0; i < 11; i++) new_bpb->volume_label[i] = label[i];
    
    const char* fstype = "FAT16   ";
    for (int i = 0; i < 8; i++) new_bpb->fs_type[i] = fstype[i];
    
    /* Boot signature at end of sector */
    sector_buffer[510] = 0x55;
    sector_buffer[511] = 0xAA;
    
    /* Write boot sector */
    if (ata_write_sectors(FAT16_PARTITION_LBA, 1, sector_buffer) < 0) {
        kprint("[FAT16] Failed to write boot sector");
        kprint_newline();
        return -1;
    }
    
    kprint("[FAT16] Boot sector written");
    kprint_newline();
    
    /* Calculate FAT locations */
    uint32_t fat1_start = FAT16_PARTITION_LBA + 1;
    uint32_t fat2_start = fat1_start + 256;
    uint32_t root_start = fat2_start + 256;
    
    /* Clear and initialize FAT */
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    
    /* First FAT sector has special entries */
    sector_buffer[0] = 0xF8;  /* Media type */
    sector_buffer[1] = 0xFF;
    sector_buffer[2] = 0xFF;  /* End of chain marker for cluster 1 */
    sector_buffer[3] = 0xFF;
    
    /* Write first FAT sector */
    if (ata_write_sectors(fat1_start, 1, sector_buffer) < 0) return -1;
    if (ata_write_sectors(fat2_start, 1, sector_buffer) < 0) return -1;
    
    /* Clear rest of FAT */
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    for (uint32_t s = 1; s < 256; s++) {
        if (ata_write_sectors(fat1_start + s, 1, sector_buffer) < 0) return -1;
        if (ata_write_sectors(fat2_start + s, 1, sector_buffer) < 0) return -1;
    }
    
    kprint("[FAT16] FAT tables initialized");
    kprint_newline();
    
    /* Clear root directory (32 sectors for 512 entries) */
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    for (uint32_t s = 0; s < 32; s++) {
        if (ata_write_sectors(root_start + s, 1, sector_buffer) < 0) return -1;
    }
    
    kprint("[FAT16] Root directory cleared");
    kprint_newline();
    kprint("[FAT16] Format complete!");
    kprint_newline();
    
    /* Re-initialize with new format */
    fat16_initialized = 0;
    return fat16_init();
}

/* Create a file with content */
int fat16_create_file(const char* filename, const char* content) {
    if (!fat16_initialized) {
        kprint("[FAT16] Filesystem not initialized");
        kprint_newline();
        return -1;
    }
    
    /* Check if file already exists */
    if (fat16_file_exists(filename)) {
        kprint("[FAT16] File already exists: ");
        kprint(filename);
        kprint_newline();
        return -1;
    }
    
    /* Calculate content size */
    uint32_t content_size = 0;
    while (content[content_size]) content_size++;
    
    /* Find free cluster for data */
    uint16_t first_cluster = 0;
    if (content_size > 0) {
        first_cluster = fat16_find_free_cluster();
        if (first_cluster == 0) {
            kprint("[FAT16] Disk full");
            kprint_newline();
            return -1;
        }
        
        /* Mark cluster as end of chain */
        if (fat16_write_fat(first_cluster, FAT16_END_OF_CHAIN) < 0) return -1;
        
        /* Write content to cluster */
        for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
        for (uint32_t i = 0; i < content_size && i < 512 * bpb.sectors_per_cluster; i++) {
            sector_buffer[i % 512] = content[i];
            if ((i % 512) == 511 || i == content_size - 1) {
                uint32_t sector_offset = i / 512;
                if (ata_write_sectors(cluster_to_lba(first_cluster) + sector_offset, 1, sector_buffer) < 0) {
                    return -1;
                }
                for (int j = 0; j < 512; j++) sector_buffer[j] = 0;
            }
        }
    }
    
    /* Find free directory entry */
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_read_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return -1;
        
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buffer;
        for (int e = 0; e < 16; e++) {  /* 16 entries per sector */
            if (entries[e].filename[0] == 0x00 || entries[e].filename[0] == 0xE5) {
                /* Free entry found */
                for (int i = 0; i < 32; i++) ((uint8_t*)&entries[e])[i] = 0;
                
                to_fat16_name(filename, entries[e].filename);
                entries[e].attributes = FAT16_ATTR_ARCHIVE;
                entries[e].first_cluster = first_cluster;
                entries[e].file_size = content_size;
                
                /* Write directory sector back */
                if (ata_write_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return -1;
                
                kprint("[FAT16] Created: ");
                kprint(filename);
                kprint_newline();
                return 0;
            }
        }
    }
    
    kprint("[FAT16] Root directory full");
    kprint_newline();
    return -1;
}

/* Read file into buffer */
int fat16_read_file(const char* filename, char* buffer, uint32_t max_size) {
    if (!fat16_initialized) return -1;
    
    /* Search root directory */
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_read_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return -1;
        
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buffer;
        for (int e = 0; e < 16; e++) {
            if (entries[e].filename[0] == 0x00) break;
            if (entries[e].filename[0] == 0xE5) continue;
            if (entries[e].attributes & FAT16_ATTR_VOLUME_ID) continue;
            
            if (fat16_name_match(entries[e].filename, filename)) {
                uint32_t file_size = entries[e].file_size;
                uint16_t cluster = entries[e].first_cluster;
                uint32_t bytes_read = 0;
                
                while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN && bytes_read < file_size && bytes_read < max_size) {
                    uint32_t cluster_lba = cluster_to_lba(cluster);
                    
                    for (int sec = 0; sec < bpb.sectors_per_cluster && bytes_read < file_size && bytes_read < max_size; sec++) {
                        if (ata_read_sectors(cluster_lba + sec, 1, sector_buffer) < 0) return -1;
                        
                        for (int i = 0; i < 512 && bytes_read < file_size && bytes_read < max_size; i++) {
                            buffer[bytes_read++] = sector_buffer[i];
                        }
                    }
                    
                    cluster = fat16_read_fat(cluster);
                }
                
                buffer[bytes_read] = '\0';
                return bytes_read;
            }
        }
    }
    
    return -1;  /* File not found */
}

/* Delete a file */
int fat16_delete_file(const char* filename) {
    if (!fat16_initialized) return -1;
    
    /* Search root directory */
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_read_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return -1;
        
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buffer;
        for (int e = 0; e < 16; e++) {
            if (entries[e].filename[0] == 0x00) break;
            if (entries[e].filename[0] == 0xE5) continue;
            
            if (fat16_name_match(entries[e].filename, filename)) {
                uint16_t cluster = entries[e].first_cluster;
                
                /* Free cluster chain */
                while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
                    uint16_t next = fat16_read_fat(cluster);
                    fat16_write_fat(cluster, FAT16_FREE_CLUSTER);
                    cluster = next;
                }
                
                /* Mark directory entry as deleted */
                entries[e].filename[0] = 0xE5;
                if (ata_write_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return -1;
                
                kprint("[FAT16] Deleted: ");
                kprint(filename);
                kprint_newline();
                return 0;
            }
        }
    }
    
    kprint("[FAT16] File not found: ");
    kprint(filename);
    kprint_newline();
    return -1;
}

/* List all files */
int fat16_list_files(void) {
    if (!fat16_initialized) {
        kprint("[FAT16] Filesystem not initialized");
        kprint_newline();
        return -1;
    }
    
    kprint("[FAT16] Directory listing:");
    kprint_newline();
    kprint("----------------------------------------");
    kprint_newline();
    
    int file_count = 0;
    
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_read_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return -1;
        
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buffer;
        for (int e = 0; e < 16; e++) {
            if (entries[e].filename[0] == 0x00) goto done;
            if (entries[e].filename[0] == 0xE5) continue;
            if (entries[e].attributes & FAT16_ATTR_VOLUME_ID) continue;
            
            /* Print filename */
            kprint("  ");
            for (int i = 0; i < 8; i++) {
                if (entries[e].filename[i] != ' ') {
                    char c = entries[e].filename[i];
                    if (c >= 'A' && c <= 'Z') c += 32;  /* lowercase */
                    char str[2] = {c, 0};
                    kprint(str);
                }
            }
            
            /* Print extension */
            if (entries[e].filename[8] != ' ') {
                kprint(".");
                for (int i = 8; i < 11; i++) {
                    if (entries[e].filename[i] != ' ') {
                        char c = entries[e].filename[i];
                        if (c >= 'A' && c <= 'Z') c += 32;
                        char str[2] = {c, 0};
                        kprint(str);
                    }
                }
            }
            
            /* Print size */
            kprint("  (");
            print_num(entries[e].file_size);
            kprint(" bytes)");
            
            if (entries[e].attributes & FAT16_ATTR_DIRECTORY) {
                kprint(" <DIR>");
            }
            
            kprint_newline();
            file_count++;
        }
    }

done:
    kprint("----------------------------------------");
    kprint_newline();
    kprint("  Total files: ");
    print_num(file_count);
    kprint_newline();
    
    return file_count;
}

/* Check if file exists */
int fat16_file_exists(const char* filename) {
    if (!fat16_initialized) return 0;
    
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_read_sectors(root_dir_start_lba + s, 1, sector_buffer) < 0) return 0;
        
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)sector_buffer;
        for (int e = 0; e < 16; e++) {
            if (entries[e].filename[0] == 0x00) return 0;
            if (entries[e].filename[0] == 0xE5) continue;
            if (entries[e].attributes & FAT16_ATTR_VOLUME_ID) continue;
            
            if (fat16_name_match(entries[e].filename, filename)) {
                return 1;
            }
        }
    }
    
    return 0;
}

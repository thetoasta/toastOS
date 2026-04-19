/* toastOS FAT16 Filesystem Implementation */

#include "fat16.hpp"
#include "ata.hpp"
#include "kio.hpp"
#include "funcs.hpp"
#include "string.hpp"
#include "time.hpp"

/* Cached BPB info */
static fat16_bpb_t bpb;
static uint32_t fat_start_lba;
static uint32_t root_dir_start_lba;
static uint32_t data_start_lba;
static uint32_t root_dir_sectors;
static int fat16_initialized = 0;

/* Current working directory cluster (0 = root) */
static uint16_t fat16_cwd = 0;
static char fat16_cwd_path[256] = "/";

/* Encode time_t into FAT16 date/time fields */
static uint16_t fat16_encode_time(time_t t) {
    return ((uint16_t)(t.second / 2)) |
           ((uint16_t)t.minute << 5) |
           ((uint16_t)t.hour << 11);
}

static uint16_t fat16_encode_date(time_t t) {
    int year = (int)t.year - 1980;
    if (year < 0) year = 0;
    return ((uint16_t)t.day) |
           ((uint16_t)t.month << 5) |
           ((uint16_t)year << 9);
}

static void fat16_stamp_entry(fat16_dir_entry_t* e) {
    time_t now = get_time();
    uint16_t ftime = fat16_encode_time(now);
    uint16_t fdate = fat16_encode_date(now);
    e->create_time = ftime;
    e->create_date = fdate;
    e->modify_time = ftime;
    e->modify_date = fdate;
    e->access_date = fdate;
}

/* Sector buffers — separate buffers prevent FAT and dir operations
   from clobbering each other when they share function calls. */
static uint8_t sector_buffer[512];     /* general / data I/O  */
static uint8_t fat_buffer[512];        /* FAT read/write only */
static uint8_t dir_buffer[512];        /* directory operations */

/* Convert filename to FAT16 8.3 format */
static void to_fat16_name(const char* filename, uint8_t* fat_name) {
    int i, j;
    
    /* Initialize with spaces */
    for (i = 0; i < 11; i++) {
        fat_name[i] = ' ';
    }

    /* Special-case "." and ".." — these are literal name bytes, not 8.3 */
    if (filename[0] == '.' && filename[1] == '\0') {
        fat_name[0] = '.';
        return;
    }
    if (filename[0] == '.' && filename[1] == '.' && filename[2] == '\0') {
        fat_name[0] = '.';
        fat_name[1] = '.';
        return;
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

/* Read FAT entry for a cluster (uses fat_buffer) */
static uint16_t fat16_read_fat(uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    if (ata_read_sectors(fat_sector, 1, fat_buffer) < 0) {
        return FAT16_BAD_CLUSTER;
    }
    
    return *(uint16_t*)(&fat_buffer[entry_offset]);
}

/* Write FAT entry (uses fat_buffer) */
static int fat16_write_fat(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    /* Read sector */
    if (ata_read_sectors(fat_sector, 1, fat_buffer) < 0) return -1;
    
    /* Modify entry */
    *(uint16_t*)(&fat_buffer[entry_offset]) = value;
    
    /* Write back to both FATs */
    if (ata_write_sectors(fat_sector, 1, fat_buffer) < 0) return -1;
    if (bpb.fat_count > 1) {
        uint32_t fat2_sector = fat_sector + bpb.sectors_per_fat;
        if (ata_write_sectors(fat2_sector, 1, fat_buffer) < 0) return -1;
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
                fat16_stamp_entry(&entries[e]);
                
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

/* Delete a file (uses dir_buffer so FAT ops don't clobber the dir sector) */
int fat16_delete_file(const char* filename) {
    if (!fat16_initialized) return -1;
    
    /* Search root directory */
    for (uint32_t s = 0; s < root_dir_sectors; s++) {
        if (ata_read_sectors(root_dir_start_lba + s, 1, dir_buffer) < 0) return -1;
        
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
        for (int e = 0; e < 16; e++) {
            if (entries[e].filename[0] == 0x00) break;
            if (entries[e].filename[0] == 0xE5) continue;
            
            if (fat16_name_match(entries[e].filename, filename)) {
                uint16_t cluster = entries[e].first_cluster;
                
                /* Free cluster chain (uses fat_buffer internally) */
                while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
                    uint16_t next = fat16_read_fat(cluster);
                    fat16_write_fat(cluster, FAT16_FREE_CLUSTER);
                    cluster = next;
                }
                
                /* Mark directory entry as deleted — dir_buffer is still intact */
                entries[e].filename[0] = 0xE5;
                if (ata_write_sectors(root_dir_start_lba + s, 1, dir_buffer) < 0) return -1;
                
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

            /* Print timestamp */
            if (entries[e].modify_date != 0) {
                kprint("  ");
                uint16_t md = entries[e].modify_date;
                uint16_t mt = entries[e].modify_time;
                print_num((md >> 5) & 0x0F); kprint("/");
                print_num(md & 0x1F); kprint("/");
                print_num(((md >> 9) & 0x7F) + 1980);
                kprint(" ");
                print_num((mt >> 11) & 0x1F); kprint(":");
                uint8_t mins = (mt >> 5) & 0x3F;
                if (mins < 10) kprint("0");
                print_num(mins);
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

/* =========================================================================
 * Directory support — path resolution, mkdir, path-aware file operations
 * ========================================================================= */

/* Maximum path depth we support */
#define FAT16_MAX_PATH_DEPTH 16
#define FAT16_PATH_SEP       '/'

/* Special cluster value meaning "root directory" */
#define FAT16_ROOT_CLUSTER   0

/*
 * Read directory entries from a given parent.
 * If parent_cluster == FAT16_ROOT_CLUSTER, reads from the root directory area.
 * Otherwise reads the cluster chain of a subdirectory.
 *
 * Calls `callback` for every valid (non-deleted, non-volume) entry.
 * The callback receives the entry pointer, the sector LBA it lives in,
 * and the index within that sector.  Return 1 from callback to stop early.
 */
typedef int (*dir_iter_cb)(fat16_dir_entry_t* entry, uint32_t sector_lba,
                           int entry_index, void* ctx);

static int iterate_dir(uint16_t parent_cluster, dir_iter_cb cb, void* ctx) {
    if (parent_cluster == FAT16_ROOT_CLUSTER) {
        /* Root directory — fixed area */
        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            uint32_t lba = root_dir_start_lba + s;
            if (ata_read_sectors(lba, 1, dir_buffer) < 0) return -1;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
            for (int e = 0; e < 16; e++) {
                if (entries[e].filename[0] == 0x00) return 0;
                if (entries[e].filename[0] == 0xE5) continue;
                if (entries[e].attributes & FAT16_ATTR_VOLUME_ID) continue;
                if (cb(&entries[e], lba, e, ctx)) return 1;
            }
        }
    } else {
        /* Subdirectory — follow cluster chain */
        uint16_t cluster = parent_cluster;
        while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
            uint32_t clust_lba = cluster_to_lba(cluster);
            for (int sec = 0; sec < bpb.sectors_per_cluster; sec++) {
                uint32_t lba = clust_lba + sec;
                if (ata_read_sectors(lba, 1, dir_buffer) < 0) return -1;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
                for (int e = 0; e < 16; e++) {
                    if (entries[e].filename[0] == 0x00) return 0;
                    if (entries[e].filename[0] == 0xE5) continue;
                    if (entries[e].attributes & FAT16_ATTR_VOLUME_ID) continue;
                    if (cb(&entries[e], lba, e, ctx)) return 1;
                }
            }
            cluster = fat16_read_fat(cluster);
        }
    }
    return 0;
}

/* --- helper: find a named entry inside a directory ---------------------- */
typedef struct {
    const char* name;
    fat16_dir_entry_t result;
    int found;
} find_ctx_t;

static int find_entry_cb(fat16_dir_entry_t* e, uint32_t lba, int idx, void* ctx) {
    find_ctx_t* fc = (find_ctx_t*)ctx;
    if (fat16_name_match(e->filename, fc->name)) {
        fc->result = *e;
        fc->found = 1;
        return 1;  /* stop */
    }
    return 0;
}

/*
 * Resolve a path like "docs/notes.txt" starting from root.
 * Sets *parent_cluster to the cluster of the final directory,
 * and *leaf to point at the leaf component (filename or last dir name).
 *
 * Returns 0 on success, -1 if an intermediate component is not found or
 * not a directory.
 */
static int resolve_path(const char* path, uint16_t* parent_cluster, const char** leaf) {
    static char pathbuf[256];
    int len = 0;
    while (path[len] && len < 255) { pathbuf[len] = path[len]; len++; }
    pathbuf[len] = '\0';

    /* Skip leading slash */
    int pos = 0;
    if (pathbuf[pos] == FAT16_PATH_SEP) pos++;

    uint16_t cur_cluster = FAT16_ROOT_CLUSTER;

    /* Walk each component except the last one */
    while (1) {
        /* Find next separator */
        int start = pos;
        while (pathbuf[pos] && pathbuf[pos] != FAT16_PATH_SEP) pos++;

        if (pathbuf[pos] == '\0') {
            /* This is the leaf component */
            *parent_cluster = cur_cluster;
            *leaf = path + start;
            return 0;
        }

        /* Intermediate component — must be a directory */
        pathbuf[pos] = '\0';
        find_ctx_t fc;
        fc.name = &pathbuf[start];
        fc.found = 0;
        iterate_dir(cur_cluster, find_entry_cb, &fc);
        if (!fc.found || !(fc.result.attributes & FAT16_ATTR_DIRECTORY)) {
            return -1;  /* not found or not a directory */
        }
        cur_cluster = fc.result.first_cluster;
        pos++;  /* skip past the separator */
    }
}

/* --- find a free dir entry in a parent directory and return its location - */
typedef struct {
    uint32_t sector_lba;
    int entry_index;
    int found;
} free_slot_ctx_t;

static int find_free_slot_cb(fat16_dir_entry_t* e, uint32_t lba, int idx, void* ctx) {
    (void)e; (void)lba; (void)idx;
    /* We're iterating valid entries — not what we want.
       We override below with a raw scan. */
    return 0;
}

/*
 * Scan a directory (root or sub) for a free entry slot.
 * Returns 0 on success and fills ctx, -1 on failure.
 */
static int find_free_dir_entry(uint16_t parent_cluster, free_slot_ctx_t* out) {
    out->found = 0;
    if (parent_cluster == FAT16_ROOT_CLUSTER) {
        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            uint32_t lba = root_dir_start_lba + s;
            if (ata_read_sectors(lba, 1, dir_buffer) < 0) return -1;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
            for (int e = 0; e < 16; e++) {
                if (entries[e].filename[0] == 0x00 || entries[e].filename[0] == 0xE5) {
                    out->sector_lba = lba;
                    out->entry_index = e;
                    out->found = 1;
                    return 0;
                }
            }
        }
    } else {
        uint16_t cluster = parent_cluster;
        while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
            uint32_t clust_lba = cluster_to_lba(cluster);
            for (int sec = 0; sec < bpb.sectors_per_cluster; sec++) {
                uint32_t lba = clust_lba + sec;
                if (ata_read_sectors(lba, 1, dir_buffer) < 0) return -1;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
                for (int e = 0; e < 16; e++) {
                    if (entries[e].filename[0] == 0x00 || entries[e].filename[0] == 0xE5) {
                        out->sector_lba = lba;
                        out->entry_index = e;
                        out->found = 1;
                        return 0;
                    }
                }
            }
            cluster = fat16_read_fat(cluster);
        }
    }
    return -1;
}

/* ---- fat16_mkdir ------------------------------------------------------- */
int fat16_mkdir(const char* path) {
    if (!fat16_initialized) {
        kprint("[FAT16] Filesystem not initialized");
        kprint_newline();
        return -1;
    }

    uint16_t parent_cluster;
    const char* dirname;
    if (resolve_path(path, &parent_cluster, &dirname) < 0) {
        kprint("[FAT16] Parent directory not found");
        kprint_newline();
        return -1;
    }

    /* Check if name already exists in parent */
    find_ctx_t fc;
    fc.name = dirname;
    fc.found = 0;
    iterate_dir(parent_cluster, find_entry_cb, &fc);
    if (fc.found) {
        kprint("[FAT16] Already exists: ");
        kprint(dirname);
        kprint_newline();
        return -1;
    }

    /* Allocate a cluster for the new directory's data */
    uint16_t dir_cluster = fat16_find_free_cluster();
    if (dir_cluster == 0) {
        kprint("[FAT16] Disk full");
        kprint_newline();
        return -1;
    }
    if (fat16_write_fat(dir_cluster, FAT16_END_OF_CHAIN) < 0) return -1;

    /* Clear the new cluster */
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    for (int sec = 0; sec < bpb.sectors_per_cluster; sec++) {
        if (ata_write_sectors(cluster_to_lba(dir_cluster) + sec, 1, sector_buffer) < 0)
            return -1;
    }

    /* Write "." and ".." entries in the first sector of the new cluster */
    if (ata_read_sectors(cluster_to_lba(dir_cluster), 1, sector_buffer) < 0) return -1;
    fat16_dir_entry_t* de = (fat16_dir_entry_t*)sector_buffer;

    /* "." entry — points to self */
    for (int i = 0; i < 32; i++) ((uint8_t*)&de[0])[i] = 0;
    for (int i = 0; i < 11; i++) de[0].filename[i] = ' ';
    de[0].filename[0] = '.';
    de[0].attributes = FAT16_ATTR_DIRECTORY;
    de[0].first_cluster = dir_cluster;
    fat16_stamp_entry(&de[0]);

    /* ".." entry — points to parent (0 = root) */
    for (int i = 0; i < 32; i++) ((uint8_t*)&de[1])[i] = 0;
    for (int i = 0; i < 11; i++) de[1].filename[i] = ' ';
    de[1].filename[0] = '.';
    de[1].filename[1] = '.';
    de[1].attributes = FAT16_ATTR_DIRECTORY;
    de[1].first_cluster = parent_cluster;  /* 0 for root */
    fat16_stamp_entry(&de[1]);

    if (ata_write_sectors(cluster_to_lba(dir_cluster), 1, sector_buffer) < 0) return -1;

    /* Add entry in parent directory */
    free_slot_ctx_t slot;
    if (find_free_dir_entry(parent_cluster, &slot) < 0 || !slot.found) {
        kprint("[FAT16] Directory full");
        kprint_newline();
        fat16_write_fat(dir_cluster, FAT16_FREE_CLUSTER);
        return -1;
    }

    /* Read that sector, fill entry, write back */
    if (ata_read_sectors(slot.sector_lba, 1, dir_buffer) < 0) return -1;
    fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
    fat16_dir_entry_t* new_entry = &entries[slot.entry_index];

    for (int i = 0; i < 32; i++) ((uint8_t*)new_entry)[i] = 0;
    to_fat16_name(dirname, new_entry->filename);
    new_entry->attributes = FAT16_ATTR_DIRECTORY;
    new_entry->first_cluster = dir_cluster;
    new_entry->file_size = 0;  /* directories have size 0 in FAT16 */
    fat16_stamp_entry(new_entry);

    if (ata_write_sectors(slot.sector_lba, 1, dir_buffer) < 0) return -1;

    kprint("[FAT16] Created directory: ");
    kprint(dirname);
    kprint_newline();
    return 0;
}

/* ---- fat16_list_dir — list entries at an arbitrary path ----------------- */

typedef struct {
    int count;
} list_ctx_t;

static int list_entry_cb(fat16_dir_entry_t* e, uint32_t lba, int idx, void* ctx) {
    list_ctx_t* lc = (list_ctx_t*)ctx;
    (void)lba; (void)idx;

    kprint("  ");
    for (int i = 0; i < 8; i++) {
        if (e->filename[i] != ' ') {
            char c = e->filename[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            char str[2] = {c, 0};
            kprint(str);
        }
    }
    if (e->filename[8] != ' ') {
        kprint(".");
        for (int i = 8; i < 11; i++) {
            if (e->filename[i] != ' ') {
                char c = e->filename[i];
                if (c >= 'A' && c <= 'Z') c += 32;
                char str[2] = {c, 0};
                kprint(str);
            }
        }
    }

    if (e->attributes & FAT16_ATTR_DIRECTORY) {
        kprint("  <DIR>");
    } else {
        kprint("  (");
        print_num(e->file_size);
        kprint(" bytes)");
    }

    /* Print timestamp */
    if (e->modify_date != 0) {
        kprint("  ");
        uint16_t md = e->modify_date;
        uint16_t mt = e->modify_time;
        print_num((md >> 5) & 0x0F); kprint("/");
        print_num(md & 0x1F); kprint("/");
        print_num(((md >> 9) & 0x7F) + 1980);
        kprint(" ");
        print_num((mt >> 11) & 0x1F); kprint(":");
        uint8_t mins = (mt >> 5) & 0x3F;
        if (mins < 10) kprint("0");
        print_num(mins);
    }

    kprint_newline();
    lc->count++;
    return 0;
}

int fat16_list_dir(const char* path) {
    if (!fat16_initialized) {
        kprint("[FAT16] Filesystem not initialized");
        kprint_newline();
        return -1;
    }

    uint16_t dir_cluster;

    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        /* Root directory */
        dir_cluster = FAT16_ROOT_CLUSTER;
    } else {
        /* Strip trailing slash for resolve_path */
        static char clean[256];
        int clen = 0;
        while (path[clen] && clen < 255) { clean[clen] = path[clen]; clen++; }
        clean[clen] = '\0';
        while (clen > 1 && clean[clen - 1] == '/') { clen--; clean[clen] = '\0'; }

        /* Re-check after stripping: might be just "/" */
        if (clean[0] == '/' && clean[1] == '\0') {
            dir_cluster = FAT16_ROOT_CLUSTER;
        } else {
            uint16_t parent;
            const char* leaf;
            if (resolve_path(clean, &parent, &leaf) < 0) {
                kprint("[FAT16] Path not found");
                kprint_newline();
                return -1;
            }
            find_ctx_t fc;
            fc.name = leaf;
            fc.found = 0;
            iterate_dir(parent, find_entry_cb, &fc);
            if (!fc.found || !(fc.result.attributes & FAT16_ATTR_DIRECTORY)) {
                kprint("[FAT16] Not a directory: ");
                kprint(path);
                kprint_newline();
                return -1;
            }
            dir_cluster = fc.result.first_cluster;
        }
    }

    kprint("[FAT16] Directory listing:");
    kprint_newline();
    kprint("----------------------------------------");
    kprint_newline();

    list_ctx_t lc;
    lc.count = 0;
    iterate_dir(dir_cluster, list_entry_cb, &lc);

    kprint("----------------------------------------");
    kprint_newline();
    kprint("  Total entries: ");
    print_num(lc.count);
    kprint_newline();
    return lc.count;
}

/* ---- fat16_create_file_at — create a file at a path like "dir/file.txt" */
int fat16_create_file_at(const char* path, const char* content) {
    if (!fat16_initialized) {
        kprint("[FAT16] Filesystem not initialized");
        kprint_newline();
        return -1;
    }

    uint16_t parent_cluster;
    const char* filename;
    if (resolve_path(path, &parent_cluster, &filename) < 0) {
        kprint("[FAT16] Parent directory not found");
        kprint_newline();
        return -1;
    }

    /* Check if already exists */
    find_ctx_t fc;
    fc.name = filename;
    fc.found = 0;
    iterate_dir(parent_cluster, find_entry_cb, &fc);
    if (fc.found) {
        kprint("[FAT16] File already exists: ");
        kprint(filename);
        kprint_newline();
        return -1;
    }

    /* Calculate content size */
    uint32_t content_size = 0;
    while (content[content_size]) content_size++;

    /* Allocate cluster if needed */
    uint16_t first_cluster = 0;
    if (content_size > 0) {
        first_cluster = fat16_find_free_cluster();
        if (first_cluster == 0) {
            kprint("[FAT16] Disk full");
            kprint_newline();
            return -1;
        }
        if (fat16_write_fat(first_cluster, FAT16_END_OF_CHAIN) < 0) return -1;

        for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
        for (uint32_t i = 0; i < content_size && i < 512 * (uint32_t)bpb.sectors_per_cluster; i++) {
            sector_buffer[i % 512] = content[i];
            if ((i % 512) == 511 || i == content_size - 1) {
                uint32_t sector_offset = i / 512;
                if (ata_write_sectors(cluster_to_lba(first_cluster) + sector_offset, 1, sector_buffer) < 0)
                    return -1;
                for (int j = 0; j < 512; j++) sector_buffer[j] = 0;
            }
        }
    }

    /* Find free slot in parent */
    free_slot_ctx_t slot;
    if (find_free_dir_entry(parent_cluster, &slot) < 0 || !slot.found) {
        kprint("[FAT16] Directory full");
        kprint_newline();
        return -1;
    }

    if (ata_read_sectors(slot.sector_lba, 1, dir_buffer) < 0) return -1;
    fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
    fat16_dir_entry_t* ne = &entries[slot.entry_index];

    for (int i = 0; i < 32; i++) ((uint8_t*)ne)[i] = 0;
    to_fat16_name(filename, ne->filename);
    ne->attributes = FAT16_ATTR_ARCHIVE;
    ne->first_cluster = first_cluster;
    ne->file_size = content_size;
    fat16_stamp_entry(ne);

    if (ata_write_sectors(slot.sector_lba, 1, dir_buffer) < 0) return -1;

    kprint("[FAT16] Created: ");
    kprint(filename);
    kprint_newline();
    return 0;
}

/* ---- fat16_read_file_at — read a file at a given path ------------------ */
int fat16_read_file_at(const char* path, char* buffer, uint32_t max_size) {
    if (!fat16_initialized) return -1;

    uint16_t parent_cluster;
    const char* filename;
    if (resolve_path(path, &parent_cluster, &filename) < 0) return -1;

    find_ctx_t fc;
    fc.name = filename;
    fc.found = 0;
    iterate_dir(parent_cluster, find_entry_cb, &fc);
    if (!fc.found || (fc.result.attributes & FAT16_ATTR_DIRECTORY)) return -1;

    uint32_t file_size = fc.result.file_size;
    uint16_t cluster = fc.result.first_cluster;
    uint32_t bytes_read = 0;

    while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN
           && bytes_read < file_size && bytes_read < max_size) {
        uint32_t clust_lba = cluster_to_lba(cluster);
        for (int sec = 0; sec < bpb.sectors_per_cluster
             && bytes_read < file_size && bytes_read < max_size; sec++) {
            if (ata_read_sectors(clust_lba + sec, 1, sector_buffer) < 0) return -1;
            for (int i = 0; i < 512 && bytes_read < file_size && bytes_read < max_size; i++) {
                buffer[bytes_read++] = sector_buffer[i];
            }
        }
        cluster = fat16_read_fat(cluster);
    }
    buffer[bytes_read] = '\0';
    return bytes_read;
}

/* ---- fat16_delete_at — delete a file or empty directory at a path ------ */
int fat16_delete_at(const char* path) {
    if (!fat16_initialized) return -1;

    uint16_t parent_cluster;
    const char* name;
    if (resolve_path(path, &parent_cluster, &name) < 0) return -1;

    /* Scan the parent dir for the entry — need sector + index to mark deleted */
    if (parent_cluster == FAT16_ROOT_CLUSTER) {
        for (uint32_t s = 0; s < root_dir_sectors; s++) {
            uint32_t lba = root_dir_start_lba + s;
            if (ata_read_sectors(lba, 1, dir_buffer) < 0) return -1;
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
            for (int e = 0; e < 16; e++) {
                if (entries[e].filename[0] == 0x00) goto not_found;
                if (entries[e].filename[0] == 0xE5) continue;
                if (fat16_name_match(entries[e].filename, name)) {
                    uint16_t cluster = entries[e].first_cluster;
                    while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
                        uint16_t next = fat16_read_fat(cluster);
                        fat16_write_fat(cluster, FAT16_FREE_CLUSTER);
                        cluster = next;
                    }
                    entries[e].filename[0] = 0xE5;
                    if (ata_write_sectors(lba, 1, dir_buffer) < 0) return -1;
                    kprint("[FAT16] Deleted: ");
                    kprint(name);
                    kprint_newline();
                    return 0;
                }
            }
        }
    } else {
        uint16_t cluster = parent_cluster;
        while (cluster >= 2 && cluster < FAT16_END_OF_CHAIN) {
            uint32_t clust_lba = cluster_to_lba(cluster);
            for (int sec = 0; sec < bpb.sectors_per_cluster; sec++) {
                uint32_t lba = clust_lba + sec;
                if (ata_read_sectors(lba, 1, dir_buffer) < 0) return -1;
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)dir_buffer;
                for (int e = 0; e < 16; e++) {
                    if (entries[e].filename[0] == 0x00) goto not_found;
                    if (entries[e].filename[0] == 0xE5) continue;
                    if (fat16_name_match(entries[e].filename, name)) {
                        uint16_t fc = entries[e].first_cluster;
                        while (fc >= 2 && fc < FAT16_END_OF_CHAIN) {
                            uint16_t next = fat16_read_fat(fc);
                            fat16_write_fat(fc, FAT16_FREE_CLUSTER);
                            fc = next;
                        }
                        entries[e].filename[0] = 0xE5;
                        if (ata_write_sectors(lba, 1, dir_buffer) < 0) return -1;
                        kprint("[FAT16] Deleted: ");
                        kprint(name);
                        kprint_newline();
                        return 0;
                    }
                }
            }
            cluster = fat16_read_fat(cluster);
        }
    }

not_found:
    kprint("[FAT16] Not found: ");
    kprint(name);
    kprint_newline();
    return -1;
}

/* ---- fat16_file_exists_at — check if a file/dir exists at a path ------- */
int fat16_file_exists_at(const char* path) {
    if (!fat16_initialized) return 0;

    uint16_t parent_cluster;
    const char* name;
    if (resolve_path(path, &parent_cluster, &name) < 0) return 0;

    find_ctx_t fc;
    fc.name = name;
    fc.found = 0;
    iterate_dir(parent_cluster, find_entry_cb, &fc);
    return fc.found;
}

/* ---- Working directory ------------------------------------------------- */

const char* fat16_getcwd(void) {
    return fat16_cwd_path;
}

uint16_t fat16_get_cwd_cluster(void) {
    return fat16_cwd;
}

int fat16_chdir(const char* path) {
    if (!fat16_initialized) {
        kprint("[FAT16] Filesystem not initialized");
        kprint_newline();
        return -1;
    }

    if (!path || path[0] == '\0') {
        fat16_cwd = FAT16_ROOT_CLUSTER;
        fat16_cwd_path[0] = '/';
        fat16_cwd_path[1] = '\0';
        return 0;
    }

    /* "cd /" — go to root */
    if (path[0] == '/' && path[1] == '\0') {
        fat16_cwd = FAT16_ROOT_CLUSTER;
        fat16_cwd_path[0] = '/';
        fat16_cwd_path[1] = '\0';
        return 0;
    }

    /* "cd .." — go to parent */
    if (path[0] == '.' && path[1] == '.' && path[2] == '\0') {
        if (fat16_cwd == FAT16_ROOT_CLUSTER) return 0;

        find_ctx_t fc;
        fc.name = "..";
        fc.found = 0;
        iterate_dir(fat16_cwd, find_entry_cb, &fc);
        if (!fc.found) return -1;

        fat16_cwd = fc.result.first_cluster;

        /* Strip last component from cwd_path */
        int len = 0;
        while (fat16_cwd_path[len]) len++;
        if (len > 1 && fat16_cwd_path[len - 1] == '/') len--;
        while (len > 1 && fat16_cwd_path[len - 1] != '/') len--;
        if (len == 0) len = 1;
        fat16_cwd_path[len] = '\0';
        return 0;
    }

    /* Absolute path — walk from root */
    if (path[0] == '/') {
        uint16_t parent;
        const char* leaf;
        if (resolve_path(path, &parent, &leaf) < 0) {
            kprint("[FAT16] Path not found");
            kprint_newline();
            return -1;
        }
        find_ctx_t fc;
        fc.name = leaf;
        fc.found = 0;
        iterate_dir(parent, find_entry_cb, &fc);
        if (!fc.found || !(fc.result.attributes & FAT16_ATTR_DIRECTORY)) {
            kprint("[FAT16] Not a directory: ");
            kprint(path);
            kprint_newline();
            return -1;
        }
        fat16_cwd = fc.result.first_cluster;

        /* Build cwd path: copy path then ensure trailing slash */
        int i = 0;
        while (path[i] && i < 254) { fat16_cwd_path[i] = path[i]; i++; }
        if (i > 1 && fat16_cwd_path[i - 1] != '/') fat16_cwd_path[i++] = '/';
        fat16_cwd_path[i] = '\0';
        return 0;
    }

    /* Relative single-component name — look up directly in cwd */
    find_ctx_t fc;
    fc.name = path;
    fc.found = 0;
    iterate_dir(fat16_cwd, find_entry_cb, &fc);
    if (!fc.found || !(fc.result.attributes & FAT16_ATTR_DIRECTORY)) {
        kprint("[FAT16] Not a directory: ");
        kprint(path);
        kprint_newline();
        return -1;
    }
    fat16_cwd = fc.result.first_cluster;

    /* Append name to cwd_path */
    int i = 0;
    while (fat16_cwd_path[i]) i++;
    if (i > 0 && fat16_cwd_path[i - 1] != '/') fat16_cwd_path[i++] = '/';
    int pi = 0;
    while (path[pi] && i < 254) fat16_cwd_path[i++] = path[pi++];
    fat16_cwd_path[i++] = '/';
    fat16_cwd_path[i] = '\0';
    return 0;
}

/* ---- fat16_enumerate_dir — fill array of entries for POSIX dirent ------ */

typedef struct {
    fat16_enum_entry_t *out;
    int max;
    int count;
} enum_ctx_t;

static int enum_entry_cb(fat16_dir_entry_t* e, uint32_t lba, int idx, void* ctx) {
    enum_ctx_t *ec = (enum_ctx_t *)ctx;
    (void)lba; (void)idx;
    if (ec->count >= ec->max) return 1; /* stop */

    /* Skip . and .. entries for cleaner output */
    if (e->filename[0] == '.') {
        if (e->filename[1] == ' ' || e->filename[1] == '.') return 0;
    }

    fat16_enum_entry_t *out = &ec->out[ec->count];

    /* Build lowercase 8.3 name */
    int pos = 0;
    for (int i = 0; i < 8; i++) {
        if (e->filename[i] != ' ') {
            char c = e->filename[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            out->name[pos++] = c;
        }
    }
    if (e->extension[0] != ' ') {
        out->name[pos++] = '.';
        for (int i = 0; i < 3; i++) {
            if (e->extension[i] != ' ') {
                char c = e->extension[i];
                if (c >= 'A' && c <= 'Z') c += 32;
                out->name[pos++] = c;
            }
        }
    }
    out->name[pos] = '\0';
    out->is_dir    = (e->attributes & FAT16_ATTR_DIRECTORY) ? 1 : 0;
    out->file_size = e->file_size;
    ec->count++;
    return 0;
}

int fat16_enumerate_dir(const char *path, fat16_enum_entry_t *out, int max_entries) {
    if (!fat16_initialized || !out || max_entries <= 0) return -1;

    uint16_t dir_cluster;

    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        dir_cluster = FAT16_ROOT_CLUSTER;
    } else {
        static char clean[256];
        int clen = 0;
        while (path[clen] && clen < 255) { clean[clen] = path[clen]; clen++; }
        clean[clen] = '\0';
        while (clen > 1 && clean[clen - 1] == '/') { clen--; clean[clen] = '\0'; }

        if (clean[0] == '/' && clean[1] == '\0') {
            dir_cluster = FAT16_ROOT_CLUSTER;
        } else {
            uint16_t parent;
            const char *leaf;
            if (resolve_path(clean, &parent, &leaf) < 0) return -1;
            find_ctx_t fc;
            fc.name = leaf;
            fc.found = 0;
            iterate_dir(parent, find_entry_cb, &fc);
            if (!fc.found || !(fc.result.attributes & FAT16_ATTR_DIRECTORY))
                return -1;
            dir_cluster = fc.result.first_cluster;
        }
    }

    enum_ctx_t ec;
    ec.out   = out;
    ec.max   = max_entries;
    ec.count = 0;
    iterate_dir(dir_cluster, enum_entry_cb, &ec);
    return ec.count;
}


/* ========== toast::fs namespace implementations ========== */
namespace toast {
namespace fs {

int init() { return fat16_init(); }
int format() { return fat16_format(); }
int create(const char* path, const char* content) { return fat16_create_file_at(path, content); }
int read(const char* path, char* buffer, uint32_t max_size) { return fat16_read_file_at(path, buffer, max_size); }
int remove(const char* path) { return fat16_delete_at(path); }
int exists(const char* path) { return fat16_file_exists_at(path); }
int list(const char* path) { return fat16_list_dir(path); }
int mkdir(const char* path) { return fat16_mkdir(path); }
int chdir(const char* path) { return fat16_chdir(path); }
const char* getcwd() { return fat16_getcwd(); }
uint16_t cwd_cluster() { return fat16_get_cwd_cluster(); }
int enumerate(const char* path, fat16_enum_entry_t* out, int max_entries) { return fat16_enumerate_dir(path, out, max_entries); }

} // namespace fs
} // namespace toast

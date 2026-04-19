/*
 * toastOS++ FAT16 Filesystem
 * Namespace: toast::fs
 */

#ifndef FAT16_HPP
#define FAT16_HPP

#include "stdint.hpp"

/* FAT16 Boot Sector / BIOS Parameter Block */
struct __attribute__((packed)) fat16_bpb_t {
    uint8_t  jmp[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_serial;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
};

/* FAT16 Directory Entry (32 bytes) */
struct __attribute__((packed)) fat16_dir_entry_t {
    uint8_t  filename[8];
    uint8_t  extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t first_cluster;
    uint32_t file_size;
};

/* File attributes */
#define FAT16_ATTR_READ_ONLY  0x01
#define FAT16_ATTR_HIDDEN     0x02
#define FAT16_ATTR_SYSTEM     0x04
#define FAT16_ATTR_VOLUME_ID  0x08
#define FAT16_ATTR_DIRECTORY  0x10
#define FAT16_ATTR_ARCHIVE    0x20
#define FAT16_ATTR_LFN        0x0F

/* FAT16 cluster values */
#define FAT16_FREE_CLUSTER    0x0000
#define FAT16_RESERVED        0x0001
#define FAT16_BAD_CLUSTER     0xFFF7
#define FAT16_END_OF_CHAIN    0xFFF8

#define FAT16_PARTITION_LBA   2048
#define FAT16_MAX_FILENAME    12

/* File handle structure */
struct fat16_file_t {
    uint16_t first_cluster;
    uint32_t file_size;
    uint32_t current_pos;
    uint16_t current_cluster;
    uint8_t  is_open;
    uint8_t  is_dir;
    char     name[FAT16_MAX_FILENAME];
};

/* Directory enumeration entry */
struct fat16_enum_entry_t {
    char     name[13];
    uint8_t  is_dir;
    uint32_t file_size;
};

namespace toast {
namespace fs {

int init();
int format();

/* File operations */
int create(const char* path, const char* content);
int read(const char* path, char* buffer, uint32_t max_size);
int remove(const char* path);
int exists(const char* path);
int list(const char* path);

/* Directory operations */
int mkdir(const char* path);
int chdir(const char* path);
const char* getcwd();
uint16_t cwd_cluster();

/* Directory enumeration */
int enumerate(const char* path, fat16_enum_entry_t* out, int max_entries);

} // namespace fs
} // namespace toast

/* Legacy C-style function aliases */
int fat16_init();
int fat16_format();
int fat16_create_file(const char* filename, const char* content);
int fat16_read_file(const char* filename, char* buffer, uint32_t max_size);
int fat16_delete_file(const char* filename);
int fat16_list_files();
int fat16_file_exists(const char* filename);
int fat16_mkdir(const char* path);
int fat16_list_dir(const char* path);
int fat16_create_file_at(const char* path, const char* content);
int fat16_read_file_at(const char* path, char* buffer, uint32_t max_size);
int fat16_delete_at(const char* path);
int fat16_file_exists_at(const char* path);
int fat16_chdir(const char* path);
const char* fat16_getcwd();
uint16_t fat16_get_cwd_cluster();
int fat16_enumerate_dir(const char* path, fat16_enum_entry_t* out, int max_entries);

#endif /* FAT16_HPP */

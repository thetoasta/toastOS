/*
 * toastOS++ ATA/IDE Disk Driver
 * Namespace: toast::disk
 */

#include "ata.hpp"
#include "kio.hpp"
#include "funcs.hpp"

namespace toast {
namespace disk {

namespace {  // anonymous namespace for internal helpers

inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

int wait_bsy() {
    int timeout = 100000;
    while ((inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY) && timeout > 0) {
        timeout--;
    }
    return (timeout > 0) ? 0 : -1;
}

int wait_drq() {
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

void delay() {
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
}

} // anonymous namespace

int init() {
    /* Soft reset */
    outb(ATA_PRIMARY_CTRL, 0x04);
    delay();
    outb(ATA_PRIMARY_CTRL, 0x00);
    delay();
    
    if (wait_bsy() < 0) {
        kprint("[ATA] Timeout waiting for drive");
        kprint_newline();
        return -1;
    }
    
    kprint("[ATA] ATA driver initialized");
    kprint_newline();
    return 0;
}

int identify() {
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER);
    delay();
    
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    delay();
    
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        kprint("[ATA] No drive detected");
        kprint_newline();
        return -1;
    }
    
    if (wait_bsy() < 0) {
        kprint("[ATA] Timeout on IDENTIFY");
        kprint_newline();
        return -1;
    }
    
    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) {
        kprint("[ATA] ATAPI device (not supported)");
        kprint_newline();
        return -1;
    }
    
    if (wait_drq() < 0) {
        kprint("[ATA] Error on IDENTIFY");
        kprint_newline();
        return -1;
    }
    
    for (int i = 0; i < 256; i++) {
        inw(ATA_PRIMARY_DATA);
    }
    
    kprint("[ATA] Drive detected and ready");
    kprint_newline();
    return 0;
}

int read(uint32_t lba, uint8_t sector_count, void* buffer) {
    if (sector_count == 0) return -1;
    
    uint16_t* buf = static_cast<uint16_t*>(buffer);
    
    if (wait_bsy() < 0) return -1;
    
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER | ((lba >> 24) & 0x0F));
    delay();
    
    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LO, static_cast<uint8_t>(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, static_cast<uint8_t>((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, static_cast<uint8_t>((lba >> 16) & 0xFF));
    
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    for (int s = 0; s < sector_count; s++) {
        if (wait_drq() < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            buf[s * 256 + i] = inw(ATA_PRIMARY_DATA);
        }
        
        delay();
    }
    
    return 0;
}

int write(uint32_t lba, uint8_t sector_count, const void* buffer) {
    if (sector_count == 0) return -1;
    
    const uint16_t* buf = static_cast<const uint16_t*>(buffer);
    
    if (wait_bsy() < 0) return -1;
    
    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER | ((lba >> 24) & 0x0F));
    delay();
    
    outb(ATA_PRIMARY_SECCOUNT, sector_count);
    outb(ATA_PRIMARY_LBA_LO, static_cast<uint8_t>(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, static_cast<uint8_t>((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, static_cast<uint8_t>((lba >> 16) & 0xFF));
    
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    for (int s = 0; s < sector_count; s++) {
        if (wait_drq() < 0) return -1;
        
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_DATA, buf[s * 256 + i]);
        }
        
        delay();
    }
    
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_FLUSH);
    if (wait_bsy() < 0) return -1;
    
    return 0;
}

int erase(uint32_t start_lba, uint32_t count) {
    static uint8_t zero_buffer[512];
    
    for (int i = 0; i < 512; i++) {
        zero_buffer[i] = 0;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        if (write(start_lba + i, 1, zero_buffer) < 0) {
            return -1;
        }
    }
    
    return 0;
}

int info(Info* out) {
    uint8_t *p = reinterpret_cast<uint8_t*>(out);
    for (int i = 0; i < static_cast<int>(sizeof(Info)); i++) p[i] = 0;

    outb(ATA_PRIMARY_DRIVE_HEAD, ATA_MASTER);
    delay();
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    delay();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) return -1;
    if (wait_bsy() < 0) return -1;
    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) return -1;
    if (wait_drq() < 0) return -1;

    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(ATA_PRIMARY_DATA);

    for (int i = 0; i < 20; i++) {
        out->model[i * 2]     = static_cast<char>(id[27 + i] >> 8);
        out->model[i * 2 + 1] = static_cast<char>(id[27 + i] & 0xFF);
    }
    out->model[40] = '\0';
    for (int i = 39; i >= 0 && out->model[i] == ' '; i--)
        out->model[i] = '\0';

    out->type[0] = 'A'; out->type[1] = 'T'; out->type[2] = 'A';
    out->type[3] = '\0';

    out->total_sectors = static_cast<uint32_t>(id[60]) | (static_cast<uint32_t>(id[61]) << 16);
    out->size_mb = out->total_sectors / 2048;

    return 0;
}

} // namespace disk
} // namespace toast

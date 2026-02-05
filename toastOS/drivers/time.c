#include "time.h"
#include "kio.h"
#include "stdint.h"

#define ALL_MEMORY 0x100000 // Placeholder for now

extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern unsigned int total_memory_kb;

static int bcd2bin(int num) { // Convert BCD to Binary
    return ((num >> 4) * 10) + (num & 0x0F);
}

time_t get_time() {
    time_t time_now;
    
    // Read status register B
    write_port(0x70, 0x0B);
    uint8_t status = read_port(0x71);
    
    // Read time components
    write_port(0x70, 0x00);
    time_now.second = read_port(0x71);
    
    write_port(0x70, 0x02);
    time_now.minute = read_port(0x71);
    
    write_port(0x70, 0x04);
    time_now.hour = read_port(0x71);
    
    write_port(0x70, 0x07);
    time_now.day = read_port(0x71);
    
    write_port(0x70, 0x08);
    time_now.month = read_port(0x71);
    
    write_port(0x70, 0x09);
    time_now.year = read_port(0x71);

    // Convert BCD to binary if necessary
    if (!(status & 0x04)) {
        time_now.second = bcd2bin(time_now.second);
        time_now.minute = bcd2bin(time_now.minute);
        time_now.hour = bcd2bin(time_now.hour);
        time_now.day = bcd2bin(time_now.day);
        time_now.month = bcd2bin(time_now.month);
        time_now.year = bcd2bin(time_now.year);
    }
    
    // Adjust year (usually last 2 digits)
    time_now.year += 2000;
    
    return time_now;
}

// Helper to write string at specific location without affecting global cursor
void write_string_at(int x, int y, const char* str, uint8_t color) {
    volatile char* video_memory = (volatile char*)0xB8000;
    int offset = (y * 80 + x) * 2;
    
    while (*str) {
        video_memory[offset] = *str++;
        video_memory[offset + 1] = color;
        offset += 2;
    }
}

void write_char_at(int x, int y, char c, uint8_t color) {
    volatile char* video_memory = (volatile char*)0xB8000;
    int offset = (y * 80 + x) * 2;
    video_memory[offset] = c;
    video_memory[offset + 1] = color;
}

void write_time_string(int x, int y, uint8_t val) {
    char buf[3];
    buf[0] = '0' + (val / 10);
    buf[1] = '0' + (val % 10);
    buf[2] = 0;
    write_string_at(x, y, buf, ((BLUE << 4) | WHITE));
}

static uint32_t ticks = 0;

void timer_handler() {
    ticks++;
    if (ticks % 18 == 0) { // Roughly every second (18.2 Hz is standard ticker)
        update_top_bar();
    }
    
    // Send EOI to PIC (Master only since IRQ0)
    write_port(0x20, 0x20); 
}

void write_num_at(int x, int y, uint32_t val, uint8_t color) {
    char buf[16];
    int i = 0;
    if (val == 0) {
        write_char_at(x, y, '0', color);
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    // Reverse
    for (int j = 0; j < i; j++) {
        write_char_at(x + j, y, buf[i - 1 - j], color);
    }
}

void update_top_bar() {
    // Draw background bar
    for (int i = 0; i < 80; i++) {
        write_char_at(i, 0, ' ', ((BLUE << 4) | WHITE));
    }

    // Left: toastOS Version
    write_string_at(1, 0, "toastOS v1.1", ((BLUE << 4) | WHITE));

    // Center: Memory Usage (Total Memory)
    if (total_memory_kb > 0) {
        write_string_at(30, 0, "Mem: ", ((BLUE << 4) | WHITE));
        write_num_at(35, 0, total_memory_kb / 1024, ((BLUE << 4) | WHITE));
        write_string_at(40, 0, " MB", ((BLUE << 4) | WHITE));
    } else {
        write_string_at(30, 0, "Mem: Unknown", ((BLUE << 4) | WHITE));
    }

    // Right: Time
    time_t t = get_time();
    write_time_string(70, 0, t.hour);
    write_string_at(72, 0, ":", ((BLUE << 4) | WHITE));
    write_time_string(73, 0, t.minute);
    write_string_at(75, 0, ":", ((BLUE << 4) | WHITE));
    write_time_string(76, 0, t.second);
}

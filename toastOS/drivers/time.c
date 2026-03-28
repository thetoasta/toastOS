#include "time.h"
#include "registry.h"
#include "kio.h"
#include "stdint.h"
#include "string.h"

#define ALL_MEMORY 0x100000 // Placeholder for now
#define PIT_FREQ 1193180
#define TARGET_HZ 18  // ~18 Hz = roughly every 55ms

extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern unsigned int total_memory_kb;
extern volatile int registry_saving;

static int timezone_offset = 0;  // Hours offset from UTC
static int use_24hr = 1;         // 1 = 24hr, 0 = 12hr

static int parse_numeric_tz(const char* s, int* out_offset) {
    int sign = 1;
    int i = 0;
    int value = 0;

    if (!s || !out_offset || s[0] == '\0') return 0;

    if (s[0] == '+') {
        i = 1;
    } else if (s[0] == '-') {
        sign = -1;
        i = 1;
    }

    if (s[i] == '\0') return 0;

    while (s[i]) {
        if (s[i] < '0' || s[i] > '9') return 0;
        value = (value * 10) + (s[i] - '0');
        i++;
    }

    value *= sign;
    if (value < -12 || value > 12) return 0;

    *out_offset = value;
    return 1;
}

static int parse_registry_timezone(const char* tz, int* out_offset) {
    if (!tz || !out_offset) return 0;

    if (strcmp(tz, "EST") == 0) { *out_offset = -5; return 1; }
    if (strcmp(tz, "CST") == 0) { *out_offset = -6; return 1; }
    if (strcmp(tz, "MST") == 0) { *out_offset = -7; return 1; }
    if (strcmp(tz, "PST") == 0) { *out_offset = -8; return 1; }
    if (strcmp(tz, "UTC") == 0) { *out_offset = 0;  return 1; }
    if (strcmp(tz, "GMT") == 0) { *out_offset = 0;  return 1; }

    return parse_numeric_tz(tz, out_offset);
}

static void sync_time_settings_from_registry(void) {
    int reg_offset = 0;
    const char* tz = reg_get("TOASTOS/KERNEL/TIMEZONE");
    const char* fmt = reg_get("TOASTOS/KERNEL/TIMEFORMAT");

    if (parse_registry_timezone(tz, &reg_offset)) {
        timezone_offset = reg_offset;
    }

    if (fmt) {
        if (strcmp(fmt, "12") == 0) {
            use_24hr = 0;
        } else if (strcmp(fmt, "24") == 0) {
            use_24hr = 1;
        }
    }
}

void set_timezone(int offset_hours) {
    timezone_offset = offset_hours;
}

int get_timezone(void) {
    return timezone_offset;
}

void set_time_format(int is_24hr) {
    use_24hr = is_24hr;
}

int get_time_format(void) {
    return use_24hr;
}

void init_timer() {
    uint16_t divisor = PIT_FREQ / TARGET_HZ;
    write_port(0x43, 0x36);              // Channel 0, lo/hi byte, square wave
    write_port(0x40, divisor & 0xFF);    // Low byte
    write_port(0x40, (divisor >> 8) & 0xFF); // High byte
}

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

uint32_t get_uptime_seconds(void) {
    return ticks / 18;
}

void timer_handler() {
    ticks++;
    if (ticks % 18 == 0) { // Roughly every second (18.2 Hz is standard ticker)
        update_top_bar();
    }
    // Don't auto-save registry from timer - only on explicit reg_save() calls
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
    /* Keep runtime time settings in sync with persisted registry values. */
    sync_time_settings_from_registry();

    // Draw background bar
    for (int i = 0; i < 80; i++) {
        write_char_at(i, 0, ' ', ((BLUE << 4) | WHITE));
    }

    // Left: toastOS Version and Terminal number
    write_string_at(1, 0, "toastOS v1.1", ((BLUE << 4) | WHITE));
    write_string_at(14, 0, "[", ((BLUE << 4) | WHITE));
    write_num_at(15, 0, get_current_terminal() + 1, ((BLUE << 4) | YELLOW));
    write_string_at(16, 0, "]", ((BLUE << 4) | WHITE));

    /* Show saving indicator when registry is being persisted */
    if (reg_is_saving()) {
        write_string_at(18, 0, "*", ((BLUE << 4) | YELLOW));
    }

    // Center: Memory Usage (Total Memory)
    if (total_memory_kb > 0) {
        write_string_at(30, 0, "Mem: ", ((BLUE << 4) | WHITE));
        write_num_at(35, 0, total_memory_kb / 1024, ((BLUE << 4) | WHITE));
        write_string_at(40, 0, " MB", ((BLUE << 4) | WHITE));
    } else {
        write_string_at(30, 0, "Mem: Unknown", ((BLUE << 4) | WHITE));
    }

    // Right: Time (with timezone offset)
    time_t t = get_time();
    int adjusted_hour = (int)t.hour + timezone_offset;
    if (adjusted_hour < 0) adjusted_hour += 24;
    if (adjusted_hour >= 24) adjusted_hour -= 24;

    if (use_24hr) {
        write_time_string(70, 0, (uint8_t)adjusted_hour);
    } else {
        int h12 = adjusted_hour % 12;
        if (h12 == 0) h12 = 12;
        write_time_string(70, 0, (uint8_t)h12);
    }
    write_string_at(72, 0, ":", ((BLUE << 4) | WHITE));
    write_time_string(73, 0, t.minute);
    write_string_at(75, 0, ":", ((BLUE << 4) | WHITE));
    write_time_string(76, 0, t.second);
    if (!use_24hr) {
        write_string_at(78, 0, adjusted_hour < 12 ? "AM" : "PM", ((BLUE << 4) | WHITE));
    }
}

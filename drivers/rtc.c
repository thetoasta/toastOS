#include "rtc.h"
#include "funcs.h"
#include "kio.h"

// Timezone offset (default to EST: -5 hours from UTC)
int rtc_timezone_offset = -5;

// Read a register from CMOS
uint8_t rtc_read_register(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// Convert BCD to binary
uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// Check if RTC is updating
static int rtc_is_updating(void) {
    outb(CMOS_ADDRESS, RTC_STATUS_A);
    return (inb(CMOS_DATA) & 0x80);
}

// Initialize RTC
void rtc_init(void) {
    // Wait for any ongoing update to complete
    while (rtc_is_updating());
    
    // Read status register B to check format
    uint8_t status_b = rtc_read_register(RTC_STATUS_B);
    
    // Most CMOS chips use BCD format by default
    // Bit 2 of status B: 0 = BCD, 1 = Binary
    // Bit 1 of status B: 0 = 12-hour, 1 = 24-hour
}

// Set timezone offset
void rtc_set_timezone(int offset_hours) {
    rtc_timezone_offset = offset_hours;
}

// Get current time from RTC
void rtc_get_time(rtc_time_t* time) {
    uint8_t century_register = 0x00;
    uint8_t last_second, last_minute, last_hour, last_day, last_month, last_year;
    
    // Make sure an update isn't in progress
    while (rtc_is_updating());
    
    // Read all values
    time->second = rtc_read_register(RTC_SECONDS);
    time->minute = rtc_read_register(RTC_MINUTES);
    time->hour = rtc_read_register(RTC_HOURS);
    time->day = rtc_read_register(RTC_DAY);
    time->month = rtc_read_register(RTC_MONTH);
    time->year = rtc_read_register(RTC_YEAR);
    time->weekday = rtc_read_register(RTC_WEEKDAY);
    
    // Read again to check consistency (in case update happened between reads)
    do {
        last_second = time->second;
        last_minute = time->minute;
        last_hour = time->hour;
        last_day = time->day;
        last_month = time->month;
        last_year = time->year;
        
        while (rtc_is_updating());
        
        time->second = rtc_read_register(RTC_SECONDS);
        time->minute = rtc_read_register(RTC_MINUTES);
        time->hour = rtc_read_register(RTC_HOURS);
        time->day = rtc_read_register(RTC_DAY);
        time->month = rtc_read_register(RTC_MONTH);
        time->year = rtc_read_register(RTC_YEAR);
        time->weekday = rtc_read_register(RTC_WEEKDAY);
    } while ((last_second != time->second) || (last_minute != time->minute) ||
             (last_hour != time->hour) || (last_day != time->day) ||
             (last_month != time->month) || (last_year != time->year));
    
    // Check format from status register B
    uint8_t status_b = rtc_read_register(RTC_STATUS_B);
    
    // Convert BCD to binary if necessary
    // Bit 2 of status B: 0 = BCD, 1 = Binary
    if (!(status_b & 0x04)) {
        time->second = bcd_to_binary(time->second);
        time->minute = bcd_to_binary(time->minute);
        time->hour = ((time->hour & 0x0F) + (((time->hour & 0x70) / 16) * 10)) | (time->hour & 0x80);
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary(time->year);
    }
    
    // Convert 12-hour to 24-hour if necessary
    // Bit 1 of status B: 0 = 12-hour, 1 = 24-hour
    if (!(status_b & 0x02) && (time->hour & 0x80)) {
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }
    
    // Apply timezone offset
    int adjusted_hour = (int)time->hour + rtc_timezone_offset;
    if (adjusted_hour < 0) {
        adjusted_hour += 24;
        // TODO: adjust day/month/year for date rollback
    } else if (adjusted_hour >= 24) {
        adjusted_hour -= 24;
        // TODO: adjust day/month/year for date rollover
    }
    time->hour = (uint8_t)adjusted_hour;
    
    // Calculate the full year (assume 2000-2099)
    time->year += 2000;
}

// Get weekday name
const char* rtc_get_weekday_name(uint8_t weekday) {
    static const char* days[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    
    if (weekday >= 1 && weekday <= 7) {
        return days[weekday - 1];
    }
    return "Unknown";
}

// Helper to print a 2-digit number with leading zero
static void print_two_digits(uint8_t value) {
    char buffer[3];
    buffer[0] = '0' + (value / 10);
    buffer[1] = '0' + (value % 10);
    buffer[2] = '\0';
    kprint(buffer);
}

// Print current time
void rtc_print_time(void) {
    rtc_time_t time;
    rtc_get_time(&time);
    
    // Convert to 12-hour format
    uint8_t display_hour = time.hour;
    const char* am_pm = "AM";
    
    if (display_hour == 0) {
        display_hour = 12;  // Midnight
    } else if (display_hour == 12) {
        am_pm = "PM";  // Noon
    } else if (display_hour > 12) {
        display_hour -= 12;
        am_pm = "PM";
    }
    
    // Print HH:MM:SS AM/PM
    print_two_digits(display_hour);
    kprint(":");
    print_two_digits(time.minute);
    kprint(":");
    print_two_digits(time.second);
    kprint(" ");
    kprint(am_pm);
}

// Print current date
void rtc_print_date(void) {
    rtc_time_t time;
    rtc_get_time(&time);
    
    // Print MM/DD/YYYY
    print_two_digits(time.month);
    kprint("/");
    print_two_digits(time.day);
    kprint("/");
    kprint_int(time.year);
}

// Print date and time
void rtc_print_datetime(void) {
    rtc_time_t time;
    rtc_get_time(&time);
    
    // Convert to 12-hour format
    uint8_t display_hour = time.hour;
    const char* am_pm = "AM";
    
    if (display_hour == 0) {
        display_hour = 12;  // Midnight
    } else if (display_hour == 12) {
        am_pm = "PM";  // Noon
    } else if (display_hour > 12) {
        display_hour -= 12;
        am_pm = "PM";
    }
    
    // Print: Day, MM/DD/YYYY HH:MM:SS AM/PM
    kprint(rtc_get_weekday_name(time.weekday));
    kprint(", ");
    
    print_two_digits(time.month);
    kprint("/");
    print_two_digits(time.day);
    kprint("/");
    kprint_int(time.year);
    kprint(" ");
    
    print_two_digits(display_hour);
    kprint(":");
    print_two_digits(time.minute);
    kprint(":");
    print_two_digits(time.second);
    kprint(" ");
    kprint(am_pm);
}

#ifndef RTC_H
#define RTC_H

#include "stdint.h"

// CMOS/RTC ports
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

// CMOS registers
#define RTC_SECONDS      0x00
#define RTC_MINUTES      0x02
#define RTC_HOURS        0x04
#define RTC_WEEKDAY      0x06
#define RTC_DAY          0x07
#define RTC_MONTH        0x08
#define RTC_YEAR         0x09
#define RTC_STATUS_A     0x0A
#define RTC_STATUS_B     0x0B

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;
} rtc_time_t;

// Timezone offset in hours (can be negative)
extern int rtc_timezone_offset;

// Function declarations
void rtc_init(void);
void rtc_set_timezone(int offset_hours);
uint8_t rtc_read_register(uint8_t reg);
uint8_t bcd_to_binary(uint8_t bcd);
void rtc_get_time(rtc_time_t* time);
void rtc_print_time(void);
void rtc_print_date(void);
void rtc_print_datetime(void);
const char* rtc_get_weekday_name(uint8_t weekday);

#endif // RTC_H

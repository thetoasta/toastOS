#ifndef TIME_H
#define TIME_H

#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
} time_t;

void init_timer();
void timer_handler();
time_t get_time();
void update_top_bar();
void set_timezone(int offset_hours);
int get_timezone(void);
void set_time_format(int is_24hr);
int get_time_format(void);
uint32_t get_uptime_seconds(void);

#endif

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

#endif

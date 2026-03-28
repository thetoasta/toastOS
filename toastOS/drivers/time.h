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

/* ===== ALARM SYSTEM ===== */
#define MAX_ALARMS 8
#define ALARM_NOTE_LEN 64

typedef struct {
    uint8_t active;
    uint8_t hour;
    uint8_t minute;
    char    note[ALARM_NOTE_LEN];
} Alarm;

int  alarm_set(uint8_t hour, uint8_t minute, const char *note);
void alarm_clear(int index);
void alarm_clear_all(void);
int  alarm_count(void);
const Alarm* alarm_get(int index);
void alarm_check(void);

#endif

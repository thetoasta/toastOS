/*
 * toastOS++ Time System
 * Namespace: toast::time
 */

#ifndef TIME_HPP
#define TIME_HPP

#include "stdint.hpp"

struct time_t {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
};

#define MAX_ALARMS 8
#define ALARM_NOTE_LEN 64

struct Alarm {
    uint8_t active;
    uint8_t hour;
    uint8_t minute;
    char    note[ALARM_NOTE_LEN];
};

namespace toast {
namespace time {

void init();
void tick();
time_t now();
void update_bar();

/* Timezone settings */
void set_timezone(int offset_hours);
int get_timezone();
void set_24hr(int is_24hr);
int is_24hr();

/* Uptime */
uint32_t uptime();

/* Alarm system */
namespace alarm {
    int set(uint8_t hour, uint8_t minute, const char* note);
    void clear(int index);
    void clear_all();
    int count();
    const Alarm* get(int index);
    void check();
}

} // namespace time
} // namespace toast

/* Legacy C functions */
extern "C" {
    void init_timer();
    void timer_handler();
    time_t get_time();
    void update_top_bar();
    void set_timezone(int offset_hours);
    int get_timezone();
    void set_time_format(int is_24hr);
    int get_time_format();
    uint32_t get_uptime_seconds();
    int alarm_set(uint8_t hour, uint8_t minute, const char* note);
    void alarm_clear(int index);
    void alarm_clear_all();
    int alarm_count();
    const Alarm* alarm_get(int index);
    void alarm_check();
}

#endif /* TIME_HPP */

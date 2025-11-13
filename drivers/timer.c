/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#include "timer.h"
#include "kio.h"
#include "funcs.h"

#define PIT_FREQUENCY 1193182
#define TIMER_HZ 10  // 10 ticks per second (100ms per tick)

static volatile uint32_t timer_ticks = 0;
static timer_callback_t timer_callback = 0;
static uint32_t callback_interval = 0;
static uint32_t last_callback_tick = 0;

// Timer interrupt handler
void timer_handler_main(void) {
    timer_ticks++;
    
    // Call registered callback if interval has passed
    if (timer_callback && callback_interval > 0) {
        if (timer_ticks - last_callback_tick >= callback_interval) {
            timer_callback();
            last_callback_tick = timer_ticks;
        }
    }
    
    // Update clock display in top-right corner
    update_clock_display();
}

void timer_init(void) {
    serial_write_string("[TIMER] Initializing PIT timer...\n");
    
    // Calculate divisor for desired frequency
    uint32_t divisor = PIT_FREQUENCY / TIMER_HZ;
    
    // Send command byte (channel 0, lobyte/hibyte, rate generator)
    write_port(0x43, 0x36);
    
    // Send frequency divisor
    write_port(0x40, divisor & 0xFF);        // Low byte
    write_port(0x40, (divisor >> 8) & 0xFF); // High byte
    
    serial_write_string("[TIMER] PIT configured for ");
    serial_write_string(" Hz (100ms ticks)\n");
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_register_callback(timer_callback_t callback, uint32_t interval_ticks) {
    timer_callback = callback;
    callback_interval = interval_ticks;
    last_callback_tick = timer_ticks;
    
    serial_write_string("[TIMER] Callback registered with interval: ");
    serial_write_string(" ticks\n");
}

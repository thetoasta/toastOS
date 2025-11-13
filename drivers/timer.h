/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#ifndef TIMER_H
#define TIMER_H

#include "stdint.h"

// Initialize PIT timer
void timer_init(void);

// Get current tick count
uint32_t timer_get_ticks(void);

// Register a callback to be called every N ticks
typedef void (*timer_callback_t)(void);
void timer_register_callback(timer_callback_t callback, uint32_t interval_ticks);

#endif // TIMER_H

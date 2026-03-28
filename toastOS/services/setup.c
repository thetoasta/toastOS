/*

toastOS Setup Service
Teaches the user on how to use, sets up timezone and time format, and also other features.

*/

#include "../drivers/kio.h"
#include "../drivers/funcs.h"
#include "../drivers/panic.h"
#include "../drivers/fat16.h"
#include "../drivers/time.h"
#include "../drivers/toast_libc.h"
#include "../drivers/registry.h"
#include "setup.h"

void setupProgram();


void setupProgram() {
    clear_screen();
    kprint("Welcome to toastOS. What's your name?");
    kprint_newline();
    kprint("> ");
    char* name_raw = rec_input();
    /* Copy name immediately — rec_input returns a static buffer that the
       next rec_input call will overwrite. */
    static char name[64];
    int ni = 0;
    while (name_raw[ni] && ni < 63) { name[ni] = name_raw[ni]; ni++; }
    name[ni] = '\0';
    reg_set("TOASTOS/KERNEL/NAME", name);
    kprint_newline();
    kprint("Hi, ");
    kprint(name);
    kprint("!");
    kprint_newline();
    clear_screen();
    kprint("Let's get you set up. At the top of the screen, you see a menu bar.");
    kprint_newline();
    kprint("That menu bar contains the time. We need to make it your timezone and options.");
    kprint_newline();
    kprint("We need to set up your timezone! What timezone are you in? (EST, CST, PST, MST, UTC, GMT)");
    kprint_newline();
    kprint("Not on there? Do the UTC code. ex: EST = -5");
    kprint_newline();
    kprint("What's the timezone? > ");
    char* tz = rec_input();
    clear_screen();
    kprint_newline();
    if (strcmp(tz, "EST") == 0) {
        set_timezone(-5);
        reg_set("TOASTOS/KERNEL/TIMEZONE", "EST");
        kprint("Timezone set to EST (UTC-5)");
    } else if (strcmp(tz, "CST") == 0) {
        set_timezone(-6);
        reg_set("TOASTOS/KERNEL/TIMEZONE", "CST");
        kprint("Timezone set to CST (UTC-6)");
    } else if (strcmp(tz, "PST") == 0) {
        set_timezone(-8);
        reg_set("TOASTOS/KERNEL/TIMEZONE", "PST");
        kprint("Timezone set to PST (UTC-8)");
    } else if (strcmp(tz, "MST") == 0) {
        set_timezone(-7);
        reg_set("TOASTOS/KERNEL/TIMEZONE", "MST");
        kprint("Timezone set to MST (UTC-7)");
    } else if (strcmp(tz, "UTC") == 0) {
        set_timezone(0);
        reg_set("TOASTOS/KERNEL/TIMEZONE", "UTC");
        kprint("Timezone set to UTC");
    } else if (strcmp(tz, "GMT") == 0) {
        set_timezone(0);
        reg_set("TOASTOS/KERNEL/TIMEZONE", "GMT");
        kprint("Timezone set to GMT");
    } else if (tz[0] == '+' || tz[0] == '-') {
        int sign = (tz[0] == '-') ? -1 : 1;
        int val = 0;
        int i = 1;
        while (tz[i] >= '0' && tz[i] <= '9') {
            val = val * 10 + (tz[i] - '0');
            i++;
        }
        if (val <= 12) {
            set_timezone(sign * val);
            reg_set("TOASTOS/KERNEL/TIMEZONE", tz);
            kprint("Timezone set to UTC");
            kprint(tz);
        } else {
            kprint("Invalid offset. Use -12 to +12.");
        }
    } else {
        kprint("Usage: timezone EST|CST|MST|PST|UTC|GMT|+N|-N");
    }
    kprint_newline();
    update_top_bar();
    reg_set("TOASTOS/KERNEL/SETUPSTATUS", "1");
    reg_save();
    kprint_newline();
    clear_screen();
    kprint("All done! Let's go into toastOS now!");
    kprint_newline();
    kprint("made with <3 by toasta !");

    /* Keep the final setup splash visible briefly before entering shell. */
    {
        uint32_t start = get_uptime_seconds();
        while ((get_uptime_seconds() - start) < 4) {
            __asm__ volatile("hlt");
        }
    }
}

/*
 * Toast Manager app
 * Uses toast::app namespace
 */

#include "../services/tapplayer.hpp"
#include "../drivers/kio.hpp"
#include "../drivers/toast_libc.hpp"

extern "C" {

void toast_mgr_main(int app_id) {
    toast::app::print((char*)"--- Welcome to Toast Manager ---");
    toast::app::print((char*)"Commands: 'time', 'burn', 'exit'");

    while (1) {
        toast::app::print((char*)"Enter command > ");
        char* cmd = rec_input();

        if (strcmp(cmd, "time") == 0) {
            toast::app::print((char*)"Checking the toaster clock...");
        } 
        else if (strcmp(cmd, "burn") == 0) {
            toast::app::print((char*)"Attempting to burn the toast (Panic)...");
            toast::app::panic((char*)"User requested a system burn!", 0); 
        } 
        else if (strcmp(cmd, "exit") == 0) {
            toast::app::print((char*)"Shutting down Toast Manager. Goodbye!");
            toast::app::exit(0);
            return;
        } 
        else {
            toast::app::print((char*)"Unknown command. Try again.");
        }
    }
}

} // extern "C"
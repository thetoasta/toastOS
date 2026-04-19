/*
 * Obama test app
 * Uses toast::app namespace
 */

#include "../services/tapplayer.hpp"
#include "../drivers/kio.hpp"
#include "../drivers/toast_libc.hpp"

extern "C" {

void obama_main(int app_id) {
    while (1) {
        toast::app::print((char*)"Hello from Obama!");
        toast::app::print((char*)"Ima kill u");
        char* question = rec_input();
        if (strcmp(question, "obama123") == 0) {
            toast::app::print((char*)"Correct! Exiting...");
            toast::app::exit(0);
            return;
        }
    }
}

} // extern "C"
/*

obama test app

*/

#include "../services/tapplayer.h"

/* The kernel calls register_app() before running this.
   The app receives its app_id from the kernel. */

void obama_main(int app_id) {
    while (1) {
        print("Hello from Obama!");
        print("Ima kill u");
        char* question = rec_input();
        if (strcmp(question, "obama123") == 0) {
            print("Correct! Exiting...");
            exitapp(0);
            return;
        }


    }
}
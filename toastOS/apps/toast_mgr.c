#include "../services/tapplayer.h"

void toast_mgr_main(int app_id) {
    print("--- Welcome to Toast Manager ---");
    print("Commands: 'time', 'burn', 'exit'");

    while (1) {
        print("Enter command > ");
        char* cmd = rec_input();

        if (strcmp(cmd, "time") == 0) {
            // Assuming time.h is linked in tapplayer
            print("Checking the toaster clock...");
            // You could call a time function here
        } 
        else if (strcmp(cmd, "burn") == 0) {
            print("Attempting to burn the toast (Panic)...");
            panic("User requested a system burn!", 0); 
        } 
        else if (strcmp(cmd, "exit") == 0) {
            print("Shutting down Toast Manager. Goodbye!");
            exitapp(0);
            return;
        } 
        else {
            print("Unknown command. Try again.");
        }
    }
}
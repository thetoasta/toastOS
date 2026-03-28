/*
 * hello_ext.c — Example toastOS external application
 *
 * This app is NOT compiled into the kernel. It is built separately
 * as a .tapp ELF32 binary, placed on the FAT16 disk image, and
 * loaded at runtime with: exec HELLO.ELF
 *
 * BUILD COMMANDS (from the toastOS project root):
 *   export PATH=/usr/local/cross/bin:$PATH
 *   x86_64-elf-gcc -m32 -ffreestanding -nostdlib -I apps/sdk \
 *       -c apps/hello_ext.c -o hello_ext.o
 *   x86_64-elf-gcc -m32 -ffreestanding -nostdlib -I apps/sdk \
 *       -c apps/sdk/tapp_start.c -o tapp_start.o
 *   x86_64-elf-ld -m elf_i386 -T apps/sdk/app_link.ld \
 *       -o HELLO.ELF hello_ext.o tapp_start.o
 *
 * Then write HELLO.ELF to the FAT16 image and run:
 *   toastOS > exec HELLO.ELF
 */

#include "sdk/toast_app.h"

/* Declare this app's identity and required permissions.
   PERM_PRINT is safe — no security prompt shown. */
TAPP_DEFINE("HelloExternal", "YourName", "1.0", PERM_PRINT);

void app_main(int app_id) {
    print("Hello from an external app!");
    print("This binary was loaded from the FAT16 disk.");
    print("Enter your name: ");

    char *name = rec_input();

    print("Welcome to toastOS,");
    print(name);
    print("Type 'exit' to quit.");

    while (1) {
        char *input = rec_input();
        if (!input) break;

        /* Simple strcmp — the SDK has no strcmp, use manual check */
        int eq = 1;
        int i;
        for (i = 0; input[i] != '\0' && "exit"[i] != '\0'; i++) {
            if (input[i] != "exit"[i]) { eq = 0; break; }
        }
        if (eq && input[i] == '\0' && "exit"[i] == '\0') {
            print("Goodbye!");
            exitapp(0);
            return;
        }

        print("Unknown command. Type 'exit' to quit.");
    }

    exitapp(0);
}

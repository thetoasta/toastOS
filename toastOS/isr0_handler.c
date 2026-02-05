#include <stdint.h>

void isr0_handler() {
    // Handle divide by zero fault
    // You can print to screen, halt, etc.
    // For now, just infinite loop
    while (1) {}
}

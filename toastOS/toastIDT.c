#include <stdint.h>
#include <stdio.h>

// Define handler function type
typedef void (*fault_handler_t)(void);

// Example handlers
void db0handler() { printf("Divide by zero fault!\n"); }
void gpfhandler() { printf("General protection fault!\n"); }

// ToastIDT struct mapping names to handlers
struct toastIDT {
    fault_handler_t divide_by_zero;
    fault_handler_t general_protection;
    // Add more faults as needed
};

// Initialize the table
struct toastIDT idt_table = {
    .divide_by_zero = db0handler,
    .general_protection = gpfhandler,
};

// Example usage
void handle_fault(const char* fault_name) {
    if (fault_name == NULL) return;
    if (!strcmp(fault_name, "divide_by_zero")) idt_table.divide_by_zero();
    else if (!strcmp(fault_name, "general_protection")) idt_table.general_protection();
    else printf("Unknown fault!\n");
}

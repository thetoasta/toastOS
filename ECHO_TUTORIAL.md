# Writing Your First toastOS App: Echo

This tutorial will show you how to create a simple echo application for toastOS using the clean API.

## Step 1: Understanding the API

toastOS provides a user-friendly API that masks system calls with intuitive function names:

- `console_print("text")` - Print text to screen
- `console_print_color("text", COLOR_CYAN)` - Print colored text
- `console_input()` - Get user input
- `console_clear()` - Clear the screen
- `string_compare(s1, s2)` - Compare two strings

## Step 2: Create Your App File

Your app file is already created at: `drivers/user_apps/example_echo.c`

Let's break down how it works:

```c
#include "toast_api.h"  // Import the toastOS API

void app_echo(void) {
    // Clear screen for clean interface
    console_clear();
    
    // Print colored header
    console_print_color("=== Echo Application ===\n", COLOR_CYAN);
    console_print("Type something and press Enter. Type 'exit' to quit.\n\n");
    
    // Main app loop
    while (1) {
        console_print_color("> ", COLOR_YELLOW);
        char* input = console_input();
        
        // Check for exit command
        if (string_compare(input, "exit") == 0) {
            console_print_color("\nExiting Echo app...\n", COLOR_GREEN);
            break;
        }
        
        // Echo the input back
        console_print_color("Echo: ", COLOR_LIGHT_GREEN);
        console_print(input);
        console_newline();
    }
}
```

## Step 3: How System Calls Are Masked

When you write `console_print("hi")`, here's what happens:

1. The API header (`toast_api.h`) defines: `#define console_print(str) _api_print(str)`
2. Your call becomes: `_api_print("hi")`
3. The API implementation (`toast_api.c`) translates to: `kprint("hi")`
4. The kernel function `kprint()` actually prints to screen

**This layering provides:**
- Clean, readable code for users
- Protection from kernel internals
- Easy-to-remember function names
- Consistent interface across all apps

## Step 4: Available API Functions

### Console I/O
```c
console_print("Hello!\n");
console_print_color("Error!", COLOR_RED);
console_print_int(42);
console_newline();
char* input = console_input();
console_clear();
```

### Filesystem
```c
fs_write("myfile.txt", "content");
fs_read("0");  // Read file ID 0
fs_list();
fs_delete("0");
```

### Time/Date
```c
time_get_current();  // Display current time
date_get_current();  // Display current date
unsigned char h = time_read_hours();
unsigned char m = time_read_minutes();
unsigned char s = time_read_seconds();
```

### String Utilities
```c
if (string_compare(s1, s2) == 0) { /* equal */ }
string_length(str);
string_copy(dest, src, n);
```

## Step 5: Register Your App

The echo app is already registered in `drivers/apps.c`:

```c
app_register("echo", app_echo);
```

You can add aliases too:
```c
app_register("echo", app_echo);
app_register("repeat", app_echo);  // Same app, different name
```

## Step 6: Build and Run

```bash
./build-mac.sh
```

In toastOS shell:
```
apps          # List all available apps
run echo      # Run your echo app
```

## Example: More Advanced App

Here's a simple calculator using the same pattern:

```c
#include "toast_api.h"

void app_simple_calc(void) {
    console_clear();
    console_print_color("=== Simple Calculator ===\n", COLOR_CYAN);
    
    console_print("Enter first number: ");
    char* num1_str = console_input();
    
    console_print("Enter operation (+, -, *, /): ");
    char* op = console_input();
    
    console_print("Enter second number: ");
    char* num2_str = console_input();
    
    // Convert strings to integers (simplified)
    int num1 = 0, num2 = 0;
    for (int i = 0; num1_str[i] >= '0' && num1_str[i] <= '9'; i++)
        num1 = num1 * 10 + (num1_str[i] - '0');
    for (int i = 0; num2_str[i] >= '0' && num2_str[i] <= '9'; i++)
        num2 = num2 * 10 + (num2_str[i] - '0');
    
    // Perform calculation
    int result = 0;
    if (string_compare(op, "+") == 0) result = num1 + num2;
    else if (string_compare(op, "-") == 0) result = num1 - num2;
    else if (string_compare(op, "*") == 0) result = num1 * num2;
    else if (string_compare(op, "/") == 0 && num2 != 0) result = num1 / num2;
    
    console_print_color("\nResult: ", COLOR_YELLOW);
    console_print_int(result);
    console_newline();
}
```

Then register it:
```c
app_register("simplecalc", app_simple_calc);
```

## Key Takeaways

✅ **Use the API** - Never call kernel functions directly (like `kprint()`)
✅ **Include toast_api.h** - This gives you all the masked system calls
✅ **Clear naming** - `console_print()` is clearer than `kprint()`
✅ **Color everything** - Makes your UI look professional
✅ **Always provide exit** - Let users quit gracefully
✅ **Register in apps.c** - Add your function to `apps_init()`

## What's Happening Under the Hood?

```
Your Code:          console_print("hi");
      ↓
API Macro:          _api_print("hi");
      ↓
API Implementation: kprint("hi");
      ↓
Kernel Function:    Writes to VGA buffer
```

This is exactly like:
- Python's `print()` masking `sys.stdout.write()`
- Java's `System.out.println()` masking native I/O
- C++'s `cout` masking `write()` syscalls

**You're writing real OS applications with a clean interface! 🍞**

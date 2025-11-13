# toastOS Application Development Guide

## Overview
toastOS allows users to create their own applications using a simple C API. Apps are registered at compile-time and can access system functions through a clean interface.

## Quick Start

### 1. Create Your App File
Create a new `.c` file in `drivers/user_apps/` (e.g., `my_app.c`)

### 2. Include the API
```c
#include "toast_api.h"
```

### 3. Write Your App Function
```c
void app_myapp(void) {
    console_print_color("=== My App ===\n", COLOR_CYAN);
    console_print("Hello from my app!\n");
    
    console_print("Enter your name: ");
    char* name = console_input();
    
    console_print("Hello, ");
    console_print(name);
    console_newline();
}
```

### 4. Register Your App
Edit `drivers/apps.c` and add your app to `apps_init()`:
```c
app_register("myapp", app_myapp);
```

### 5. Compile
Run `./build-mac.sh` (or `build-win.bat` on Windows)

### 6. Run Your App
In toastOS shell:
```
run myapp
```

## API Reference

### Console I/O

| Function | Description | Example |
|----------|-------------|---------|
| `console_print(str)` | Print a string | `console_print("Hello!\n");` |
| `console_print_color(str, color)` | Print colored text | `console_print_color("Error!\n", COLOR_RED);` |
| `console_print_int(num)` | Print an integer | `console_print_int(42);` |
| `console_newline()` | Print a newline | `console_newline();` |
| `console_input()` | Get user input | `char* input = console_input();` |
| `console_clear()` | Clear the screen | `console_clear();` |

### Filesystem

| Function | Description | Example |
|----------|-------------|---------|
| `fs_write(filename, content)` | Write a file | `fs_write("data.txt", "Hello");` |
| `fs_read(file_id)` | Read and display file | `fs_read("0");` |
| `fs_list()` | List all files | `fs_list();` |
| `fs_delete(file_id)` | Delete a file | `fs_delete("0");` |

### Time/Date

| Function | Description | Example |
|----------|-------------|---------|
| `time_get_current()` | Display current time | `time_get_current();` |
| `date_get_current()` | Display current date | `date_get_current();` |
| `time_read_hours()` | Get hours (0-23) | `unsigned char h = time_read_hours();` |
| `time_read_minutes()` | Get minutes (0-59) | `unsigned char m = time_read_minutes();` |
| `time_read_seconds()` | Get seconds (0-59) | `unsigned char s = time_read_seconds();` |

### Registry (System Settings)

| Function | Description | Example |
|----------|-------------|---------|
| `registry_get(key)` | Get a registry value | `const char* user = registry_get("user.name");` |
| `registry_set(key, value)` | Set a registry value | `registry_set("theme", "dark");` |

### String Utilities

| Function | Description | Example |
|----------|-------------|---------|
| `string_compare(s1, s2)` | Compare strings | `if (string_compare(input, "yes") == 0) {...}` |
| `string_compare_n(s1, s2, n)` | Compare N chars | `string_compare_n(cmd, "run ", 4)` |
| `string_length(s)` | Get string length | `unsigned int len = string_length(str);` |
| `string_copy(dest, src, n)` | Copy N characters | `string_copy(buffer, input, 10);` |

### Colors

Available color constants:
- `COLOR_BLACK`, `COLOR_BLUE`, `COLOR_GREEN`, `COLOR_CYAN`
- `COLOR_RED`, `COLOR_MAGENTA`, `COLOR_BROWN`, `COLOR_LIGHT_GREY`
- `COLOR_DARK_GREY`, `COLOR_LIGHT_BLUE`, `COLOR_LIGHT_GREEN`, `COLOR_LIGHT_CYAN`
- `COLOR_LIGHT_RED`, `COLOR_LIGHT_MAGENTA`, `COLOR_YELLOW`, `COLOR_WHITE`

## Example Apps

### Echo App
See `drivers/user_apps/example_echo.c` for a complete example.

### Simple Calculator
```c
#include "toast_api.h"

void app_calc(void) {
    console_print("Enter two numbers:\n");
    console_print("Number 1: ");
    char* num1_str = console_input();
    console_print("Number 2: ");
    char* num2_str = console_input();
    
    // Convert strings to integers (simplified)
    int num1 = 0, num2 = 0;
    for (int i = 0; num1_str[i]; i++)
        num1 = num1 * 10 + (num1_str[i] - '0');
    for (int i = 0; num2_str[i]; i++)
        num2 = num2 * 10 + (num2_str[i] - '0');
    
    console_print("Sum: ");
    console_print_int(num1 + num2);
    console_newline();
}
```

### Todo List
```c
#include "toast_api.h"

void app_todo(void) {
    console_print_color("=== Todo List ===\n", COLOR_CYAN);
    console_print("1. View todos\n2. Add todo\n3. Exit\n> ");
    
    char* choice = console_input();
    
    if (string_compare(choice, "1") == 0) {
        fs_read("todos.txt");
    } else if (string_compare(choice, "2") == 0) {
        console_print("Enter todo: ");
        char* todo = console_input();
        fs_write("todos.txt", todo);
        console_print_color("Todo saved!\n", COLOR_GREEN);
    }
}
```

## Best Practices

1. **Always clear screen at start**: `console_clear();` for better UX
2. **Use colors**: Make your UI intuitive with `console_print_color()`
3. **Provide exit option**: Allow users to exit your app gracefully
4. **Handle input safely**: Always check input before processing
5. **Save data to disk**: Use `fs_write()` to persist user data
6. **Show feedback**: Confirm actions with colored messages

## Tips

- **App names**: Keep them short and memorable (e.g., "calc", "todo")
- **Aliases**: Register multiple names for the same app (e.g., "calculator" and "calc")
- **Error handling**: Always validate user input
- **Testing**: Test your app in QEMU before distributing

## Advanced: Direct System Access

If you need lower-level access (advanced users only), you can:
- Include specific driver headers (e.g., `#include "drivers/kio.h"`)
- Call kernel functions directly
- Access hardware ports (use inline assembly)

**Warning**: Direct system access bypasses API safety checks!

## Contributing

To contribute your app to toastOS:
1. Create your app in `drivers/user_apps/`
2. Document it in this README
3. Submit a pull request to the toastOS repository

## Support

For questions or issues:
- Check existing apps in `drivers/apps.c` for examples
- Read the API documentation in `drivers/toast_api.h`
- Open an issue on the toastOS GitHub repository

---

**Happy coding with toastOS! 🍞**

# toastOS
hi! welcome to toastOS, a open source os that isn't the best but also is pretty unique and cool

## Features
- 🎨 **Intuitive Shell** - Colorful commands with help system
- 💾 **toastFS** - Persistent filesystem that saves to disk
- ⏰ **Real-time Clock** - Live clock display with timezone support
- 📝 **Toast Editor** - Text editor with autosave
- 🚀 **Application Runtime** - Run built-in and custom apps
- 🛠️ **User API** - Write your own apps with clean masked system calls
- ⏲️ **Timer System** - Background tasks and callbacks
- 📋 **Registry** - System configuration storage

## Quick Start

### Building
- **macOS/Linux**: `./build-mac.sh`
- **Windows**: `build-win.bat` (requires WSL)
  - Run `d-requirements.bat` if dependencies are missing

### Built-in Apps
```
apps                 # List all available apps
run calc             # Calculator
run todo             # Todo list manager
run echo             # Echo application
run sysinfo          # System information
run stopwatch        # Timer/stopwatch
run notes            # Quick notes
```

### Basic Commands
```
help                 # Show all commands
fs-create myfile     # Create a file
fs-list              # List all files
fs-read 0            # Read file by ID
edit myfile          # Open text editor
time                 # Show current time
date                 # Show current date
clear                # Clear screen
```

## Writing Your Own Apps

toastOS lets you write applications in C with a clean API that masks system calls:

```c
#include "toast_api.h"

void app_myapp(void) {
    console_clear();
    console_print_color("=== My App ===\n", COLOR_CYAN);
    console_print("Hello from my app!\n");
    
    console_print("Enter your name: ");
    char* name = console_input();
    
    console_print("Hello, ");
    console_print(name);
    console_newline();
}
```

**Documentation:**
- 📘 [API Quick Reference](API_REFERENCE.md) - One-page API guide
- 📗 [Echo Tutorial](ECHO_TUTORIAL.md) - Build your first app
- 📕 [App Development Guide](APP_DEVELOPMENT.md) - Complete documentation
- Line 1 is always reserved for the DTI. 

## Contributing
please contribute i need it, you need it, we all need it.

### Credits
[kernel101](https://arjunsreedharan.org/post/82710718100/kernels-101-lets-write-a-kernel)
[kernel201](https://arjunsreedharan.org/post/99370248137/kernels-201-lets-write-a-kernel-with-keyboard)
[gemini AI](https://gemini.google.com)
[chatGPT (ai)](https://chatgpt.com)
[escapeOS](https://github.com/exscape/exscapeOS)
[build-your-own-x](https://github.com/codecrafters-io/build-your-own-x)
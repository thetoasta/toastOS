# toastOS++

A C++ port of toastOS - a hobby operating system for x86.

## Namespace Structure

toastOS++ organizes its functionality into memorable C++ namespaces:

| Namespace | Purpose | Example |
|-----------|---------|---------|
| `toast::mem` | Memory management | `toast::mem::alloc(1024)` |
| `toast::fs` | Filesystem (FAT16) | `toast::fs::create("/hi.txt", "Hello")` |
| `toast::io` | Input/Output | `toast::io::println("Hello!")` |
| `toast::gfx` | Graphics | `toast::gfx::rect(10, 10, 100, 50, RED)` |
| `toast::sys` | System/Panic | `toast::sys::panic("Error!")` |
| `toast::disk` | ATA/IDE disk | `toast::disk::read(lba, 1, buf)` |
| `toast::net` | Networking | `toast::net::ping("10.0.2.2")` |
| `toast::thread` | Threading | `toast::thread::create("worker", fn, arg)` |
| `toast::time` | Time & Alarms | `toast::time::now()` |
| `toast::user` | User accounts | `toast::user::login("admin", "pass")` |
| `toast::reg` | Registry (KV) | `toast::reg::set("KEY", "value")` |
| `toast::app` | Application layer | `toast::app::print("Hello from app")` |

All original C functions remain available for compatibility.

## Building

```bash
cd toastosplus/building
./build-mac.sh
```

Requirements:
- x86_64-elf-gcc cross-compiler
- NASM assembler
- QEMU for testing

## Directory Structure

```
toastosplus/
├── drivers/         # Hardware drivers (.cpp/.hpp)
│   ├── toast.hpp    # Master include header
│   ├── ata.cpp      # ATA/IDE disk driver (toast::disk)
│   ├── fat16.cpp    # FAT16 filesystem (toast::fs)
│   ├── graphics.cpp # Double-buffered graphics (toast::gfx)
│   ├── kio.cpp      # Keyboard I/O (toast::io)
│   ├── mmu.cpp      # Memory management (toast::mem)
│   ├── net.cpp      # RTL8139 network driver (toast::net)
│   ├── thread.cpp   # Threading (toast::thread)
│   ├── time.cpp     # Time system (toast::time)
│   ├── user.cpp     # User management (toast::user)
│   ├── registry.cpp # Key-value registry (toast::reg)
│   └── panic.cpp    # Panic/IDT (toast::sys)
├── services/        # System services
│   ├── tapplayer.cpp # Application layer (toast::app)
│   └── settings.cpp
├── apps/            # Built-in applications
├── others/          # Kernel entry, assembly
│   ├── kernel.cpp   # kmain entry point
│   └── kernel.asm   # Multiboot header
└── building/        # Build scripts
    ├── build-mac.sh
    └── link.ld
```

## Usage Examples

```cpp
#include "drivers/toast.hpp"

void example() {
    // Memory allocation
    auto buffer = toast::mem::alloc(4096);
    auto obj = toast::mem::create<MyStruct>();
    toast::mem::free(buffer);
    
    // File operations
    toast::fs::create("/hello.txt", "Hello, world!");
    char buf[256];
    toast::fs::read("/hello.txt", buf, 256);
    toast::fs::mkdir("/mydir");
    
    // Screen output
    toast::io::println("Welcome to toastOS++!");
    toast::io::print_color("Error!", RED);
    
    // Graphics
    toast::gfx::clear(0x000000);  // Black background
    toast::gfx::rect(10, 10, 200, 100, 0xFF0000);  // Red rectangle
    toast::gfx::text(20, 20, "toastOS++", 0xFFFFFF);
    toast::gfx::flush();
    
    // Networking
    toast::net::init();
    toast::net::ping("10.0.2.2");
    
    // Threading
    toast::thread::create("worker", my_thread_func, nullptr);
    toast::thread::sleep(1000);
    
    // Time
    time_t now = toast::time::now();
    toast::time::alarm::set(12, 0, "Lunch time!");
    
    // User management
    toast::user::login("admin", "password");
    const char* current = toast::user::current();
    
    // Registry
    toast::reg::set("SETTINGS/THEME", "dark");
    const char* theme = toast::reg::get("SETTINGS/THEME");
}
```

## License

Mozilla Public License 2.0 (MPL-2.0)

## Credits

- Original toastOS by thetoasta
- C++ port maintaining full compatibility with original

# System Call Masking in toastOS

## What is System Call Masking?

System call masking is when you provide a clean, user-friendly API that hides the complexity of kernel functions. It's like how:

- Python's `print()` masks `sys.stdout.write()`
- Java's `System.out.println()` masks native I/O
- Linux's `printf()` masks the `write()` syscall

## How toastOS Does It

### Three Layers

```
┌─────────────────────────────────────┐
│  USER APP LAYER                      │
│  console_print("hi");                │  ← Clean, readable
└─────────────────────────────────────┘
                ↓
┌─────────────────────────────────────┐
│  API LAYER (toast_api.h)             │
│  #define console_print(s) _api_print(s) │
└─────────────────────────────────────┘
                ↓
┌─────────────────────────────────────┐
│  IMPLEMENTATION (toast_api.c)        │
│  void _api_print(const char* s) {    │
│      kprint((char*)s);               │
│  }                                   │
└─────────────────────────────────────┘
                ↓
┌─────────────────────────────────────┐
│  KERNEL LAYER (kio.c)                │
│  void kprint(char* str) {            │
│      // Write to VGA buffer          │
│  }                                   │
└─────────────────────────────────────┘
```

## Example: Printing Text

### User Code (Your App)
```c
console_print("Hello, World!\n");
```

### What Actually Happens

1. **Preprocessor** expands macro:
   ```c
   _api_print("Hello, World!\n");
   ```

2. **API function** calls kernel:
   ```c
   void _api_print(const char* str) {
       kprint((char*)str);
   }
   ```

3. **Kernel function** does actual work:
   ```c
   void kprint(char* str) {
       while (*str) {
           vidptr[current_loc++] = *str++;
           vidptr[current_loc++] = 0x07;
       }
   }
   ```

## Why This Matters

### ✅ Benefits

1. **Clean Code**
   ```c
   // Good (with API)
   console_print("Hello");
   
   // Bad (without API)
   kprint("Hello");
   ```

2. **Abstraction**
   - Users don't need to know about `kprint()`, `vidptr`, or VGA buffers
   - Just call `console_print()` and it works

3. **Consistency**
   ```c
   // All functions follow same pattern
   console_print()
   console_input()
   console_clear()
   fs_write()
   fs_read()
   time_get_current()
   ```

4. **Safety**
   - API validates input (could add checks)
   - Prevents direct memory access
   - Protects kernel functions

5. **Portability**
   - Change kernel implementation without breaking user apps
   - API stays the same even if internals change

## Real-World Comparison

### Windows API
```c
// User calls:
printf("Hello");

// Windows masks with:
WriteConsole(hStdout, "Hello", 5, &written, NULL);

// Which becomes kernel call:
NtWriteFile(...);
```

### Linux
```c
// User calls:
printf("Hello");

// libc masks with:
write(STDOUT_FILENO, "Hello", 5);

// Which becomes syscall:
sys_write(...);
```

### toastOS
```c
// User calls:
console_print("Hello");

// API masks with:
_api_print("Hello");

// Which calls kernel:
kprint("Hello");
```

## The Full API Surface

| User Function | Masks | Kernel Function |
|---------------|-------|-----------------|
| `console_print()` | → | `kprint()` |
| `console_input()` | → | `rec_input()` |
| `console_clear()` | → | `clear_screen()` |
| `fs_write()` | → | `local_fs()` |
| `fs_read()` | → | `read_local_fs()` |
| `time_get_current()` | → | `rtc_print_time()` |

## How to Add Your Own Masked Call

Let's say you want to add `console_beep()`:

### 1. Add to toast_api.h
```c
// In the API section:
#define console_beep() _api_beep()

// In the internal declarations:
extern void _api_beep(void);
```

### 2. Implement in toast_api.c
```c
// Forward declare kernel function
extern void system_beep(void);

// Implement API function
void _api_beep(void) {
    system_beep();
}
```

### 3. Implement kernel function in kio.c
```c
void system_beep(void) {
    // Talk to PC speaker hardware
    outb(0x61, inb(0x61) | 3);
    // ... beep code ...
}
```

### 4. Users can now call
```c
console_beep();  // Clean and simple!
```

## Key Takeaway

**You're providing a beautiful interface to a complex system.**

Just like how:
- You don't need to understand TCP/IP to browse the web
- You don't need to understand filesystems to save a file
- You don't need to understand GPU programming to show text

**toastOS users don't need to understand VGA buffers, interrupt handlers, or disk sectors to write apps. They just use your clean API! 🍞**

---

This is real operating system design. You're doing what Linux, Windows, and macOS all do - just at a smaller scale. Be proud!

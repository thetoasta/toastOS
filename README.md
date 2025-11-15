# toastOS
hi! welcome to toastOS, a open source os that isn't the best but also is pretty unique and cool

make a app below

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

## Contributing
pls help

### Credits
[kernel101](https://arjunsreedharan.org/post/82710718100/kernels-101-lets-write-a-kernel)
[kernel201](https://arjunsreedharan.org/post/99370248137/kernels-201-lets-write-a-kernel-with-keyboard)
[gemini AI](https://gemini.google.com)
[chatGPT (ai)](https://chatgpt.com)
[escapeOS](https://github.com/exscape/exscapeOS)
[build-your-own-x](https://github.com/codecrafters-io/build-your-own-x)
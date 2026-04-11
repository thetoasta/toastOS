/*
 * toastOS++ Panic Handler
 * Namespace: toast::sys
 */

#ifndef PANIC_HPP
#define PANIC_HPP

namespace toast {
namespace sys {

/* Recoverable panic - shows error, user can press Enter to continue */
void warn(const char* msg);

/* Fatal panic - halts system */
[[noreturn]] void panic(const char* msg);

/* Initialize IDT */
void init();

/* ISR handler */
void isr_handler();

} // namespace sys
} // namespace toast

/* Legacy C-style function aliases */
void l1_panic(const char* message);
void l3_panic(const char* message);
void init_idt();
void isr_handler();

#endif /* PANIC_HPP */

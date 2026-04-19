/*
 * toastOS++ Tscript
 * Converted to C++ from toastOS
 */

#ifndef TSCRIPT_HPP
#define TSCRIPT_HPP

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ToastScript — tiny interpreted scripting language for toastOS
 *
 * Language reference:
 *   # comment
 *   print <text>           Print text ($var expanded)
 *   println <text>         Print text + newline
 *   println                Just newline
 *   input $var             Read a line into variable
 *   set $var <value>       Set variable ($var expanded in value)
 *   add $var <N>           Add integer to variable
 *   sub $var <N>           Subtract integer from variable
 *   if $var op <value>     Conditional (op: == != > < >= <=)
 *   endif                  End conditional block
 *   goto <label>           Jump to label
 *   :label                 Define a label
 *   clear                  Clear screen
 *   wait                   Wait for any keypress
 *   exit                   End program
 */

/* Run a ToastScript program from source text.  Returns 0 on success. */
int tscript_run(const char *source);

/* Validate syntax.  Returns 0 on success.
   On error, writes a message into errmsg (up to maxlen chars). */
int tscript_validate(const char *source, char *errmsg, int maxlen);

#ifdef __cplusplus
}
#endif

#endif /* TSCRIPT_HPP */

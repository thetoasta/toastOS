/* toastCC - c interpreter for toastOS
 * supports a useful subset of C: variables, functions, arrays,
 * if/else, while, for, break, continue, return, printf, etc.
 * Includes a sandbox layer with instruction limits for safe in-OS coding.
 */

#include "toastcc.h"
#include "kio.h"
#include "fat16.h"
#include "mmu.h"
#include "toast_libc.h"
#include "stdio.h"
#include "time.h"

/* ===== limits ===== */
#define TCC_MAX_TOKENS   4096
#define TCC_MAX_VARS     128
#define TCC_MAX_FUNCS    64
#define TCC_MAX_STR      256
#define TCC_MAX_PARAMS   8
#define TCC_MAX_ARRAY    256
#define TCC_MAX_ARGS     16
#define TCC_SRC_MAX      32768

/* ===== token types ===== */
enum {
    TOK_NUM, TOK_STR, TOK_IDENT,
    TOK_KW_INT, TOK_KW_CHAR, TOK_KW_VOID, TOK_KW_CONST,
    TOK_KW_IF, TOK_KW_ELSE, TOK_KW_WHILE, TOK_KW_FOR,
    TOK_KW_RETURN, TOK_KW_BREAK, TOK_KW_CONTINUE,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_MOD,
    TOK_ASSIGN, TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_BITNOT,
    TOK_SHL, TOK_SHR,
    TOK_INC, TOK_DEC,
    TOK_PLUSEQ, TOK_MINUSEQ, TOK_STAREQ, TOK_SLASHEQ, TOK_MODEQ,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_SEMI, TOK_COMMA,
    TOK_EOF
};

/* ===== data structures ===== */
typedef struct {
    int type;
    int num_val;
    char str_val[128];
    int line;
} token_t;

#define VAL_INT 0
#define VAL_STR 1

typedef struct {
    int type;
    int int_val;
    char str_val[TCC_MAX_STR];
} value_t;

typedef struct {
    char name[64];
    value_t val;
    int is_array;
    int array_size;
    int *arr_data;
    int depth;
} var_t;

typedef struct {
    char name[64];
    char params[TCC_MAX_PARAMS][64];
    int param_count;
    int body_pos;
} func_t;

/* execution flags */
#define EXEC_NORMAL   0
#define EXEC_RETURN   1
#define EXEC_BREAK    2
#define EXEC_CONTINUE 3

/* ===== interpreter state ===== */
static token_t tokens[TCC_MAX_TOKENS];
static int num_tokens;
static int pos;

static var_t vars[TCC_MAX_VARS];
static int num_vars;

static func_t funcs[TCC_MAX_FUNCS];
static int num_funcs;

static int scope_depth;
static int exec_flag;
static value_t return_val;
static int had_error;
static int call_depth;

/* ===== sandbox limits ===== */
#define TCC_MAX_INSTRUCTIONS  500000   /* max ops before forced halt */
#define TCC_MAX_ALLOC_BYTES   (64*1024) /* 64 KB heap cap per program */
static int instruction_count;
static int alloc_bytes_used;

/* ===== helpers ===== */
static void tcc_print(const char *s) { kprint(s); }
static void tcc_newline(void) { kprint_newline(); }

static void tcc_print_int(int n) {
    char buf[32];
    snprintf(buf, 32, "%d", n);
    kprint(buf);
}

static void tcc_error(const char *msg) {
    if (had_error) return;
    had_error = 1;
    kprint("[tcc] error");
    if (pos < num_tokens) {
        kprint(" (line ");
        tcc_print_int(tokens[pos].line);
        kprint(")");
    }
    kprint(": ");
    kprint(msg);
    kprint_newline();
}

/* Sandbox: call this at every statement/loop iteration */
static void sandbox_tick(void) {
    if (had_error) return;
    instruction_count++;
    if (instruction_count > TCC_MAX_INSTRUCTIONS) {
        tcc_error("sandbox: program exceeded instruction limit (infinite loop?)");
    }
}

/* Sandbox: tracked allocation */
static void *tcc_alloc(int size) {
    if (alloc_bytes_used + size > TCC_MAX_ALLOC_BYTES) {
        tcc_error("sandbox: memory allocation limit exceeded");
        return 0;
    }
    void *p = kmalloc(size);
    if (p) alloc_bytes_used += size;
    return p;
}

static int peek(void) { return tokens[pos].type; }
static void advance(void) { if (pos < num_tokens - 1) pos++; }

static void expect(int type) {
    if (tokens[pos].type != type) {
        tcc_error("unexpected token");
        return;
    }
    advance();
}

static int match(int type) {
    if (tokens[pos].type == type) { advance(); return 1; }
    return 0;
}

static int is_type_tok(int t) {
    return t == TOK_KW_INT || t == TOK_KW_CHAR || t == TOK_KW_VOID || t == TOK_KW_CONST;
}

/* ===== lexer ===== */
static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_alnum(char c) { return is_alpha(c) || is_digit(c); }

static int check_keyword(const char *s) {
    if (strcmp(s, "int") == 0) return TOK_KW_INT;
    if (strcmp(s, "char") == 0) return TOK_KW_CHAR;
    if (strcmp(s, "void") == 0) return TOK_KW_VOID;
    if (strcmp(s, "const") == 0) return TOK_KW_CONST;
    if (strcmp(s, "if") == 0) return TOK_KW_IF;
    if (strcmp(s, "else") == 0) return TOK_KW_ELSE;
    if (strcmp(s, "while") == 0) return TOK_KW_WHILE;
    if (strcmp(s, "for") == 0) return TOK_KW_FOR;
    if (strcmp(s, "return") == 0) return TOK_KW_RETURN;
    if (strcmp(s, "break") == 0) return TOK_KW_BREAK;
    if (strcmp(s, "continue") == 0) return TOK_KW_CONTINUE;
    return TOK_IDENT;
}

static int tokenize(const char *src) {
    int i = 0, t = 0, line = 1;
    int len = strlen(src);

    while (i < len && t < TCC_MAX_TOKENS - 1) {
        /* skip whitespace */
        while (i < len && (src[i] == ' ' || src[i] == '\t' || src[i] == '\r' || src[i] == '\n')) {
            if (src[i] == '\n') line++;
            i++;
        }
        if (i >= len) break;

        /* line comment */
        if (src[i] == '/' && i + 1 < len && src[i+1] == '/') {
            while (i < len && src[i] != '\n') i++;
            continue;
        }
        /* block comment */
        if (src[i] == '/' && i + 1 < len && src[i+1] == '*') {
            i += 2;
            while (i + 1 < len && !(src[i] == '*' && src[i+1] == '/')) {
                if (src[i] == '\n') line++;
                i++;
            }
            if (i + 1 < len) i += 2;
            continue;
        }
        /* preprocessor - skip line */
        if (src[i] == '#') {
            while (i < len && src[i] != '\n') i++;
            continue;
        }

        tokens[t].line = line;

        /* number */
        if (is_digit(src[i])) {
            int val = 0;
            if (src[i] == '0' && i + 1 < len && (src[i+1] == 'x' || src[i+1] == 'X')) {
                i += 2;
                while (i < len) {
                    if (src[i] >= '0' && src[i] <= '9') val = val * 16 + (src[i] - '0');
                    else if (src[i] >= 'a' && src[i] <= 'f') val = val * 16 + (src[i] - 'a' + 10);
                    else if (src[i] >= 'A' && src[i] <= 'F') val = val * 16 + (src[i] - 'A' + 10);
                    else break;
                    i++;
                }
            } else {
                while (i < len && is_digit(src[i])) {
                    val = val * 10 + (src[i] - '0');
                    i++;
                }
            }
            tokens[t].type = TOK_NUM;
            tokens[t].num_val = val;
            tokens[t].str_val[0] = 0;
            t++;
            continue;
        }

        /* string literal */
        if (src[i] == '"') {
            i++;
            int s = 0;
            while (i < len && src[i] != '"' && s < 126) {
                if (src[i] == '\\' && i + 1 < len) {
                    i++;
                    switch (src[i]) {
                        case 'n': tokens[t].str_val[s++] = '\n'; break;
                        case 't': tokens[t].str_val[s++] = '\t'; break;
                        case '\\': tokens[t].str_val[s++] = '\\'; break;
                        case '"': tokens[t].str_val[s++] = '"'; break;
                        case '0': tokens[t].str_val[s++] = '\0'; break;
                        default: tokens[t].str_val[s++] = src[i]; break;
                    }
                } else {
                    tokens[t].str_val[s++] = src[i];
                }
                i++;
            }
            tokens[t].str_val[s] = 0;
            tokens[t].type = TOK_STR;
            if (i < len && src[i] == '"') i++;
            t++;
            continue;
        }

        /* char literal */
        if (src[i] == '\'') {
            i++;
            int ch = 0;
            if (i < len && src[i] == '\\') {
                i++;
                if (i < len) {
                    switch (src[i]) {
                        case 'n': ch = '\n'; break;
                        case 't': ch = '\t'; break;
                        case '0': ch = 0; break;
                        case '\\': ch = '\\'; break;
                        case '\'': ch = '\''; break;
                        default: ch = src[i]; break;
                    }
                    i++;
                }
            } else if (i < len) {
                ch = (unsigned char)src[i];
                i++;
            }
            if (i < len && src[i] == '\'') i++;
            tokens[t].type = TOK_NUM;
            tokens[t].num_val = ch;
            tokens[t].str_val[0] = 0;
            t++;
            continue;
        }

        /* identifier / keyword */
        if (is_alpha(src[i])) {
            int s = 0;
            while (i < len && is_alnum(src[i]) && s < 62) {
                tokens[t].str_val[s++] = src[i];
                i++;
            }
            tokens[t].str_val[s] = 0;
            tokens[t].type = check_keyword(tokens[t].str_val);
            t++;
            continue;
        }

        /* two-char operators */
        if (i + 1 < len) {
            char c1 = src[i], c2 = src[i+1];
            int found = 1;
            if (c1 == '=' && c2 == '=') tokens[t].type = TOK_EQ;
            else if (c1 == '!' && c2 == '=') tokens[t].type = TOK_NEQ;
            else if (c1 == '<' && c2 == '=') tokens[t].type = TOK_LEQ;
            else if (c1 == '>' && c2 == '=') tokens[t].type = TOK_GEQ;
            else if (c1 == '<' && c2 == '<') tokens[t].type = TOK_SHL;
            else if (c1 == '>' && c2 == '>') tokens[t].type = TOK_SHR;
            else if (c1 == '&' && c2 == '&') tokens[t].type = TOK_AND;
            else if (c1 == '|' && c2 == '|') tokens[t].type = TOK_OR;
            else if (c1 == '+' && c2 == '+') tokens[t].type = TOK_INC;
            else if (c1 == '-' && c2 == '-') tokens[t].type = TOK_DEC;
            else if (c1 == '+' && c2 == '=') tokens[t].type = TOK_PLUSEQ;
            else if (c1 == '-' && c2 == '=') tokens[t].type = TOK_MINUSEQ;
            else if (c1 == '*' && c2 == '=') tokens[t].type = TOK_STAREQ;
            else if (c1 == '/' && c2 == '=') tokens[t].type = TOK_SLASHEQ;
            else if (c1 == '%' && c2 == '=') tokens[t].type = TOK_MODEQ;
            else found = 0;
            if (found) { i += 2; t++; continue; }
        }

        /* single-char operators */
        tokens[t].str_val[0] = src[i];
        tokens[t].str_val[1] = 0;
        switch (src[i]) {
            case '+': tokens[t].type = TOK_PLUS; break;
            case '-': tokens[t].type = TOK_MINUS; break;
            case '*': tokens[t].type = TOK_STAR; break;
            case '/': tokens[t].type = TOK_SLASH; break;
            case '%': tokens[t].type = TOK_MOD; break;
            case '=': tokens[t].type = TOK_ASSIGN; break;
            case '<': tokens[t].type = TOK_LT; break;
            case '>': tokens[t].type = TOK_GT; break;
            case '!': tokens[t].type = TOK_NOT; break;
            case '&': tokens[t].type = TOK_BITAND; break;
            case '|': tokens[t].type = TOK_BITOR; break;
            case '^': tokens[t].type = TOK_BITXOR; break;
            case '~': tokens[t].type = TOK_BITNOT; break;
            case '(': tokens[t].type = TOK_LPAREN; break;
            case ')': tokens[t].type = TOK_RPAREN; break;
            case '{': tokens[t].type = TOK_LBRACE; break;
            case '}': tokens[t].type = TOK_RBRACE; break;
            case '[': tokens[t].type = TOK_LBRACKET; break;
            case ']': tokens[t].type = TOK_RBRACKET; break;
            case ';': tokens[t].type = TOK_SEMI; break;
            case ',': tokens[t].type = TOK_COMMA; break;
            default:
                tcc_error("unexpected character");
                return -1;
        }
        i++;
        t++;
    }

    tokens[t].type = TOK_EOF;
    tokens[t].line = line;
    return t + 1;
}

/* ===== variable management ===== */
static var_t *find_var(const char *name) {
    for (int i = num_vars - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    }
    return 0;
}

static var_t *create_var(const char *name, int depth) {
    if (num_vars >= TCC_MAX_VARS) {
        tcc_error("too many variables");
        return 0;
    }
    var_t *v = &vars[num_vars++];
    strncpy(v->name, name, 63);
    v->name[63] = 0;
    v->val.type = VAL_INT;
    v->val.int_val = 0;
    v->val.str_val[0] = 0;
    v->is_array = 0;
    v->array_size = 0;
    v->arr_data = 0;
    v->depth = depth;
    return v;
}

static void set_var(const char *name, value_t val) {
    var_t *v = find_var(name);
    if (!v) {
        v = create_var(name, scope_depth);
        if (!v) return;
    }
    v->val = val;
}

static value_t get_var(const char *name) {
    var_t *v = find_var(name);
    if (v) return v->val;
    tcc_error("undefined variable");
    value_t r = {0};
    return r;
}

static void destroy_scope(int depth) {
    while (num_vars > 0 && vars[num_vars - 1].depth >= depth) {
        num_vars--;
        if (vars[num_vars].is_array && vars[num_vars].arr_data) {
            kfree(vars[num_vars].arr_data);
            vars[num_vars].arr_data = 0;
        }
    }
}

/* ===== function management ===== */
static func_t *find_func(const char *name) {
    for (int i = 0; i < num_funcs; i++) {
        if (strcmp(funcs[i].name, name) == 0) return &funcs[i];
    }
    return 0;
}

/* ===== forward declarations ===== */
static value_t parse_expr(void);
static void parse_stmt(void);
static void parse_block(void);
static value_t call_function(const char *name, value_t *args, int argc);

/* ===== skip a brace-delimited block (for first pass) ===== */
static void skip_block(void) {
    if (peek() != TOK_LBRACE) { tcc_error("expected '{'"); return; }
    int depth = 1;
    advance();
    while (depth > 0 && peek() != TOK_EOF) {
        if (peek() == TOK_LBRACE) depth++;
        else if (peek() == TOK_RBRACE) depth--;
        if (depth > 0) advance();
    }
    if (peek() == TOK_RBRACE) advance();
}

/* ===== built-in functions ===== */
static value_t builtin_printf(value_t *args, int argc) {
    value_t r = {0};
    if (argc < 1 || args[0].type != VAL_STR) {
        tcc_error("printf expects a format string");
        return r;
    }
    const char *fmt = args[0].str_val;
    int ai = 1;
    char buf[16];
    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] == '\\' && fmt[i+1] == 'n') {
            kprint_newline();
            i++;
            continue;
        }
        if (fmt[i] == '%' && fmt[i+1]) {
            i++;
            switch (fmt[i]) {
                case 'd': case 'i':
                    if (ai < argc) {
                        snprintf(buf, 16, "%d", args[ai++].int_val);
                        kprint(buf);
                    }
                    break;
                case 'u':
                    if (ai < argc) {
                        snprintf(buf, 16, "%u", (unsigned)args[ai++].int_val);
                        kprint(buf);
                    }
                    break;
                case 'x':
                    if (ai < argc) {
                        snprintf(buf, 16, "%x", (unsigned)args[ai++].int_val);
                        kprint(buf);
                    }
                    break;
                case 's':
                    if (ai < argc) kprint(args[ai++].str_val);
                    break;
                case 'c':
                    if (ai < argc) {
                        buf[0] = (char)args[ai++].int_val;
                        buf[1] = 0;
                        kprint(buf);
                    }
                    break;
                case '%':
                    kprint("%");
                    break;
                default:
                    buf[0] = '%'; buf[1] = fmt[i]; buf[2] = 0;
                    kprint(buf);
                    break;
            }
        } else if (fmt[i] == '\n') {
            kprint_newline();
        } else {
            buf[0] = fmt[i]; buf[1] = 0;
            kprint(buf);
        }
    }
    return r;
}

static value_t builtin_puts(value_t *args, int argc) {
    value_t r = {0};
    if (argc >= 1) {
        if (args[0].type == VAL_STR) kprint(args[0].str_val);
        else tcc_print_int(args[0].int_val);
        kprint_newline();
    }
    return r;
}

static value_t builtin_putchar(value_t *args, int argc) {
    value_t r = {0};
    if (argc >= 1) {
        char buf[2];
        buf[0] = (char)args[0].int_val;
        buf[1] = 0;
        kprint(buf);
    }
    return r;
}

static value_t builtin_strlen_func(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_INT;
    if (argc >= 1 && args[0].type == VAL_STR)
        r.int_val = strlen(args[0].str_val);
    return r;
}

static value_t builtin_strcmp_func(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_INT;
    if (argc >= 2 && args[0].type == VAL_STR && args[1].type == VAL_STR)
        r.int_val = strcmp(args[0].str_val, args[1].str_val);
    return r;
}

static value_t builtin_atoi_func(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_INT;
    if (argc >= 1 && args[0].type == VAL_STR) {
        int val = 0, sign = 1, j = 0;
        if (args[0].str_val[0] == '-') { sign = -1; j = 1; }
        while (args[0].str_val[j] >= '0' && args[0].str_val[j] <= '9') {
            val = val * 10 + (args[0].str_val[j] - '0');
            j++;
        }
        r.int_val = sign * val;
    }
    return r;
}

static value_t builtin_abs_func(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_INT;
    if (argc >= 1) r.int_val = args[0].int_val < 0 ? -args[0].int_val : args[0].int_val;
    return r;
}

static value_t builtin_input(value_t *args, int argc) {
    (void)args; (void)argc;
    value_t r = {0};
    r.type = VAL_STR;
    char *line = rec_input();
    if (line) strncpy(r.str_val, line, TCC_MAX_STR - 1);
    r.str_val[TCC_MAX_STR - 1] = 0;
    return r;
}

static value_t builtin_itoa_func(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_STR;
    if (argc >= 1)
        snprintf(r.str_val, TCC_MAX_STR, "%d", args[0].int_val);
    return r;
}

static value_t builtin_print(value_t *args, int argc) {
    value_t r = {0};
    for (int i = 0; i < argc; i++) {
        if (args[i].type == VAL_STR) kprint(args[i].str_val);
        else tcc_print_int(args[i].int_val);
    }
    return r;
}

static value_t builtin_println(value_t *args, int argc) {
    builtin_print(args, argc);
    kprint_newline();
    value_t r = {0};
    return r;
}

static value_t builtin_sizeof_func(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_INT;
    if (argc >= 1) {
        if (args[0].type == VAL_STR)
            r.int_val = strlen(args[0].str_val);
        else
            r.int_val = 4;
    }
    return r;
}

/* ===== sandbox builtins ===== */

static value_t builtin_clear(value_t *args, int argc) {
    (void)args; (void)argc;
    clear_screen();
    value_t r = {0};
    return r;
}

static value_t builtin_sleep(value_t *args, int argc) {
    (void)argc;
    value_t r = {0};
    if (argc >= 1) {
        /* Busy-wait using uptime counter (argument in seconds) */
        uint32_t start = get_uptime_seconds();
        uint32_t wait = (uint32_t)args[0].int_val;
        if (wait > 30) wait = 30; /* cap at 30s to prevent lockup */
        while (get_uptime_seconds() - start < wait)
            __asm__ volatile("hlt");
    }
    return r;
}

static value_t builtin_get_key(value_t *args, int argc) {
    (void)args; (void)argc;
    value_t r = {0};
    r.type = VAL_INT;
    /* Wait for a keypress and return the ASCII value */
    char *line = rec_input();
    if (line && line[0])
        r.int_val = (int)(unsigned char)line[0];
    return r;
}

static value_t builtin_read_file(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_STR;
    r.str_val[0] = '\0';
    if (argc >= 1 && args[0].type == VAL_STR) {
        static char tcc_fbuf[4096];
        int bytes = fat16_read_file(args[0].str_val, tcc_fbuf, 4095);
        if (bytes > 0) {
            tcc_fbuf[bytes] = '\0';
            strncpy(r.str_val, tcc_fbuf, TCC_MAX_STR - 1);
            r.str_val[TCC_MAX_STR - 1] = '\0';
        }
    }
    return r;
}

static value_t builtin_write_file(value_t *args, int argc) {
    value_t r = {0};
    r.type = VAL_INT;
    r.int_val = -1;
    if (argc >= 2 && args[0].type == VAL_STR && args[1].type == VAL_STR) {
        fat16_delete_file(args[0].str_val);
        r.int_val = fat16_create_file(args[0].str_val, args[1].str_val);
    }
    return r;
}

static uint32_t tcc_rand_seed = 12345;
static value_t builtin_rand(value_t *args, int argc) {
    (void)args; (void)argc;
    value_t r = {0};
    r.type = VAL_INT;
    /* Simple LCG PRNG */
    tcc_rand_seed = tcc_rand_seed * 1103515245 + 12345;
    r.int_val = (int)((tcc_rand_seed >> 16) & 0x7FFF);
    return r;
}

static value_t builtin_srand(value_t *args, int argc) {
    value_t r = {0};
    if (argc >= 1) tcc_rand_seed = (uint32_t)args[0].int_val;
    return r;
}

static value_t builtin_exit_func(value_t *args, int argc) {
    (void)args; (void)argc;
    value_t r = {0};
    /* Force interpreter to stop */
    had_error = 1;
    return r;
}

static value_t builtin_uptime(value_t *args, int argc) {
    (void)args; (void)argc;
    value_t r = {0};
    r.type = VAL_INT;
    r.int_val = (int)get_uptime_seconds();
    return r;
}

static int is_builtin(const char *name) {
    return strcmp(name, "printf") == 0 || strcmp(name, "puts") == 0 ||
           strcmp(name, "putchar") == 0 || strcmp(name, "strlen") == 0 ||
           strcmp(name, "strcmp") == 0 || strcmp(name, "atoi") == 0 ||
           strcmp(name, "abs") == 0 || strcmp(name, "input") == 0 ||
           strcmp(name, "itoa") == 0 || strcmp(name, "print") == 0 ||
           strcmp(name, "println") == 0 || strcmp(name, "sizeof") == 0 ||
           strcmp(name, "clear") == 0 || strcmp(name, "sleep") == 0 ||
           strcmp(name, "get_key") == 0 || strcmp(name, "read_file") == 0 ||
           strcmp(name, "write_file") == 0 || strcmp(name, "rand") == 0 ||
           strcmp(name, "srand") == 0 || strcmp(name, "exit") == 0 ||
           strcmp(name, "uptime") == 0;
}

static value_t call_builtin(const char *name, value_t *args, int argc) {
    if (strcmp(name, "printf") == 0) return builtin_printf(args, argc);
    if (strcmp(name, "puts") == 0) return builtin_puts(args, argc);
    if (strcmp(name, "putchar") == 0) return builtin_putchar(args, argc);
    if (strcmp(name, "strlen") == 0) return builtin_strlen_func(args, argc);
    if (strcmp(name, "strcmp") == 0) return builtin_strcmp_func(args, argc);
    if (strcmp(name, "atoi") == 0) return builtin_atoi_func(args, argc);
    if (strcmp(name, "abs") == 0) return builtin_abs_func(args, argc);
    if (strcmp(name, "input") == 0) return builtin_input(args, argc);
    if (strcmp(name, "itoa") == 0) return builtin_itoa_func(args, argc);
    if (strcmp(name, "print") == 0) return builtin_print(args, argc);
    if (strcmp(name, "println") == 0) return builtin_println(args, argc);
    if (strcmp(name, "sizeof") == 0) return builtin_sizeof_func(args, argc);
    if (strcmp(name, "clear") == 0) return builtin_clear(args, argc);
    if (strcmp(name, "sleep") == 0) return builtin_sleep(args, argc);
    if (strcmp(name, "get_key") == 0) return builtin_get_key(args, argc);
    if (strcmp(name, "read_file") == 0) return builtin_read_file(args, argc);
    if (strcmp(name, "write_file") == 0) return builtin_write_file(args, argc);
    if (strcmp(name, "rand") == 0) return builtin_rand(args, argc);
    if (strcmp(name, "srand") == 0) return builtin_srand(args, argc);
    if (strcmp(name, "exit") == 0) return builtin_exit_func(args, argc);
    if (strcmp(name, "uptime") == 0) return builtin_uptime(args, argc);
    value_t r = {0};
    return r;
}

/* ===== expression parser (recursive descent with precedence) ===== */

static value_t parse_primary(void) {
    value_t r = {0};
    if (had_error) return r;

    /* number literal */
    if (peek() == TOK_NUM) {
        r.type = VAL_INT;
        r.int_val = tokens[pos].num_val;
        advance();
        return r;
    }

    /* string literal */
    if (peek() == TOK_STR) {
        r.type = VAL_STR;
        strncpy(r.str_val, tokens[pos].str_val, TCC_MAX_STR - 1);
        r.str_val[TCC_MAX_STR - 1] = 0;
        advance();
        return r;
    }

    /* parenthesized expression */
    if (peek() == TOK_LPAREN) {
        advance();
        r = parse_expr();
        expect(TOK_RPAREN);
        return r;
    }

    /* identifier, function call, array access */
    if (peek() == TOK_IDENT) {
        char name[64];
        strncpy(name, tokens[pos].str_val, 63);
        name[63] = 0;
        advance();

        /* function call: name(...) */
        if (peek() == TOK_LPAREN) {
            advance();
            value_t args[TCC_MAX_ARGS];
            int argc = 0;
            while (peek() != TOK_RPAREN && peek() != TOK_EOF && !had_error) {
                if (argc > 0) expect(TOK_COMMA);
                if (argc < TCC_MAX_ARGS)
                    args[argc] = parse_expr();
                argc++;
            }
            expect(TOK_RPAREN);
            return call_function(name, args, argc);
        }

        /* array access: name[index] */
        if (peek() == TOK_LBRACKET) {
            advance();
            value_t idx = parse_expr();
            expect(TOK_RBRACKET);
            var_t *v = find_var(name);
            if (!v) { tcc_error("undefined variable"); return r; }
            if (v->is_array && v->arr_data) {
                int i = idx.int_val;
                if (i >= 0 && i < v->array_size) {
                    r.type = VAL_INT;
                    r.int_val = v->arr_data[i];
                }
            } else if (v->val.type == VAL_STR) {
                /* string indexing */
                int i = idx.int_val;
                if (i >= 0 && i < (int)strlen(v->val.str_val)) {
                    r.type = VAL_INT;
                    r.int_val = (unsigned char)v->val.str_val[i];
                }
            }
            return r;
        }

        /* post increment/decrement */
        if (peek() == TOK_INC) {
            advance();
            var_t *v = find_var(name);
            if (v) { r = v->val; v->val.int_val++; return r; }
        }
        if (peek() == TOK_DEC) {
            advance();
            var_t *v = find_var(name);
            if (v) { r = v->val; v->val.int_val--; return r; }
        }

        /* plain variable */
        return get_var(name);
    }

    tcc_error("unexpected token in expression");
    return r;
}

static value_t parse_unary(void) {
    if (had_error) { value_t r = {0}; return r; }

    if (peek() == TOK_MINUS) {
        advance();
        value_t v = parse_unary();
        v.int_val = -v.int_val;
        v.type = VAL_INT;
        return v;
    }
    if (peek() == TOK_NOT) {
        advance();
        value_t v = parse_unary();
        v.int_val = !v.int_val;
        v.type = VAL_INT;
        return v;
    }
    if (peek() == TOK_BITNOT) {
        advance();
        value_t v = parse_unary();
        v.int_val = ~v.int_val;
        v.type = VAL_INT;
        return v;
    }
    /* pre-increment */
    if (peek() == TOK_INC && tokens[pos + 1].type == TOK_IDENT) {
        advance();
        char name[64];
        strncpy(name, tokens[pos].str_val, 63);
        name[63] = 0;
        advance();
        var_t *v = find_var(name);
        if (v) { v->val.int_val++; return v->val; }
        value_t r = {0};
        return r;
    }
    if (peek() == TOK_DEC && tokens[pos + 1].type == TOK_IDENT) {
        advance();
        char name[64];
        strncpy(name, tokens[pos].str_val, 63);
        name[63] = 0;
        advance();
        var_t *v = find_var(name);
        if (v) { v->val.int_val--; return v->val; }
        value_t r = {0};
        return r;
    }
    return parse_primary();
}

static value_t parse_mul(void) {
    value_t left = parse_unary();
    while (!had_error && (peek() == TOK_STAR || peek() == TOK_SLASH || peek() == TOK_MOD)) {
        int op = peek(); advance();
        value_t right = parse_unary();
        if (op == TOK_STAR) left.int_val *= right.int_val;
        else if (op == TOK_SLASH) {
            if (right.int_val == 0) { tcc_error("division by zero"); return left; }
            left.int_val /= right.int_val;
        }
        else left.int_val %= right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_add(void) {
    value_t left = parse_mul();
    while (!had_error && (peek() == TOK_PLUS || peek() == TOK_MINUS)) {
        int op = peek(); advance();
        value_t right = parse_mul();
        if (op == TOK_PLUS) {
            /* string concatenation */
            if (left.type == VAL_STR || right.type == VAL_STR) {
                char buf[TCC_MAX_STR];
                buf[0] = 0;
                if (left.type == VAL_STR) strncpy(buf, left.str_val, TCC_MAX_STR - 1);
                else snprintf(buf, TCC_MAX_STR, "%d", left.int_val);
                int blen = strlen(buf);
                if (right.type == VAL_STR) strncpy(buf + blen, right.str_val, TCC_MAX_STR - 1 - blen);
                else snprintf(buf + blen, TCC_MAX_STR - blen, "%d", right.int_val);
                buf[TCC_MAX_STR - 1] = 0;
                left.type = VAL_STR;
                strcpy(left.str_val, buf);
            } else {
                left.int_val += right.int_val;
                left.type = VAL_INT;
            }
        } else {
            left.int_val -= right.int_val;
            left.type = VAL_INT;
        }
    }
    return left;
}

static value_t parse_shift(void) {
    value_t left = parse_add();
    while (!had_error && (peek() == TOK_SHL || peek() == TOK_SHR)) {
        int op = peek(); advance();
        value_t right = parse_add();
        if (op == TOK_SHL) left.int_val <<= right.int_val;
        else left.int_val >>= right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_comparison(void) {
    value_t left = parse_shift();
    while (!had_error && (peek() == TOK_LT || peek() == TOK_GT || peek() == TOK_LEQ || peek() == TOK_GEQ)) {
        int op = peek(); advance();
        value_t right = parse_shift();
        if (op == TOK_LT) left.int_val = left.int_val < right.int_val;
        else if (op == TOK_GT) left.int_val = left.int_val > right.int_val;
        else if (op == TOK_LEQ) left.int_val = left.int_val <= right.int_val;
        else left.int_val = left.int_val >= right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_equality(void) {
    value_t left = parse_comparison();
    while (!had_error && (peek() == TOK_EQ || peek() == TOK_NEQ)) {
        int op = peek(); advance();
        value_t right = parse_comparison();
        int eq;
        if (left.type == VAL_STR && right.type == VAL_STR)
            eq = (strcmp(left.str_val, right.str_val) == 0);
        else
            eq = (left.int_val == right.int_val);
        left.int_val = (op == TOK_EQ) ? eq : !eq;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_bitand(void) {
    value_t left = parse_equality();
    while (!had_error && peek() == TOK_BITAND) {
        advance();
        value_t right = parse_equality();
        left.int_val &= right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_bitxor(void) {
    value_t left = parse_bitand();
    while (!had_error && peek() == TOK_BITXOR) {
        advance();
        value_t right = parse_bitand();
        left.int_val ^= right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_bitor(void) {
    value_t left = parse_bitxor();
    while (!had_error && peek() == TOK_BITOR) {
        advance();
        value_t right = parse_bitxor();
        left.int_val |= right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_logic_and(void) {
    value_t left = parse_bitor();
    while (!had_error && peek() == TOK_AND) {
        advance();
        value_t right = parse_bitor();
        left.int_val = left.int_val && right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_logic_or(void) {
    value_t left = parse_logic_and();
    while (!had_error && peek() == TOK_OR) {
        advance();
        value_t right = parse_logic_and();
        left.int_val = left.int_val || right.int_val;
        left.type = VAL_INT;
    }
    return left;
}

static value_t parse_assignment(void) {
    /* handle: ident = expr, ident += expr, etc. */
    if (peek() == TOK_IDENT) {
        int saved = pos;
        char name[64];
        strncpy(name, tokens[pos].str_val, 63);
        name[63] = 0;

        /* ident[idx] = expr */
        if (tokens[pos + 1].type == TOK_LBRACKET) {
            advance(); advance(); /* skip ident and [ */
            value_t idx = parse_expr();
            expect(TOK_RBRACKET);
            if (peek() == TOK_ASSIGN) {
                advance();
                value_t val = parse_assignment();
                var_t *v = find_var(name);
                if (v && v->is_array && v->arr_data) {
                    int i = idx.int_val;
                    if (i >= 0 && i < v->array_size)
                        v->arr_data[i] = val.int_val;
                } else if (v && v->val.type == VAL_STR) {
                    int i = idx.int_val;
                    if (i >= 0 && i < (int)strlen(v->val.str_val))
                        v->val.str_val[i] = (char)val.int_val;
                }
                return val;
            }
            /* not an assignment - backtrack */
            pos = saved;
            return parse_logic_or();
        }

        /* ident = expr */
        if (tokens[pos + 1].type == TOK_ASSIGN) {
            advance(); advance();
            value_t val = parse_assignment();
            set_var(name, val);
            return val;
        }
        /* ident += expr */
        if (tokens[pos + 1].type == TOK_PLUSEQ) {
            advance(); advance();
            value_t right = parse_assignment();
            value_t left = get_var(name);
            if (left.type == VAL_STR || right.type == VAL_STR) {
                int blen = strlen(left.str_val);
                if (right.type == VAL_STR) strncpy(left.str_val + blen, right.str_val, TCC_MAX_STR - 1 - blen);
                else snprintf(left.str_val + blen, TCC_MAX_STR - blen, "%d", right.int_val);
            } else {
                left.int_val += right.int_val;
            }
            set_var(name, left);
            return left;
        }
        if (tokens[pos + 1].type == TOK_MINUSEQ) {
            advance(); advance();
            value_t right = parse_assignment();
            value_t left = get_var(name);
            left.int_val -= right.int_val;
            set_var(name, left);
            return left;
        }
        if (tokens[pos + 1].type == TOK_STAREQ) {
            advance(); advance();
            value_t right = parse_assignment();
            value_t left = get_var(name);
            left.int_val *= right.int_val;
            set_var(name, left);
            return left;
        }
        if (tokens[pos + 1].type == TOK_SLASHEQ) {
            advance(); advance();
            value_t right = parse_assignment();
            value_t left = get_var(name);
            if (right.int_val != 0) left.int_val /= right.int_val;
            set_var(name, left);
            return left;
        }
        if (tokens[pos + 1].type == TOK_MODEQ) {
            advance(); advance();
            value_t right = parse_assignment();
            value_t left = get_var(name);
            if (right.int_val != 0) left.int_val %= right.int_val;
            set_var(name, left);
            return left;
        }
    }
    return parse_logic_or();
}

static value_t parse_expr(void) {
    return parse_assignment();
}

/* ===== statement parser ===== */

static void parse_var_decl(void) {
    if (had_error) return;
    /* skip type keywords (int, char, void, const) and pointer stars */
    while (is_type_tok(peek())) advance();
    while (peek() == TOK_STAR) advance();

    if (peek() != TOK_IDENT) { tcc_error("expected variable name"); return; }
    char name[64];
    strncpy(name, tokens[pos].str_val, 63);
    name[63] = 0;
    advance();

    /* array declaration: name[size] */
    if (peek() == TOK_LBRACKET) {
        advance();
        int arr_size = 0;
        if (peek() == TOK_NUM) { arr_size = tokens[pos].num_val; advance(); }
        expect(TOK_RBRACKET);

        var_t *v = create_var(name, scope_depth);
        if (!v) return;
        v->is_array = 1;

        /* initializer list: = {1, 2, 3} */
        if (peek() == TOK_ASSIGN) {
            advance();
            if (peek() == TOK_LBRACE) {
                advance();
                int count = 0;
                int temp[TCC_MAX_ARRAY];
                while (peek() != TOK_RBRACE && peek() != TOK_EOF && !had_error && count < TCC_MAX_ARRAY) {
                    if (count > 0) expect(TOK_COMMA);
                    value_t val = parse_expr();
                    temp[count++] = val.int_val;
                }
                expect(TOK_RBRACE);
                if (arr_size == 0) arr_size = count;
                v->array_size = arr_size;
                v->arr_data = (int *)tcc_alloc(arr_size * sizeof(int));
                if (v->arr_data) {
                    memset(v->arr_data, 0, arr_size * sizeof(int));
                    for (int i = 0; i < count && i < arr_size; i++)
                        v->arr_data[i] = temp[i];
                }
            } else if (peek() == TOK_STR) {
                /* char name[] = "hello" */
                const char *s = tokens[pos].str_val;
                int slen = strlen(s);
                if (arr_size == 0) arr_size = slen + 1;
                v->array_size = arr_size;
                v->arr_data = (int *)tcc_alloc(arr_size * sizeof(int));
                if (v->arr_data) {
                    for (int i = 0; i < slen && i < arr_size; i++)
                        v->arr_data[i] = (unsigned char)s[i];
                    if (slen < arr_size)
                        v->arr_data[slen] = 0;
                }
                /* also store as string value for convenience */
                v->val.type = VAL_STR;
                strncpy(v->val.str_val, s, TCC_MAX_STR - 1);
                advance();
            }
        } else {
            if (arr_size == 0) arr_size = 16;
            v->array_size = arr_size;
            v->arr_data = (int *)tcc_alloc(arr_size * sizeof(int));
            if (v->arr_data) memset(v->arr_data, 0, arr_size * sizeof(int));
        }

        /* handle multiple declarators: int a[5], b[3]; */
        while (peek() == TOK_COMMA) {
            advance();
            if (peek() != TOK_IDENT) break;
            char name2[64];
            strncpy(name2, tokens[pos].str_val, 63);
            name2[63] = 0;
            advance();
            /* simplified: just create same-sized array */
            var_t *v2 = create_var(name2, scope_depth);
            if (v2) {
                v2->is_array = 1;
                v2->array_size = arr_size;
                v2->arr_data = (int *)tcc_alloc(arr_size * sizeof(int));
                if (v2->arr_data) memset(v2->arr_data, 0, arr_size * sizeof(int));
            }
        }
        expect(TOK_SEMI);
        return;
    }

    /* simple variable with optional initializer */
    var_t *v = create_var(name, scope_depth);
    if (!v) { expect(TOK_SEMI); return; }

    if (peek() == TOK_ASSIGN) {
        advance();
        v->val = parse_expr();
    }

    /* handle multiple declarators: int a = 1, b = 2; */
    while (peek() == TOK_COMMA) {
        advance();
        while (peek() == TOK_STAR) advance();
        if (peek() != TOK_IDENT) break;
        char name2[64];
        strncpy(name2, tokens[pos].str_val, 63);
        name2[63] = 0;
        advance();
        var_t *v2 = create_var(name2, scope_depth);
        if (v2 && peek() == TOK_ASSIGN) {
            advance();
            v2->val = parse_expr();
        }
    }
    expect(TOK_SEMI);
}

static void parse_block(void) {
    if (had_error) return;
    expect(TOK_LBRACE);
    scope_depth++;
    while (peek() != TOK_RBRACE && peek() != TOK_EOF && !had_error && exec_flag == EXEC_NORMAL) {
        parse_stmt();
    }
    /* still consume tokens if we hit break/continue/return */
    if (exec_flag != EXEC_NORMAL) {
        int depth = 1;
        while (depth > 0 && peek() != TOK_EOF) {
            if (peek() == TOK_LBRACE) depth++;
            else if (peek() == TOK_RBRACE) { depth--; if (depth == 0) break; }
            advance();
        }
    }
    destroy_scope(scope_depth);
    scope_depth--;
    if (peek() == TOK_RBRACE) advance();
}

static void parse_if(void) {
    if (had_error) return;
    advance(); /* skip 'if' */
    expect(TOK_LPAREN);
    value_t cond = parse_expr();
    expect(TOK_RPAREN);

    if (cond.int_val || (cond.type == VAL_STR && cond.str_val[0])) {
        parse_stmt();
        /* skip else branch */
        if (peek() == TOK_KW_ELSE) {
            advance();
            int saved_exec = exec_flag;
            exec_flag = EXEC_NORMAL;
            /* skip the else body without executing */
            if (peek() == TOK_LBRACE) skip_block();
            else {
                /* single statement - skip it */
                if (peek() == TOK_KW_IF) {
                    /* else if chain - skip recursively */
                    advance();
                    expect(TOK_LPAREN);
                    int pdepth = 1;
                    while (pdepth > 0 && peek() != TOK_EOF) {
                        if (peek() == TOK_LPAREN) pdepth++;
                        else if (peek() == TOK_RPAREN) pdepth--;
                        advance();
                    }
                    if (peek() == TOK_LBRACE) skip_block();
                    else { while (peek() != TOK_SEMI && peek() != TOK_EOF) advance(); if (peek() == TOK_SEMI) advance(); }
                    if (peek() == TOK_KW_ELSE) {
                        advance();
                        if (peek() == TOK_LBRACE) skip_block();
                        else { while (peek() != TOK_SEMI && peek() != TOK_EOF) advance(); if (peek() == TOK_SEMI) advance(); }
                    }
                } else {
                    while (peek() != TOK_SEMI && peek() != TOK_EOF) advance();
                    if (peek() == TOK_SEMI) advance();
                }
            }
            exec_flag = saved_exec;
        }
    } else {
        /* skip true branch without executing */
        if (peek() == TOK_LBRACE) skip_block();
        else {
            while (peek() != TOK_SEMI && peek() != TOK_EOF) advance();
            if (peek() == TOK_SEMI) advance();
        }
        /* execute else branch if present */
        if (peek() == TOK_KW_ELSE) {
            advance();
            parse_stmt();
        }
    }
}

static void parse_while(void) {
    if (had_error) return;
    advance(); /* skip 'while' */
    int cond_pos = pos;

    for (;;) {
        if (had_error) break;
        pos = cond_pos;
        expect(TOK_LPAREN);
        value_t cond = parse_expr();
        expect(TOK_RPAREN);

        if (!cond.int_val && !(cond.type == VAL_STR && cond.str_val[0])) {
            /* condition false - skip body */
            if (peek() == TOK_LBRACE) skip_block();
            else { while (peek() != TOK_SEMI && peek() != TOK_EOF) advance(); if (peek() == TOK_SEMI) advance(); }
            break;
        }

        /* execute body */
        int body_start = pos;
        parse_stmt();

        if (exec_flag == EXEC_BREAK) { exec_flag = EXEC_NORMAL; break; }
        if (exec_flag == EXEC_CONTINUE) { exec_flag = EXEC_NORMAL; continue; }
        if (exec_flag == EXEC_RETURN) break;
    }

    /* make sure we're past the loop body */
    if (exec_flag != EXEC_RETURN) {
        /* already past from the skip above */
    }
}

static void parse_for(void) {
    if (had_error) return;
    advance(); /* skip 'for' */
    expect(TOK_LPAREN);

    scope_depth++;

    /* init */
    if (peek() == TOK_SEMI) {
        advance();
    } else if (is_type_tok(peek())) {
        parse_var_decl();
    } else {
        parse_expr();
        expect(TOK_SEMI);
    }

    int cond_pos = pos;

    /* we need to find the update and body positions */
    /* first, figure out where the condition ends and update begins */

    for (;;) {
        if (had_error) break;

        /* evaluate condition */
        pos = cond_pos;
        int has_cond = (peek() != TOK_SEMI);
        value_t cond = {0};
        cond.int_val = 1;
        if (has_cond) cond = parse_expr();
        expect(TOK_SEMI);
        int update_pos = pos;

        /* skip update to find body */
        if (peek() != TOK_RPAREN) {
            int pdepth = 0;
            while (peek() != TOK_EOF) {
                if (peek() == TOK_LPAREN) pdepth++;
                else if (peek() == TOK_RPAREN) { if (pdepth == 0) break; pdepth--; }
                advance();
            }
        }
        expect(TOK_RPAREN);
        int body_pos = pos;

        if (!cond.int_val) {
            /* condition false - skip body */
            if (peek() == TOK_LBRACE) skip_block();
            else { while (peek() != TOK_SEMI && peek() != TOK_EOF) advance(); if (peek() == TOK_SEMI) advance(); }
            break;
        }

        /* execute body */
        parse_stmt();

        if (exec_flag == EXEC_BREAK) { exec_flag = EXEC_NORMAL; break; }
        if (exec_flag == EXEC_CONTINUE) exec_flag = EXEC_NORMAL;
        if (exec_flag == EXEC_RETURN) break;

        /* execute update */
        pos = update_pos;
        if (peek() != TOK_RPAREN) parse_expr();
    }

    destroy_scope(scope_depth);
    scope_depth--;
}

static void parse_stmt(void) {
    if (had_error || exec_flag != EXEC_NORMAL) return;
    sandbox_tick();
    if (had_error) return;

    /* block */
    if (peek() == TOK_LBRACE) { parse_block(); return; }

    /* if */
    if (peek() == TOK_KW_IF) { parse_if(); return; }

    /* while */
    if (peek() == TOK_KW_WHILE) { parse_while(); return; }

    /* for */
    if (peek() == TOK_KW_FOR) { parse_for(); return; }

    /* return */
    if (peek() == TOK_KW_RETURN) {
        advance();
        if (peek() != TOK_SEMI) return_val = parse_expr();
        else { return_val.type = VAL_INT; return_val.int_val = 0; }
        expect(TOK_SEMI);
        exec_flag = EXEC_RETURN;
        return;
    }

    /* break */
    if (peek() == TOK_KW_BREAK) {
        advance(); expect(TOK_SEMI);
        exec_flag = EXEC_BREAK;
        return;
    }

    /* continue */
    if (peek() == TOK_KW_CONTINUE) {
        advance(); expect(TOK_SEMI);
        exec_flag = EXEC_CONTINUE;
        return;
    }

    /* variable declaration */
    if (is_type_tok(peek())) {
        parse_var_decl();
        return;
    }

    /* expression statement (including assignments) */
    parse_expr();
    expect(TOK_SEMI);
}

/* ===== function calling ===== */
static value_t call_function(const char *name, value_t *args, int argc) {
    value_t r = {0};
    if (had_error) return r;

    /* built-ins first */
    if (is_builtin(name)) return call_builtin(name, args, argc);

    /* user-defined function */
    func_t *fn = find_func(name);
    if (!fn) {
        tcc_error("undefined function");
        return r;
    }

    if (call_depth > 64) {
        tcc_error("call stack overflow");
        return r;
    }
    call_depth++;

    int saved_pos = pos;
    int saved_depth = scope_depth;
    int saved_nvars = num_vars;

    scope_depth++;

    /* set up parameters */
    for (int i = 0; i < fn->param_count && i < argc; i++) {
        var_t *pv = create_var(fn->params[i], scope_depth);
        if (pv) pv->val = args[i];
    }

    /* jump to function body and execute */
    pos = fn->body_pos;
    parse_block();

    r = return_val;
    if (exec_flag == EXEC_RETURN) exec_flag = EXEC_NORMAL;

    /* clean up */
    destroy_scope(scope_depth);
    scope_depth = saved_depth;
    pos = saved_pos;
    call_depth--;

    return r;
}

/* ===== first pass: collect function definitions ===== */
static void parse_func_def(void) {
    /* already at the type keyword */
    advance(); /* skip return type */
    while (peek() == TOK_STAR) advance();

    if (peek() != TOK_IDENT) { tcc_error("expected function name"); return; }

    if (num_funcs >= TCC_MAX_FUNCS) { tcc_error("too many functions"); return; }
    func_t *fn = &funcs[num_funcs];
    strncpy(fn->name, tokens[pos].str_val, 63);
    fn->name[63] = 0;
    advance();

    expect(TOK_LPAREN);
    fn->param_count = 0;
    while (peek() != TOK_RPAREN && peek() != TOK_EOF && !had_error) {
        if (fn->param_count > 0) expect(TOK_COMMA);
        /* skip type */
        if (is_type_tok(peek())) advance();
        while (peek() == TOK_STAR) advance();
        /* param name */
        if (peek() == TOK_IDENT) {
            strncpy(fn->params[fn->param_count], tokens[pos].str_val, 63);
            fn->params[fn->param_count][63] = 0;
            advance();
        }
        /* skip array brackets in params: int arr[] */
        if (peek() == TOK_LBRACKET) { advance(); if (peek() != TOK_RBRACKET) advance(); expect(TOK_RBRACKET); }
        fn->param_count++;
        if (fn->param_count >= TCC_MAX_PARAMS) break;
    }
    expect(TOK_RPAREN);

    fn->body_pos = pos;
    num_funcs++;
    skip_block();
}

static void parse_global_decl(void) {
    /* skip type + optional stars */
    while (is_type_tok(peek())) advance();
    while (peek() == TOK_STAR) advance();

    if (peek() != TOK_IDENT) { tcc_error("expected identifier"); return; }
    char name[64];
    strncpy(name, tokens[pos].str_val, 63);
    name[63] = 0;
    advance();

    var_t *v = create_var(name, 0);
    if (!v) { while (peek() != TOK_SEMI && peek() != TOK_EOF) advance(); if (peek() == TOK_SEMI) advance(); return; }

    if (peek() == TOK_LBRACKET) {
        /* global array */
        advance();
        int sz = 16;
        if (peek() == TOK_NUM) { sz = tokens[pos].num_val; advance(); }
        expect(TOK_RBRACKET);
        v->is_array = 1;
        v->array_size = sz;
        v->arr_data = (int *)tcc_alloc(sz * sizeof(int));
        if (v->arr_data) memset(v->arr_data, 0, sz * sizeof(int));
    }

    if (peek() == TOK_ASSIGN) {
        advance();
        if (peek() == TOK_NUM) {
            v->val.type = VAL_INT;
            v->val.int_val = tokens[pos].num_val;
            advance();
        } else if (peek() == TOK_STR) {
            v->val.type = VAL_STR;
            strncpy(v->val.str_val, tokens[pos].str_val, TCC_MAX_STR - 1);
            advance();
        } else if (peek() == TOK_LBRACE && v->is_array) {
            advance();
            int count = 0;
            while (peek() != TOK_RBRACE && peek() != TOK_EOF && !had_error && count < v->array_size) {
                if (count > 0) expect(TOK_COMMA);
                value_t val = parse_expr();
                if (v->arr_data) v->arr_data[count] = val.int_val;
                count++;
            }
            expect(TOK_RBRACE);
        }
    }
    expect(TOK_SEMI);
}

static void parse_program(void) {
    while (peek() != TOK_EOF && !had_error) {
        if (!is_type_tok(peek())) {
            tcc_error("expected type keyword at top level");
            return;
        }
        /* look ahead: type [*] ident ( -> function def */
        int saved = pos;
        while (is_type_tok(tokens[pos].type)) pos++;
        while (tokens[pos].type == TOK_STAR) pos++;
        int is_func = 0;
        if (tokens[pos].type == TOK_IDENT && tokens[pos + 1].type == TOK_LPAREN) is_func = 1;
        pos = saved;

        if (is_func) parse_func_def();
        else parse_global_decl();
    }
}

/* ===== simple #include preprocessor ===== */
#define TCC_PP_MAX (TCC_SRC_MAX * 2)
static char pp_buf[TCC_PP_MAX];

/*
 * Expand #include "filename" directives by inlining file contents.
 * Only supports one level of includes (no recursive includes).
 * Returns pointer to preprocessed source (static buffer).
 */
static const char *tcc_preprocess(const char *source) {
    int out = 0;
    int i = 0;
    int src_len = strlen(source);

    while (i < src_len && out < TCC_PP_MAX - 2) {
        /* Check for #include at start of line */
        if (source[i] == '#') {
            /* Match #include "filename" */
            if (strncmp(source + i, "#include", 8) == 0) {
                int j = i + 8;
                /* skip whitespace */
                while (j < src_len && (source[j] == ' ' || source[j] == '\t')) j++;
                if (j < src_len && source[j] == '"') {
                    j++; /* skip opening quote */
                    char inc_name[16];
                    int ni = 0;
                    while (j < src_len && source[j] != '"' && ni < 15) {
                        inc_name[ni++] = source[j++];
                    }
                    inc_name[ni] = '\0';
                    if (j < src_len && source[j] == '"') j++;
                    /* skip to end of line */
                    while (j < src_len && source[j] != '\n') j++;
                    if (j < src_len) j++; /* skip newline */

                    /* Read included file */
                    static char inc_buf[4096];
                    int bytes = fat16_read_file(inc_name, inc_buf, 4095);
                    if (bytes > 0) {
                        inc_buf[bytes] = '\0';
                        /* Copy included content */
                        for (int k = 0; k < bytes && out < TCC_PP_MAX - 2; k++)
                            pp_buf[out++] = inc_buf[k];
                        pp_buf[out++] = '\n';
                    }
                    i = j;
                    continue;
                }
            }
        }
        pp_buf[out++] = source[i++];
    }
    pp_buf[out] = '\0';
    return pp_buf;
}

/* ===== public entry points ===== */
void tcc_run_source(const char *source) {
    /* reset state */
    num_tokens = 0;
    pos = 0;
    num_vars = 0;
    num_funcs = 0;
    scope_depth = 0;
    exec_flag = EXEC_NORMAL;
    return_val.type = VAL_INT;
    return_val.int_val = 0;
    had_error = 0;
    call_depth = 0;
    instruction_count = 0;
    alloc_bytes_used = 0;

    /* preprocess #include directives */
    const char *pp_source = tcc_preprocess(source);

    /* tokenize */
    int n = tokenize(pp_source);
    if (n < 0) return;
    num_tokens = n;

    /* first pass: collect functions and global vars */
    parse_program();
    if (had_error) return;

    /* find and call main() */
    func_t *main_fn = find_func("main");
    if (!main_fn) {
        tcc_error("no main() function found");
        return;
    }

    value_t args[1];
    args[0].type = VAL_INT;
    args[0].int_val = 0;
    call_function("main", args, 0);

    /* clean up */
    for (int i = 0; i < num_vars; i++) {
        if (vars[i].is_array && vars[i].arr_data) {
            kfree(vars[i].arr_data);
            vars[i].arr_data = 0;
        }
    }
    num_vars = 0;
}

void tcc_run_file(const char *filename) {
    static char src_buf[TCC_SRC_MAX];
    int bytes = fat16_read_file(filename, src_buf, TCC_SRC_MAX - 1);
    if (bytes < 0) {
        kprint("[tcc] file not found: ");
        kprint(filename);
        kprint_newline();
        return;
    }
    src_buf[bytes] = '\0';
    kprint("[tcc] running ");
    kprint(filename);
    kprint_newline();
    tcc_run_source(src_buf);
}

int tcc_validate(const char *source, char *errmsg, int maxlen) {
    /* Validation: preprocess, tokenize, and try to parse */
    static char pp_buf[TCC_PP_MAX];
    num_tokens = 0;
    pos = 0;
    num_vars = 0;
    num_funcs = 0;
    scope_depth = 0;
    exec_flag = EXEC_NORMAL;
    had_error = 0;
    call_depth = 0;
    instruction_count = 0;
    alloc_bytes_used = 0;

    /* Preprocess #include directives */
    const char *pp_src = tcc_preprocess(source, pp_buf, TCC_PP_MAX);

    int n = tokenize(pp_src);
    if (n < 0) {
        strncpy(errmsg, "tokenize failed", maxlen - 1);
        errmsg[maxlen - 1] = '\0';
        return -1;
    }
    num_tokens = n;

    if (num_tokens == 0) {
        strncpy(errmsg, "no code to validate", maxlen - 1);
        errmsg[maxlen - 1] = '\0';
        return -1;
    }

    /* Try to parse — this will catch syntax errors */
    pos = 0;
    int save_had_error = had_error;
    had_error = 0;

    parse_program();

    if (had_error) {
        strncpy(errmsg, "syntax error detected", maxlen - 1);
        errmsg[maxlen - 1] = '\0';
        had_error = save_had_error;
        return -1;
    }

    /* Check that main exists */
    int found_main = 0;
    for (int i = 0; i < num_funcs; i++) {
        if (strcmp(functions[i].name, "main") == 0) {
            found_main = 1;
            break;
        }
    }
    if (!found_main) {
        strncpy(errmsg, "no main() function found", maxlen - 1);
        errmsg[maxlen - 1] = '\0';
        return -1;
    }

    errmsg[0] = '\0';
    had_error = save_had_error;
    return 0;
}

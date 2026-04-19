/*
 * ToastScript interpreter — tscript.c
 * A line-by-line scripting engine for the toastOS App Engine.
 */

#include "tscript.hpp"
#include "kio.hpp"
#include "toast_libc.hpp"
#include "panic.hpp"

/* ---- limits ---- */
#define TS_MAX_VARS    16
#define TS_NAME_LEN    16
#define TS_VAL_LEN     128
#define TS_MAX_LABELS  32
#define TS_LINE_BUF    256
#define TS_MAX_STEPS   500000

/* ---- variable storage ---- */
typedef struct { char name[TS_NAME_LEN]; char val[TS_VAL_LEN]; int used; } TsVar;
static TsVar ts_vars[TS_MAX_VARS];

static const char *ts_get(const char *name) {
    for (int i = 0; i < TS_MAX_VARS; i++)
        if (ts_vars[i].used && strcmp(ts_vars[i].name, name) == 0)
            return ts_vars[i].val;
    return "";
}

static void ts_set(const char *name, const char *v) {
    for (int i = 0; i < TS_MAX_VARS; i++) {
        if (ts_vars[i].used && strcmp(ts_vars[i].name, name) == 0) {
            strncpy(ts_vars[i].val, v, TS_VAL_LEN - 1);
            ts_vars[i].val[TS_VAL_LEN - 1] = '\0';
            return;
        }
    }
    for (int i = 0; i < TS_MAX_VARS; i++) {
        if (!ts_vars[i].used) {
            strncpy(ts_vars[i].name, name, TS_NAME_LEN - 1);
            ts_vars[i].name[TS_NAME_LEN - 1] = '\0';
            strncpy(ts_vars[i].val, v, TS_VAL_LEN - 1);
            ts_vars[i].val[TS_VAL_LEN - 1] = '\0';
            ts_vars[i].used = 1;
            return;
        }
    }
}

/* ---- label table ---- */
typedef struct { char name[TS_NAME_LEN]; int offset; } TsLabel;
static TsLabel ts_labels[TS_MAX_LABELS];
static int     ts_nlabels;

static int ts_find_label(const char *name) {
    for (int i = 0; i < ts_nlabels; i++)
        if (strcmp(ts_labels[i].name, name) == 0)
            return ts_labels[i].offset;
    return -1;
}

/* ---- tiny helpers ---- */

static int ts_atoi(const char *s) {
    int neg = 0, v = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

static void ts_itoa(int n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    int neg = 0;
    if (n < 0) { neg = 1; n = -n; }
    char tmp[12]; int len = 0;
    while (n > 0) { tmp[len++] = '0' + (n % 10); n /= 10; }
    int i = 0;
    if (neg) buf[i++] = '-';
    while (len > 0) buf[i++] = tmp[--len];
    buf[i] = '\0';
}

/* Skip spaces/tabs, return pointer past them */
static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Copy next whitespace-delimited word from *p into word[],
   advance *p past it and any trailing spaces. */
static void next_word(const char **p, char *word, int maxlen) {
    const char *s = *p;
    int i = 0;
    while (*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r'
           && i < maxlen - 1)
        word[i++] = *s++;
    word[i] = '\0';
    while (*s == ' ' || *s == '\t') s++;
    *p = s;
}

/* Expand $var references in text → out (max maxlen chars) */
static void ts_expand(const char *text, char *out, int maxlen) {
    int oi = 0;
    while (*text && oi < maxlen - 1) {
        if (*text == '$') {
            text++;
            char vn[TS_NAME_LEN];
            int vi = 0;
            while (*text && *text != ' ' && *text != '\t' && *text != '$'
                   && *text != '\n' && *text != '\r' && vi < TS_NAME_LEN - 1)
                vn[vi++] = *text++;
            vn[vi] = '\0';
            const char *v = ts_get(vn);
            while (*v && oi < maxlen - 1)
                out[oi++] = *v++;
        } else {
            out[oi++] = *text++;
        }
    }
    out[oi] = '\0';
}

/* ---- first pass: collect labels ---- */
static void ts_scan_labels(const char *src) {
    ts_nlabels = 0;
    int pos = 0;
    while (src[pos]) {
        int line_start = pos;
        while (src[pos] == ' ' || src[pos] == '\t') pos++;
        if (src[pos] == ':' && src[pos + 1] && src[pos + 1] != '\n'
            && src[pos + 1] != '\r') {
            pos++;
            char name[TS_NAME_LEN]; int ni = 0;
            while (src[pos] && src[pos] != '\n' && src[pos] != '\r'
                   && src[pos] != ' ' && ni < TS_NAME_LEN - 1)
                name[ni++] = src[pos++];
            name[ni] = '\0';
            if (ts_nlabels < TS_MAX_LABELS) {
                strncpy(ts_labels[ts_nlabels].name, name, TS_NAME_LEN - 1);
                ts_labels[ts_nlabels].name[TS_NAME_LEN - 1] = '\0';
                ts_labels[ts_nlabels].offset = line_start;
                ts_nlabels++;
            }
        }
        while (src[pos] && src[pos] != '\n') pos++;
        if (src[pos] == '\n') pos++;
    }
}

/* ---- execution engine ---- */

extern char read_port(unsigned short port);

int tscript_run(const char *src) {
    /* Reset state */
    for (int i = 0; i < TS_MAX_VARS; i++) ts_vars[i].used = 0;
    ts_scan_labels(src);

    clear_screen();
    kprint_newline();

    int pos   = 0;
    int steps = TS_MAX_STEPS;
    int skip  = 0;          /* nesting depth inside a false if-block */

    while (src[pos] && steps-- > 0) {
        /* ---- extract one line ---- */
        while (src[pos] == ' ' || src[pos] == '\t') pos++;

        char line[TS_LINE_BUF];
        int li = 0;
        while (src[pos] && src[pos] != '\n' && src[pos] != '\r'
               && li < TS_LINE_BUF - 1)
            line[li++] = src[pos++];
        line[li] = '\0';
        if (src[pos] == '\r') pos++;
        if (src[pos] == '\n') pos++;

        /* blank / comment / label → skip */
        if (line[0] == '\0' || line[0] == '#') continue;
        if (line[0] == ':')                    continue;

        const char *lp = (const char *)line;
        char cmd[16];
        next_word(&lp, cmd, sizeof(cmd));

        /* ---- inside a skipped if-block ---- */
        if (skip > 0) {
            if (strcmp(cmd, "if") == 0)       skip++;
            else if (strcmp(cmd, "endif") == 0) skip--;
            continue;
        }

        /* ---- commands ---- */

        if (strcmp(cmd, "print") == 0) {
            char buf[TS_LINE_BUF];
            ts_expand(lp, buf, sizeof(buf));
            kprint(buf);
        }
        else if (strcmp(cmd, "println") == 0) {
            if (*lp) {
                char buf[TS_LINE_BUF];
                ts_expand(lp, buf, sizeof(buf));
                kprint(buf);
            }
            kprint_newline();
        }
        else if (strcmp(cmd, "input") == 0) {
            char vn[TS_NAME_LEN];
            next_word(&lp, vn, sizeof(vn));
            char *name = vn;
            if (name[0] == '$') name++;
            char *val = rec_input();
            ts_set(name, val);
        }
        else if (strcmp(cmd, "set") == 0) {
            char vn[TS_NAME_LEN];
            next_word(&lp, vn, sizeof(vn));
            char *name = vn;
            if (name[0] == '$') name++;
            char buf[TS_LINE_BUF];
            ts_expand(lp, buf, sizeof(buf));
            ts_set(name, buf);
        }
        else if (strcmp(cmd, "add") == 0) {
            char vn[TS_NAME_LEN];
            next_word(&lp, vn, sizeof(vn));
            char *name = vn;
            if (name[0] == '$') name++;
            char ns[16];
            next_word(&lp, ns, sizeof(ns));
            int cur = ts_atoi(ts_get(name));
            int inc = ts_atoi(ns);
            char buf[16]; ts_itoa(cur + inc, buf);
            ts_set(name, buf);
        }
        else if (strcmp(cmd, "sub") == 0) {
            char vn[TS_NAME_LEN];
            next_word(&lp, vn, sizeof(vn));
            char *name = vn;
            if (name[0] == '$') name++;
            char ns[16];
            next_word(&lp, ns, sizeof(ns));
            int cur = ts_atoi(ts_get(name));
            int dec = ts_atoi(ns);
            char buf[16]; ts_itoa(cur - dec, buf);
            ts_set(name, buf);
        }
        else if (strcmp(cmd, "if") == 0) {
            char vn[TS_NAME_LEN];
            next_word(&lp, vn, sizeof(vn));
            char *name = vn;
            if (name[0] == '$') name++;

            char op[4];
            next_word(&lp, op, sizeof(op));

            char exp[TS_LINE_BUF];
            ts_expand(lp, exp, sizeof(exp));

            const char *val = ts_get(name);
            int cond = 0;
            if      (strcmp(op, "==") == 0) cond = (strcmp(val, exp) == 0);
            else if (strcmp(op, "!=") == 0) cond = (strcmp(val, exp) != 0);
            else if (strcmp(op, ">")  == 0) cond = (ts_atoi(val) > ts_atoi(exp));
            else if (strcmp(op, "<")  == 0) cond = (ts_atoi(val) < ts_atoi(exp));
            else if (strcmp(op, ">=") == 0) cond = (ts_atoi(val) >= ts_atoi(exp));
            else if (strcmp(op, "<=") == 0) cond = (ts_atoi(val) <= ts_atoi(exp));

            if (!cond) skip = 1;
        }
        else if (strcmp(cmd, "endif") == 0) {
            /* nothing — skip-mode handles nesting */
        }
        else if (strcmp(cmd, "goto") == 0) {
            char label[TS_NAME_LEN];
            next_word(&lp, label, sizeof(label));
            int target = ts_find_label(label);
            if (target >= 0) {
                pos = target;
                continue;       /* re-enter the while loop at the new offset */
            }
            kprint_newline();
            kprint("[tscript] Error: label not found: ");
            kprint(label);
            kprint_newline();
            return -1;
        }
        else if (strcmp(cmd, "clear") == 0) {
            clear_screen();
        }
        else if (strcmp(cmd, "wait") == 0) {
            /* Poll keyboard until any key is pressed */
            while (!(read_port(0x64) & 0x01))
                __asm__ volatile("hlt");
            (void)read_port(0x60);   /* consume scancode */
        }
        else if (strcmp(cmd, "exit") == 0) {
            break;
        }
        /* unknown commands silently ignored */
    }

    if (steps <= 0) {
        kprint_newline();
        kprint("[tscript] Stopped: execution limit reached");
        kprint_newline();
    }
    return 0;
}

/* ---- validation (syntax check) ---- */

int tscript_validate(const char *src, char *errmsg, int maxlen) {
    int if_depth = 0;
    int line_num = 0;
    int pos = 0;

    while (src[pos]) {
        line_num++;
        while (src[pos] == ' ' || src[pos] == '\t') pos++;

        char line[TS_LINE_BUF]; int li = 0;
        while (src[pos] && src[pos] != '\n' && src[pos] != '\r'
               && li < TS_LINE_BUF - 1)
            line[li++] = src[pos++];
        line[li] = '\0';
        if (src[pos] == '\r') pos++;
        if (src[pos] == '\n') pos++;

        if (line[0] == '\0' || line[0] == '#' || line[0] == ':') continue;

        const char *lp = line;
        char cmd[16];
        next_word(&lp, cmd, sizeof(cmd));

        if (strcmp(cmd, "if") == 0)       if_depth++;
        else if (strcmp(cmd, "endif") == 0) {
            if_depth--;
            if (if_depth < 0) {
                strncpy(errmsg, "Extra endif without matching if", maxlen - 1);
                errmsg[maxlen - 1] = '\0';
                return -1;
            }
        }
        else if (strcmp(cmd, "goto") == 0) {
            char label[TS_NAME_LEN];
            next_word(&lp, label, sizeof(label));
            /* We need to scan labels to check.  Do a quick scan. */
            ts_scan_labels(src);
            if (ts_find_label(label) < 0) {
                strncpy(errmsg, "goto targets missing label: ", maxlen - 1);
                /* append label name */
                int el = (int)strlen(errmsg);
                strncpy(errmsg + el, label, maxlen - 1 - el);
                errmsg[maxlen - 1] = '\0';
                return -1;
            }
        }
    }

    if (if_depth > 0) {
        strncpy(errmsg, "Unclosed if block (missing endif)", maxlen - 1);
        errmsg[maxlen - 1] = '\0';
        return -1;
    }

    return 0;
}

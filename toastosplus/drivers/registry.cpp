#include "registry.hpp"
#include "kio.hpp"
#include "funcs.hpp"
#include "time.hpp"
#include "toast_libc.hpp"
#include "fat16.hpp"

static volatile int registry_saving = 0;

static RegEntry registry[REG_MAX_KEYS];

static int ensure_default_key(const char* key, const char* value) {
    if (reg_get(key) != (const char*)0) {
        return 0;
    }
    if (reg_set(key, value) == 0) {
        return 1;
    }
    return -1;
}

void registry_init(void) {
    int changed = 0;

    for (int i = 0; i < REG_MAX_KEYS; i++) {
        registry[i].used = 0;
    }

    /* Load persisted registry if available, then enforce required defaults. */
    (void)reg_load();

    {
        int r;

        r = ensure_default_key("TOASTOS/KERNEL/TIMEZONE", "UTC");
        if (r < 0) return;
        changed += r;

        r = ensure_default_key("TOASTOS/KERNEL/TIMEFORMAT", "24");
        if (r < 0) return;
        changed += r;

        r = ensure_default_key("TOASTOS/KERNEL/VERSION", "1.1");
        if (r < 0) return;
        changed += r;

        r = ensure_default_key("TOASTOS/KERNEL/SETUPSTATUS", "0");
        if (r < 0) return;
        changed += r;

		r = ensure_default_key("TOASTOS/KERNEL/TBOOTCOL", "WK");
        if (r < 0) return;
        changed += r;

        /* Boot policy defaults to shell mode and can be expanded later. */
        r = ensure_default_key("TOASTOS/KERNEL/BOOTMODE", "SHELL");
        if (r < 0) return;
        changed += r;
    }

    if (changed > 0) {
        reg_save();
    }
}

int reg_set(const char* key, const char* value) {
    /* Check if key already exists */
    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (registry[i].used && strcmp(registry[i].key, key) == 0) {
            strncpy(registry[i].value, value, REG_VALUE_LEN - 1);
            registry[i].value[REG_VALUE_LEN - 1] = '\0';
            return 0;
        }
    }
    /* Find empty slot */
    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (!registry[i].used) {
            strncpy(registry[i].key, key, REG_KEY_LEN - 1);
            registry[i].key[REG_KEY_LEN - 1] = '\0';
            strncpy(registry[i].value, value, REG_VALUE_LEN - 1);
            registry[i].value[REG_VALUE_LEN - 1] = '\0';
            registry[i].used = 1;
            return 0;
        }
    }
    return -1; /* Registry full */
}

const char* reg_get(const char* key) {
    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (registry[i].used && strcmp(registry[i].key, key) == 0) {
            return registry[i].value;
        }
    }
    return (const char*)0;
}

int reg_delete(const char* key) {
    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (registry[i].used && strcmp(registry[i].key, key) == 0) {
            registry[i].used = 0;
            return 0;
        }
    }
    return -1;
}

void reg_list(void) {
    int count = 0;
    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (registry[i].used) {
            kprint("  ");
            kprint(registry[i].key);
            kprint(" = ");
            kprint(registry[i].value);
            kprint_newline();
            count++;
        }
    }
    if (count == 0) {
        kprint("  (empty)");
        kprint_newline();
    }
}

/* List only entries starting with a given prefix */
void reg_list_prefix(const char* prefix) {
    int count = 0;
    int plen = 0;
    while (prefix[plen]) plen++;

    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (registry[i].used && strncmp(registry[i].key, prefix, plen) == 0) {
            kprint("  ");
            kprint(registry[i].key);
            kprint(" = ");
            kprint(registry[i].value);
            kprint_newline();
            count++;
        }
    }
    if (count == 0) {
        kprint("  (no entries)");
        kprint_newline();
    }
}

/* Persist registry to FAT16 file (root) */
int reg_save(void) {
    static char buf[REG_MAX_KEYS * (REG_KEY_LEN + REG_VALUE_LEN + 4)];
    int pos = 0;

    registry_saving = 1;
    /* Refresh UI so the top bar shows the saving indicator immediately */
    update_top_bar();

    for (int i = 0; i < REG_MAX_KEYS; i++) {
        if (registry[i].used) {
            /* key=value\n */
            int k = 0;
            while (registry[i].key[k] && pos < (int)sizeof(buf) - 1) {
                buf[pos++] = registry[i].key[k++];
            }
            if (pos < (int)sizeof(buf) - 1) buf[pos++] = '=';
            int v = 0;
            while (registry[i].value[v] && pos < (int)sizeof(buf) - 1) {
                buf[pos++] = registry[i].value[v++];
            }
            if (pos < (int)sizeof(buf) - 1) buf[pos++] = '\n';
        }
    }
    if (pos >= (int)sizeof(buf)) {
        registry_saving = 0;
        update_top_bar();
        return -1;
    }
    buf[pos] = '\0';

    /* Replace existing file if present */
    if (fat16_file_exists(REG_PERSIST_FILENAME)) {
        fat16_delete_file(REG_PERSIST_FILENAME);
    }

    if (fat16_create_file(REG_PERSIST_FILENAME, buf) == 0) {
        registry_saving = 0;
        update_top_bar();
        return 0;
    }
    registry_saving = 0;
    update_top_bar();
    return -1;
}

/* Load registry from disk file */
int reg_load(void) {
    static char buf[REG_MAX_KEYS * (REG_KEY_LEN + REG_VALUE_LEN + 4)];
    if (!fat16_file_exists(REG_PERSIST_FILENAME)) return -1;
    int r = fat16_read_file(REG_PERSIST_FILENAME, buf, sizeof(buf));
    if (r < 0) return -1;

    /* Parse lines of form key=value */
    int i = 0;
    while (i < r) {
        /* read key */
        char key[REG_KEY_LEN];
        char val[REG_VALUE_LEN];
        int kp = 0;
        while (i < r && buf[i] != '=' && buf[i] != '\n' && kp < REG_KEY_LEN - 1) {
            key[kp++] = buf[i++];
        }
        key[kp] = '\0';

        if (i < r && buf[i] == '=') i++; /* skip = */

        int vp = 0;
        while (i < r && buf[i] != '\n' && vp < REG_VALUE_LEN - 1) {
            val[vp++] = buf[i++];
        }
        val[vp] = '\0';

        /* skip newline if present */
        if (i < r && buf[i] == '\n') i++;

        if (key[0] != '\0') {
            reg_set(key, val);
        }
    }

    return 0;
}

int reg_is_saving(void) {
    return registry_saving;
}


/* ========== toast::reg namespace implementations ========== */
namespace toast {
namespace reg {

void init() { registry_init(); }
int set(const char* key, const char* value) { return reg_set(key, value); }
const char* get(const char* key) { return reg_get(key); }
int remove(const char* key) { return reg_delete(key); }
void list() { reg_list(); }
void list_prefix(const char* prefix) { reg_list_prefix(prefix); }
int save() { return reg_save(); }
int load() { return reg_load(); }
int is_saving() { return reg_is_saving(); }

} // namespace reg
} // namespace toast

#ifndef REGISTRY_H
#define REGISTRY_H

#define REG_MAX_KEYS     64
#define REG_KEY_LEN      48
#define REG_VALUE_LEN    80

/* Registry entry */
typedef struct {
    char key[REG_KEY_LEN];     // e.g. "TOASTOS/KERNEL/TIMEZONE"
    char value[REG_VALUE_LEN]; // e.g. "EST"
    int  used;
} RegEntry;

/* Initialize the registry with defaults */
void registry_init(void);

/* Set a key/value. Creates if not found. */
int reg_set(const char* key, const char* value);

/* Get a value by key. Returns NULL if not found. */
const char* reg_get(const char* key);

/* Delete a key. Returns 0 on success, -1 if not found. */
int reg_delete(const char* key);

/* Print all registry entries */
void reg_list(void);

/* Print entries under a prefix, e.g. "TOASTOS/KERNEL" */
void reg_list_prefix(const char* prefix);

/* Persist registry to disk */
int reg_save(void);
/* Load registry from disk */
int reg_load(void);

#define REG_PERSIST_FILENAME "TOASTREG.TXT"

/* Query whether a save is currently in progress */
int reg_is_saving(void);

#endif

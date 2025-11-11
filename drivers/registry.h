#ifndef REGISTRY_H
#define REGISTRY_H

#include "stdint.h"

#define MAX_REGISTRY_KEYS 32
#define MAX_KEY_NAME 32
#define MAX_KEY_VALUE 128

typedef struct {
    char key[MAX_KEY_NAME];
    char value[MAX_KEY_VALUE];
    int active;
} registry_entry_t;

// Registry functions
void registry_init(void);
void registry_load(void);
void registry_save(void);
int registry_set(const char* key, const char* value);
const char* registry_get(const char* key);
int registry_delete(const char* key);
void registry_list(void);

#endif // REGISTRY_H

#include "registry.h"
#include "funcs.h"
#include "kio.h"
#include "string.h"
#include "disk.h"

// Registry storage
static registry_entry_t registry[MAX_REGISTRY_KEYS];
static int registry_count = 0;

// Initialize registry
void registry_init(void) {
    serial_write_string("[REGISTRY] Initializing registry system...\n");
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        registry[i].active = 0;
        registry[i].key[0] = '\0';
        registry[i].value[0] = '\0';
    }
    registry_count = 0;
    
    serial_write_string("[REGISTRY] Setting default values...\n");
    // Set default values
    registry_set("os.name", "toastOS");
    registry_set("os.version", "1.1");
    registry_set("timezone.offset", "-5");
    registry_set("security.lock", "0");
    registry_set("toast.secure.ab", "enab");
    serial_write_string("[REGISTRY] Initialization complete\n");
}

// Save registry to disk
void registry_save(void) {
    serial_write_string("[REGISTRY] Saving to disk...\n");
    uint8_t buffer[SECTOR_SIZE];
    memset(buffer, 0, SECTOR_SIZE);
    
    // Pack registry entries into buffer
    // Format: [count (4 bytes)] [entries...]
    // Each entry: [key (32 bytes)] [value (128 bytes)] [active (1 byte)]
    
    uint32_t* count_ptr = (uint32_t*)buffer;
    *count_ptr = MAX_REGISTRY_KEYS;
    
    uint8_t* ptr = buffer + 4;
    int bytes_written = 4;
    
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        // Check if we have space (need 161 bytes per entry)
        if (bytes_written + 161 > SECTOR_SIZE) {
            break;
        }
        
        // Copy key (32 bytes)
        memcpy(ptr, registry[i].key, MAX_KEY_NAME);
        ptr += MAX_KEY_NAME;
        
        // Copy value (128 bytes)
        memcpy(ptr, registry[i].value, MAX_KEY_VALUE);
        ptr += MAX_KEY_VALUE;
        
        // Copy active flag (1 byte)
        *ptr = registry[i].active ? 1 : 0;
        ptr++;
        
        bytes_written += 161;
    }
    
    // Write to disk
    if (disk_write_sector(REGISTRY_SECTOR, buffer) == 0) {
        serial_write_string("[REGISTRY] Successfully saved to sector ");
        kprint_int(REGISTRY_SECTOR);
        serial_write_string("\n");
        kprint("Registry saved to disk.");
        kprint_newline();
    } else {
        serial_write_string("[REGISTRY] ERROR: Failed to write to disk\n");
        kprint("Warning: Could not save registry to disk (no persistent storage).");
        kprint_newline();
    }
}

// Load registry from disk
void registry_load(void) {
    uint8_t buffer[SECTOR_SIZE];
    
    // Try to read from disk
    if (disk_read_sector(REGISTRY_SECTOR, buffer) != 0) {
        kprint("[REGISTRY] No disk available, using defaults");
        kprint_newline();
        return;
    }
    
    // Unpack registry entries from buffer
    uint32_t* count_ptr = (uint32_t*)buffer;
    uint32_t count = *count_ptr;
    
    // Check if disk is empty (all zeros) or corrupted
    if (count == 0 || count > MAX_REGISTRY_KEYS) {
        kprint("[REGISTRY] Empty or corrupted disk, using defaults");
        kprint_newline();
        return;
    }
    
    uint8_t* ptr = buffer + 4;
    int bytes_read = 4;
    int loaded_count = 0;
    
    for (int i = 0; i < MAX_REGISTRY_KEYS && bytes_read + 161 <= SECTOR_SIZE; i++) {
        // Copy key (32 bytes)
        memcpy(registry[i].key, ptr, MAX_KEY_NAME);
        ptr += MAX_KEY_NAME;
        
        // Copy value (128 bytes)
        memcpy(registry[i].value, ptr, MAX_KEY_VALUE);
        ptr += MAX_KEY_VALUE;
        
        // Copy active flag (1 byte)
        registry[i].active = (*ptr == 1) ? 1 : 0;
        ptr++;
        
        if (registry[i].active) {
            loaded_count++;
        }
        
        bytes_read += 161;
    }
    
    registry_count = loaded_count;
    kprint("[REGISTRY] Loaded ");
    kprint_int(loaded_count);
    kprint(" entries from disk");
    kprint_newline();
}

// Set a registry value
int registry_set(const char* key, const char* value) {
    // Check if key already exists
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        if (registry[i].active && strcmp(registry[i].key, key) == 0) {
            // Update existing key
            strncpy(registry[i].value, value, MAX_KEY_VALUE - 1);
            registry[i].value[MAX_KEY_VALUE - 1] = '\0';
            return 0;
        }
    }
    
    // Add new key
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        if (!registry[i].active) {
            strncpy(registry[i].key, key, MAX_KEY_NAME - 1);
            registry[i].key[MAX_KEY_NAME - 1] = '\0';
            strncpy(registry[i].value, value, MAX_KEY_VALUE - 1);
            registry[i].value[MAX_KEY_VALUE - 1] = '\0';
            registry[i].active = 1;
            registry_count++;
            return 0;
        }
    }
    
    return -1; // Registry full
}

// Get a registry value
const char* registry_get(const char* key) {
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        if (registry[i].active && strcmp(registry[i].key, key) == 0) {
            return registry[i].value;
        }
    }
    return NULL; // Not found
}

// Delete a registry key
int registry_delete(const char* key) {
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        if (registry[i].active && strcmp(registry[i].key, key) == 0) {
            registry[i].active = 0;
            registry_count--;
            return 0;
        }
    }
    return -1; // Not found
}

// List all registry keys
void registry_list(void) {
    kprint("=== System Registry ===");
    kprint_newline();
    
    if (registry_count == 0) {
        kprint("No registry entries.");
        kprint_newline();
        return;
    }
    
    for (int i = 0; i < MAX_REGISTRY_KEYS; i++) {
        if (registry[i].active) {
            kprint("  ");
            kprint(registry[i].key);
            kprint(" = ");
            kprint(registry[i].value);
            kprint_newline();
        }
    }
    
    kprint_newline();
    kprint("Total entries: ");
    kprint_int(registry_count);
    kprint_newline();
}

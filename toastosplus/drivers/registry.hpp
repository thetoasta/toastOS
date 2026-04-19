/*
 * toastOS++ Registry
 * Namespace: toast::reg
 */

#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#define REG_MAX_KEYS     64
#define REG_KEY_LEN      48
#define REG_VALUE_LEN    80
#define REG_PERSIST_FILENAME "TOASTREG.TXT"

struct RegEntry {
    char key[REG_KEY_LEN];
    char value[REG_VALUE_LEN];
    int  used;
};

namespace toast {
namespace reg {

void init();
int set(const char* key, const char* value);
const char* get(const char* key);
int remove(const char* key);
void list();
void list_prefix(const char* prefix);
int save();
int load();
int is_saving();

} // namespace reg
} // namespace toast

/* Legacy C functions */
extern "C" {
    void registry_init();
    int reg_set(const char* key, const char* value);
    const char* reg_get(const char* key);
    int reg_delete(const char* key);
    void reg_list();
    void reg_list_prefix(const char* prefix);
    int reg_save();
    int reg_load();
    int reg_is_saving();
}

#endif /* REGISTRY_HPP */

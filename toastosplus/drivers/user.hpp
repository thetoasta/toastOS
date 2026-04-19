/*
 * toastOS++ User Management
 * Namespace: toast::user
 */

#ifndef USER_HPP
#define USER_HPP

#include "stdint.hpp"

#define MAX_USERS        16
#define MAX_USERNAME_LEN 32

#define USER_ROLE_GUEST  0
#define USER_ROLE_USER   1
#define USER_ROLE_ADMIN  2

struct user_account_t {
    char     username[MAX_USERNAME_LEN];
    uint8_t  password_hash[32];
    uint32_t salt;
    uint8_t  role;
    uint8_t  active;
    uint32_t last_login;
};

namespace toast {
namespace user {

void init();
int create(const char* username, const char* password, uint8_t role);
int login(const char* username, const char* password);
void logout();
int remove(const char* username);
int change_password(const char* username, const char* old_pw, const char* new_pw);
int set_role(const char* username, uint8_t new_role);

/* Query functions */
const char* current();
uint8_t role();
int is_logged_in();
int exists(const char* username);
int count();
int list(char usernames[][MAX_USERNAME_LEN], int max_count);

/* Session tracking */
uint32_t session_start();
uint32_t session_duration();

/* Password helpers */
void hash_password(const char* password, uint32_t salt, uint8_t out[32]);

} // namespace user
} // namespace toast

/* Legacy C functions */
extern "C" {
    void hashpword(const char* password, uint32_t salt, uint8_t out[32]);
    void set_password(const char* password);
    int check_password(const char* password);
    void user_init();
    int user_create(const char* username, const char* password, uint8_t role);
    int user_login(const char* username, const char* password);
    void user_logout();
    int user_delete(const char* username);
    int user_change_password(const char* username, const char* old_pw, const char* new_pw);
    int user_set_role(const char* username, uint8_t new_role);
    const char* user_get_current();
    uint8_t user_get_role();
    int user_is_logged_in();
    int user_exists(const char* username);
    int user_count();
    int user_list(char usernames[][MAX_USERNAME_LEN], int max_count);
    uint32_t user_get_session_start();
    uint32_t user_get_session_duration();
}

#endif /* USER_HPP */

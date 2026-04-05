//
// toastOS user management driver
//

#ifndef TOASTOS_USER_H
#define TOASTOS_USER_H

#include "stdint.h"

/* Maximum limits */
#define MAX_USERS        16
#define MAX_USERNAME_LEN 32

/* User roles */
#define USER_ROLE_GUEST  0
#define USER_ROLE_USER   1
#define USER_ROLE_ADMIN  2

/* User account structure */
typedef struct {
    char     username[MAX_USERNAME_LEN];
    uint8_t  password_hash[32];
    uint32_t salt;
    uint8_t  role;
    uint8_t  active;          /* 1 if account exists */
    uint32_t last_login;      /* timestamp */
} user_account_t;

/*
 * hashpword — SHA-256 hash a password with a 32-bit salt.
 * Writes 32 bytes (256 bits) into `out`.
 */
void hashpword(const char* password, uint32_t salt, uint8_t out[32]);
void set_password(const char* password);
int check_password(const char* password);

/* Multi-user system */
void        user_init(void);
int         user_create(const char* username, const char* password, uint8_t role);
int         user_login(const char* username, const char* password);
void        user_logout(void);
int         user_delete(const char* username);
int         user_change_password(const char* username, const char* old_pw, const char* new_pw);
int         user_set_role(const char* username, uint8_t new_role);

/* Query functions */
const char* user_get_current(void);
uint8_t     user_get_role(void);
int         user_is_logged_in(void);
int         user_exists(const char* username);
int         user_count(void);
int         user_list(char usernames[][MAX_USERNAME_LEN], int max_count);

/* Session tracking */
uint32_t    user_get_session_start(void);
uint32_t    user_get_session_duration(void);

#endif //TOASTOS_USER_H

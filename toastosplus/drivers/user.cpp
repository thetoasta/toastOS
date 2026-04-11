/*

toastOS user management driver
built for toastOS 1.2+ ;)

*/

#include "user.hpp"
#include "kio.hpp"
#include "security.hpp"
#include "registry.hpp"
#include "string.hpp"
#include "stdint.hpp"
#include "time.hpp"

/* SHA-256 implementation — no external deps */
uint8_t digest[32];

/* Current session state */
static user_account_t users[MAX_USERS];
static int            current_user_idx = -1;  /* -1 = not logged in */
static uint32_t       session_start = 0;

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t ep0(uint32_t x) { return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
static uint32_t ep1(uint32_t x) { return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
static uint32_t sig0(uint32_t x) { return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
static uint32_t sig1(uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16)
              | ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
    for (i = 16; i < 64; i++)
        w[i] = sig1(w[i-2]) + w[i-7] + sig0(w[i-15]) + w[i-16];

    a=state[0]; b=state[1]; c=state[2]; d=state[3];
    e=state[4]; f=state[5]; g=state[6]; h=state[7];

    for (i = 0; i < 64; i++) {
        t1 = h + ep1(e) + ch(e,f,g) + sha256_k[i] + w[i];
        t2 = ep0(a) + maj(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

/*
 * hashpword — SHA-256 a password with a salt prefix.
 * Output: 32-byte digest written to `out` (caller provides uint8_t[32]).
 */
void hashpword(const char* password, uint32_t salt, uint8_t out[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t buf[64];
    uint32_t i, len, total;

    /* Build message: 4 salt bytes + password bytes */
    buf[0] = (uint8_t)(salt >> 24);
    buf[1] = (uint8_t)(salt >> 16);
    buf[2] = (uint8_t)(salt >> 8);
    buf[3] = (uint8_t)(salt);
    len = 4;

    for (i = 0; password[i]; i++) {
        buf[len++] = (uint8_t)password[i];
        if (len == 64) {
            sha256_transform(state, buf);
            len = 0;
        }
    }
    total = 4 + i; /* total message length in bytes */

    /* Padding: append 1-bit, zeros, then 64-bit big-endian length */
    buf[len++] = 0x80;
    if (len > 56) {
        while (len < 64) buf[len++] = 0;
        sha256_transform(state, buf);
        len = 0;
    }
    while (len < 56) buf[len++] = 0;

    /* Append bit-length as 64-bit big-endian (message < 2^32 bytes) */
    {
        uint64_t bits = (uint64_t)total * 8;
        buf[56] = (uint8_t)(bits >> 56);
        buf[57] = (uint8_t)(bits >> 48);
        buf[58] = (uint8_t)(bits >> 40);
        buf[59] = (uint8_t)(bits >> 32);
        buf[60] = (uint8_t)(bits >> 24);
        buf[61] = (uint8_t)(bits >> 16);
        buf[62] = (uint8_t)(bits >> 8);
        buf[63] = (uint8_t)(bits);
    }
    sha256_transform(state, buf);

    /* Write digest */
    for (i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(state[i] >> 24);
        out[i*4+1] = (uint8_t)(state[i] >> 16);
        out[i*4+2] = (uint8_t)(state[i] >> 8);
        out[i*4+3] = (uint8_t)(state[i]);
    }
}

/* Convert a uint32_t to an 8-char hex string */
static void u32_to_hex(uint32_t v, char out[9]) {
    const char hex[] = "0123456789abcdef";
    int i;
    for (i = 7; i >= 0; i--) {
        out[i] = hex[v & 0xf];
        v >>= 4;
    }
    out[8] = '\0';
}

/* Parse an 8-char hex string back to uint32_t */
static uint32_t hex_to_u32(const char* s) {
    uint32_t v = 0;
    int i;
    for (i = 0; i < 8 && s[i]; i++) {
        v <<= 4;
        if (s[i] >= '0' && s[i] <= '9') v |= s[i] - '0';
        else if (s[i] >= 'a' && s[i] <= 'f') v |= s[i] - 'a' + 10;
        else if (s[i] >= 'A' && s[i] <= 'F') v |= s[i] - 'A' + 10;
    }
    return v;
}

/* Convert 32-byte digest to 64-char hex string */
static void digest_to_hex(const uint8_t d[32], char out[65]) {
    const char hex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; i++) {
        out[i*2]   = hex[(d[i] >> 4) & 0xf];
        out[i*2+1] = hex[d[i] & 0xf];
    }
    out[64] = '\0';
}

void set_password(const char* password) {
    uint32_t salt = 0;
    const char* salt_str = reg_get("TOASTOS/SECURITY/SALT");
    char salt_buf[9];
    char hash_str[65];

    if (salt_str != 0) {
        salt = hex_to_u32(salt_str);
    }
    if (salt == 0) {
        /* Ensure salt is never 0 - use uptime or fallback to a fixed seed */
        salt = get_uptime_seconds();
        if (salt == 0) {
            salt = 0x54415354;  /* 'TAST' - fallback if uptime is 0 */
        }
        u32_to_hex(salt, salt_buf);
        reg_set("TOASTOS/SECURITY/SALT", salt_buf);
    }
    hashpword(password, salt, digest);
    digest_to_hex(digest, hash_str);
    reg_set("TOASTOS/SECURITY/PASSWORD", hash_str);
    reg_save();
}

/*
 * check_password — returns 1 if `password` matches the stored hash, 0 otherwise.
 */
int check_password(const char* password) {
    const char* stored = reg_get("TOASTOS/SECURITY/PASSWORD");
    const char* salt_str = reg_get("TOASTOS/SECURITY/SALT");
    uint32_t salt;
    uint8_t d[32];
    char hash_str[65];
    int i, diff;

    /* Also check legacy key location for backwards compatibility */
    if (!salt_str) {
        salt_str = reg_get("user_salt");
    }

    if (!stored || !salt_str)
        return 0;

    salt = hex_to_u32(salt_str);
    hashpword(password, salt, d);
    digest_to_hex(d, hash_str);

    /* Constant-time comparison to avoid timing attacks */
    diff = 0;
    for (i = 0; i < 64; i++) {
        diff |= hash_str[i] ^ stored[i];
    }
    return diff == 0;
}

/* ------------------------------------------------------------------ */
/* Multi-user system implementation                                    */
/* ------------------------------------------------------------------ */

/* Convert username to registry key */
static void make_user_key(const char* username, const char* field, char* out, int size) {
    int pos = 0;
    const char* prefix = "TOASTOS/USERS/";
    for (int i = 0; prefix[i] && pos < size - 1; i++)
        out[pos++] = prefix[i];
    for (int i = 0; username[i] && pos < size - 1; i++)
        out[pos++] = username[i];
    out[pos++] = '/';
    for (int i = 0; field[i] && pos < size - 1; i++)
        out[pos++] = field[i];
    out[pos] = '\0';
}

/* Initialize user subsystem */
void user_init(void) {
    for (int i = 0; i < MAX_USERS; i++) {
        users[i].username[0] = '\0';
        users[i].active = 0;
        users[i].role = USER_ROLE_GUEST;
    }
    current_user_idx = -1;
    session_start = 0;
    
    /* Load users from registry */
    for (int i = 0; i < MAX_USERS; i++) {
        char key[64];
        char idx_str[4];
        idx_str[0] = '0' + (i / 10);
        idx_str[1] = '0' + (i % 10);
        idx_str[2] = '\0';
        
        /* Build key: TOASTOS/USERS/INDEX/username */
        int pos = 0;
        const char* prefix = "TOASTOS/USERS/";
        for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
        for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
        const char* suffix = "/username";
        for (int j = 0; suffix[j]; j++) key[pos++] = suffix[j];
        key[pos] = '\0';
        
        const char* uname = reg_get(key);
        if (uname && uname[0]) {
            strncpy(users[i].username, uname, MAX_USERNAME_LEN - 1);
            users[i].username[MAX_USERNAME_LEN - 1] = '\0';
            users[i].active = 1;
            
            /* Load role */
            pos = 0;
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* role_suffix = "/role";
            for (int j = 0; role_suffix[j]; j++) key[pos++] = role_suffix[j];
            key[pos] = '\0';
            
            const char* role_str = reg_get(key);
            if (role_str) {
                users[i].role = (uint8_t)(role_str[0] - '0');
            }
            
            /* Load salt */
            pos = 0;
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* salt_suffix = "/salt";
            for (int j = 0; salt_suffix[j]; j++) key[pos++] = salt_suffix[j];
            key[pos] = '\0';
            
            const char* salt_str = reg_get(key);
            if (salt_str) {
                users[i].salt = hex_to_u32(salt_str);
            }
            
            /* Load hash */
            pos = 0;
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* hash_suffix = "/hash";
            for (int j = 0; hash_suffix[j]; j++) key[pos++] = hash_suffix[j];
            key[pos] = '\0';
            
            const char* hash_str = reg_get(key);
            if (hash_str) {
                /* Parse hex hash into bytes */
                for (int h = 0; h < 32 && hash_str[h*2]; h++) {
                    uint8_t hi = 0, lo = 0;
                    char c = hash_str[h*2];
                    if (c >= '0' && c <= '9') hi = c - '0';
                    else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
                    
                    c = hash_str[h*2+1];
                    if (c >= '0' && c <= '9') lo = c - '0';
                    else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;
                    
                    users[i].password_hash[h] = (hi << 4) | lo;
                }
            }
        }
    }
}

/* Create a new user account */
int user_create(const char* username, const char* password, uint8_t role) {
    if (!username || !username[0] || !password)
        return -1;
    
    /* Check if username already exists */
    if (user_exists(username))
        return -2;
    
    /* Find empty slot */
    int slot = -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (!users[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -3;  /* No room */
    
    /* Setup the user */
    strncpy(users[slot].username, username, MAX_USERNAME_LEN - 1);
    users[slot].username[MAX_USERNAME_LEN - 1] = '\0';
    users[slot].role = role;
    users[slot].active = 1;
    users[slot].salt = get_uptime_seconds();
    users[slot].last_login = 0;
    
    /* Hash password */
    hashpword(password, users[slot].salt, users[slot].password_hash);
    
    /* Save to registry */
    char key[64], value[65];
    char idx_str[4];
    idx_str[0] = '0' + (slot / 10);
    idx_str[1] = '0' + (slot % 10);
    idx_str[2] = '\0';
    
    /* Save username */
    int pos = 0;
    const char* prefix = "TOASTOS/USERS/";
    for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
    for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
    const char* suffix = "/username";
    for (int j = 0; suffix[j]; j++) key[pos++] = suffix[j];
    key[pos] = '\0';
    reg_set(key, username);
    
    /* Save role */
    pos = 0;
    for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
    for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
    const char* role_suffix = "/role";
    for (int j = 0; role_suffix[j]; j++) key[pos++] = role_suffix[j];
    key[pos] = '\0';
    value[0] = '0' + role;
    value[1] = '\0';
    reg_set(key, value);
    
    /* Save salt */
    pos = 0;
    for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
    for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
    const char* salt_suffix = "/salt";
    for (int j = 0; salt_suffix[j]; j++) key[pos++] = salt_suffix[j];
    key[pos] = '\0';
    u32_to_hex(users[slot].salt, value);
    reg_set(key, value);
    
    /* Save hash */
    pos = 0;
    for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
    for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
    const char* hash_suffix = "/hash";
    for (int j = 0; hash_suffix[j]; j++) key[pos++] = hash_suffix[j];
    key[pos] = '\0';
    digest_to_hex(users[slot].password_hash, value);
    reg_set(key, value);
    
    reg_save();
    return 0;
}

/* Login as a user */
int user_login(const char* username, const char* password) {
    if (!username || !password)
        return -1;
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            /* Verify password */
            uint8_t hash[32];
            hashpword(password, users[i].salt, hash);
            
            int match = 1;
            for (int j = 0; j < 32; j++) {
                if (hash[j] != users[i].password_hash[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                current_user_idx = i;
                session_start = get_uptime_seconds();
                users[i].last_login = session_start;
                return 0;
            }
            return -2;  /* Wrong password */
        }
    }
    return -3;  /* User not found */
}

/* Logout current user */
void user_logout(void) {
    current_user_idx = -1;
    session_start = 0;
}

/* Delete a user account */
int user_delete(const char* username) {
    if (!username)
        return -1;
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            /* Clear the slot */
            users[i].username[0] = '\0';
            users[i].active = 0;
            
            /* Remove from registry */
            char key[64];
            char idx_str[4];
            idx_str[0] = '0' + (i / 10);
            idx_str[1] = '0' + (i % 10);
            idx_str[2] = '\0';
            
            int pos = 0;
            const char* prefix = "TOASTOS/USERS/";
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* suffix = "/username";
            for (int j = 0; suffix[j]; j++) key[pos++] = suffix[j];
            key[pos] = '\0';
            reg_set(key, "");
            
            /* If this was the current user, log out */
            if (current_user_idx == i) {
                user_logout();
            }
            
            reg_save();
            return 0;
        }
    }
    return -2;  /* Not found */
}

/* Change user password */
int user_change_password(const char* username, const char* old_pw, const char* new_pw) {
    if (!username || !old_pw || !new_pw)
        return -1;
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            /* Verify old password */
            uint8_t hash[32];
            hashpword(old_pw, users[i].salt, hash);
            
            int match = 1;
            for (int j = 0; j < 32; j++) {
                if (hash[j] != users[i].password_hash[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (!match) return -2;  /* Wrong old password */
            
            /* Generate new salt and hash */
            users[i].salt = get_uptime_seconds();
            hashpword(new_pw, users[i].salt, users[i].password_hash);
            
            /* Save to registry */
            char key[64], value[65];
            char idx_str[4];
            idx_str[0] = '0' + (i / 10);
            idx_str[1] = '0' + (i % 10);
            idx_str[2] = '\0';
            
            int pos = 0;
            const char* prefix = "TOASTOS/USERS/";
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* salt_suffix = "/salt";
            for (int j = 0; salt_suffix[j]; j++) key[pos++] = salt_suffix[j];
            key[pos] = '\0';
            u32_to_hex(users[i].salt, value);
            reg_set(key, value);
            
            pos = 0;
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* hash_suffix = "/hash";
            for (int j = 0; hash_suffix[j]; j++) key[pos++] = hash_suffix[j];
            key[pos] = '\0';
            digest_to_hex(users[i].password_hash, value);
            reg_set(key, value);
            
            reg_save();
            return 0;
        }
    }
    return -3;  /* User not found */
}

/* Set user role (admin only) */
int user_set_role(const char* username, uint8_t new_role) {
    if (!username)
        return -1;
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            users[i].role = new_role;
            
            /* Save to registry */
            char key[64], value[2];
            char idx_str[4];
            idx_str[0] = '0' + (i / 10);
            idx_str[1] = '0' + (i % 10);
            idx_str[2] = '\0';
            
            int pos = 0;
            const char* prefix = "TOASTOS/USERS/";
            for (int j = 0; prefix[j]; j++) key[pos++] = prefix[j];
            for (int j = 0; idx_str[j]; j++) key[pos++] = idx_str[j];
            const char* role_suffix = "/role";
            for (int j = 0; role_suffix[j]; j++) key[pos++] = role_suffix[j];
            key[pos] = '\0';
            value[0] = '0' + new_role;
            value[1] = '\0';
            reg_set(key, value);
            
            reg_save();
            return 0;
        }
    }
    return -2;  /* Not found */
}

/* Get current logged-in username */
const char* user_get_current(void) {
    if (current_user_idx >= 0 && current_user_idx < MAX_USERS)
        return users[current_user_idx].username;
    return 0;
}

/* Get current user's role */
uint8_t user_get_role(void) {
    if (current_user_idx >= 0 && current_user_idx < MAX_USERS)
        return users[current_user_idx].role;
    return USER_ROLE_GUEST;
}

/* Check if any user is logged in */
int user_is_logged_in(void) {
    return current_user_idx >= 0;
}

/* Check if a username exists */
int user_exists(const char* username) {
    if (!username) return 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0)
            return 1;
    }
    return 0;
}

/* Count active users */
int user_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active) count++;
    }
    return count;
}

/* List all usernames */
int user_list(char usernames[][MAX_USERNAME_LEN], int max_count) {
    int count = 0;
    for (int i = 0; i < MAX_USERS && count < max_count; i++) {
        if (users[i].active) {
            strncpy(usernames[count], users[i].username, MAX_USERNAME_LEN - 1);
            usernames[count][MAX_USERNAME_LEN - 1] = '\0';
            count++;
        }
    }
    return count;
}

/* Get session start time */
uint32_t user_get_session_start(void) {
    return session_start;
}

/* Get session duration in seconds */
uint32_t user_get_session_duration(void) {
    if (session_start == 0) return 0;
    return get_uptime_seconds() - session_start;
}

/* ========== toast::user namespace implementations ========== */
namespace toast {
namespace user {

void init() { user_init(); }
int create(const char* username, const char* password, uint8_t role) { return user_create(username, password, role); }
int login(const char* username, const char* password) { return user_login(username, password); }
void logout() { user_logout(); }
int remove(const char* username) { return user_delete(username); }
int change_password(const char* username, const char* old_pw, const char* new_pw) { return user_change_password(username, old_pw, new_pw); }
int set_role(const char* username, uint8_t new_role) { return user_set_role(username, new_role); }
const char* current() { return user_get_current(); }
uint8_t role() { return user_get_role(); }
int is_logged_in() { return user_is_logged_in(); }
int exists(const char* username) { return user_exists(username); }
int count() { return user_count(); }
int list(char usernames[][MAX_USERNAME_LEN], int max_count) { return user_list(usernames, max_count); }
uint32_t session_start() { return user_get_session_start(); }
uint32_t session_duration() { return user_get_session_duration(); }
void hash_password(const char* password, uint32_t salt, uint8_t out[32]) { hashpword(password, salt, out); }

} // namespace user
} // namespace toast

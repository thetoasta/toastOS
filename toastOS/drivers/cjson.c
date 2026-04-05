/*
 * cJSON - Ultralight JSON parser for toastOS
 * Based on cJSON by Dave Gamble (MIT License)
 * Adapted for toastOS kernel environment (integer-only, no doubles)
 */

#include "cjson.h"
#include "toast_libc.h"
#include "stdio.h"

/* ---- Internal helpers ---- */

static cJSON *cJSON_New_Item(void) {
    cJSON *node = (cJSON *)malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

static char *cJSON_strdup(const char *str) {
    if (!str) return NULL;
    return strdup(str);
}

/* ---- Delete ---- */

void cJSON_Delete(cJSON *item) {
    cJSON *next;
    while (item) {
        next = item->next;
        if (item->child) cJSON_Delete(item->child);
        if (!(item->type & cJSON_IsReference) && item->valuestring)
            free(item->valuestring);
        if (!(item->type & cJSON_StringIsConst) && item->string)
            free(item->string);
        free(item);
        item = next;
    }
}

/* ---- Parser ---- */

static const char *skip_whitespace(const char *s) {
    while (s && *s && (*s <= ' ')) s++;
    return s;
}

static const char *parse_string(cJSON *item, const char *str) {
    if (*str != '\"') return NULL;
    str++;
    const char *start = str;
    while (*str && *str != '\"') {
        if (*str == '\\') str++; /* skip escaped char */
        str++;
    }
    if (*str != '\"') return NULL;

    size_t len = (size_t)(str - start);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    /* Copy with basic escape handling */
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (start[i] == '\\' && i + 1 < len) {
            i++;
            switch (start[i]) {
                case '\"': out[j++] = '\"'; break;
                case '\\': out[j++] = '\\'; break;
                case '/':  out[j++] = '/';  break;
                case 'n':  out[j++] = '\n'; break;
                case 'r':  out[j++] = '\r'; break;
                case 't':  out[j++] = '\t'; break;
                case 'b':  out[j++] = '\b'; break;
                case 'f':  out[j++] = '\f'; break;
                default:   out[j++] = start[i]; break;
            }
        } else {
            out[j++] = start[i];
        }
    }
    out[j] = '\0';

    item->valuestring = out;
    item->type = cJSON_String;
    return str + 1; /* skip closing quote */
}

static const char *parse_number(cJSON *item, const char *str) {
    char *end;
    long val = strtol(str, &end, 10);
    if (end == str) return NULL;
    item->valueint = (int)val;
    item->type = cJSON_Number;
    return end;
}

/* Forward declarations */
static const char *parse_value(cJSON *item, const char *value);
static const char *parse_array(cJSON *item, const char *value);
static const char *parse_object(cJSON *item, const char *value);

static const char *parse_value(cJSON *item, const char *value) {
    if (!value) return NULL;
    value = skip_whitespace(value);

    if (!strncmp(value, "null", 4))  { item->type = cJSON_NULL;  return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4))  { item->type = cJSON_True;  return value + 4; }
    if (*value == '\"') return parse_string(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[')  return parse_array(item, value);
    if (*value == '{')  return parse_object(item, value);
    return NULL;
}

static const char *parse_array(cJSON *item, const char *value) {
    if (*value != '[') return NULL;
    item->type = cJSON_Array;
    value = skip_whitespace(value + 1);
    if (*value == ']') return value + 1;

    cJSON *child = cJSON_New_Item();
    if (!child) return NULL;
    item->child = child;
    value = skip_whitespace(parse_value(child, value));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
    }

    if (*value == ']') return value + 1;
    return NULL;
}

static const char *parse_object(cJSON *item, const char *value) {
    if (*value != '{') return NULL;
    item->type = cJSON_Object;
    value = skip_whitespace(value + 1);
    if (*value == '}') return value + 1;

    cJSON *child = cJSON_New_Item();
    if (!child) return NULL;
    item->child = child;

    /* Parse key */
    value = skip_whitespace(value);
    cJSON key_holder;
    memset(&key_holder, 0, sizeof(key_holder));
    value = parse_string(&key_holder, value);
    if (!value) return NULL;
    child->string = key_holder.valuestring;

    value = skip_whitespace(value);
    if (*value != ':') return NULL;
    value = skip_whitespace(value + 1);
    value = parse_value(child, value);
    if (!value) return NULL;
    value = skip_whitespace(value);

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;

        value = skip_whitespace(value + 1);
        cJSON kh2;
        memset(&kh2, 0, sizeof(kh2));
        value = parse_string(&kh2, value);
        if (!value) return NULL;
        child->string = kh2.valuestring;

        value = skip_whitespace(value);
        if (*value != ':') return NULL;
        value = skip_whitespace(value + 1);
        value = parse_value(child, value);
        if (!value) return NULL;
        value = skip_whitespace(value);
    }

    if (*value == '}') return value + 1;
    return NULL;
}

cJSON *cJSON_Parse(const char *value) {
    cJSON *item = cJSON_New_Item();
    if (!item) return NULL;
    const char *end = parse_value(item, skip_whitespace(value));
    if (!end) { cJSON_Delete(item); return NULL; }
    return item;
}

/* ---- Printer ---- */

static void print_to_buf(char **buf, size_t *cap, size_t *len, const char *str) {
    size_t slen = strlen(str);
    while (*len + slen + 1 > *cap) {
        *cap *= 2;
        *buf = (char *)realloc(*buf, *cap);
        if (!*buf) return;
    }
    memcpy(*buf + *len, str, slen);
    *len += slen;
    (*buf)[*len] = '\0';
}

static void print_item(const cJSON *item, int depth, int fmt, char **buf, size_t *cap, size_t *len);

static void print_item(const cJSON *item, int depth, int fmt, char **buf, size_t *cap, size_t *len) {
    if (!item) return;

    switch (item->type & 0xFF) {
        case cJSON_NULL:   print_to_buf(buf, cap, len, "null"); break;
        case cJSON_False:  print_to_buf(buf, cap, len, "false"); break;
        case cJSON_True:   print_to_buf(buf, cap, len, "true"); break;
        case cJSON_Number: {
            char num[32];
            snprintf(num, sizeof(num), "%d", item->valueint);
            print_to_buf(buf, cap, len, num);
            break;
        }
        case cJSON_String:
            print_to_buf(buf, cap, len, "\"");
            if (item->valuestring) print_to_buf(buf, cap, len, item->valuestring);
            print_to_buf(buf, cap, len, "\"");
            break;
        case cJSON_Array: {
            print_to_buf(buf, cap, len, "[");
            cJSON *child = item->child;
            while (child) {
                if (fmt) { print_to_buf(buf, cap, len, "\n"); for (int i=0; i<=depth; i++) print_to_buf(buf, cap, len, "\t"); }
                print_item(child, depth + 1, fmt, buf, cap, len);
                child = child->next;
                if (child) print_to_buf(buf, cap, len, ",");
            }
            if (fmt) { print_to_buf(buf, cap, len, "\n"); for (int i=0; i<depth; i++) print_to_buf(buf, cap, len, "\t"); }
            print_to_buf(buf, cap, len, "]");
            break;
        }
        case cJSON_Object: {
            print_to_buf(buf, cap, len, "{");
            cJSON *child = item->child;
            while (child) {
                if (fmt) { print_to_buf(buf, cap, len, "\n"); for (int i=0; i<=depth; i++) print_to_buf(buf, cap, len, "\t"); }
                print_to_buf(buf, cap, len, "\"");
                if (child->string) print_to_buf(buf, cap, len, child->string);
                print_to_buf(buf, cap, len, fmt ? "\": " : "\":");
                print_item(child, depth + 1, fmt, buf, cap, len);
                child = child->next;
                if (child) print_to_buf(buf, cap, len, ",");
            }
            if (fmt) { print_to_buf(buf, cap, len, "\n"); for (int i=0; i<depth; i++) print_to_buf(buf, cap, len, "\t"); }
            print_to_buf(buf, cap, len, "}");
            break;
        }
    }
}

char *cJSON_Print(const cJSON *item) {
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    print_item(item, 0, 1, &buf, &cap, &len);
    return buf;
}

char *cJSON_PrintUnformatted(const cJSON *item) {
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    print_item(item, 0, 0, &buf, &cap, &len);
    return buf;
}

/* ---- Accessors ---- */

int cJSON_GetArraySize(const cJSON *array) {
    int count = 0;
    cJSON *child = array ? array->child : NULL;
    while (child) { count++; child = child->next; }
    return count;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    cJSON *child = array ? array->child : NULL;
    while (child && index > 0) { child = child->next; index--; }
    return child;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string) {
    cJSON *child = object ? object->child : NULL;
    while (child) {
        if (child->string && strcmp(child->string, string) == 0)
            return child;
        child = child->next;
    }
    return NULL;
}

/* ---- Type checks ---- */
int cJSON_IsInvalid(const cJSON *i) { return i ? (i->type & 0xFF) == cJSON_Invalid : 1; }
int cJSON_IsFalse(const cJSON *i)   { return i ? (i->type & 0xFF) == cJSON_False : 0; }
int cJSON_IsTrue(const cJSON *i)    { return i ? (i->type & 0xFF) == cJSON_True : 0; }
int cJSON_IsBool(const cJSON *i)    { return i ? ((i->type & 0xFF) == cJSON_True || (i->type & 0xFF) == cJSON_False) : 0; }
int cJSON_IsNull(const cJSON *i)    { return i ? (i->type & 0xFF) == cJSON_NULL : 0; }
int cJSON_IsNumber(const cJSON *i)  { return i ? (i->type & 0xFF) == cJSON_Number : 0; }
int cJSON_IsString(const cJSON *i)  { return i ? (i->type & 0xFF) == cJSON_String : 0; }
int cJSON_IsArray(const cJSON *i)   { return i ? (i->type & 0xFF) == cJSON_Array : 0; }
int cJSON_IsObject(const cJSON *i)  { return i ? (i->type & 0xFF) == cJSON_Object : 0; }

/* ---- Creators ---- */

cJSON *cJSON_CreateNull(void)  { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_NULL;  return i; }
cJSON *cJSON_CreateTrue(void)  { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_True;  return i; }
cJSON *cJSON_CreateFalse(void) { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_False; return i; }

cJSON *cJSON_CreateBool(int boolean) {
    cJSON *i = cJSON_New_Item();
    if (i) i->type = boolean ? cJSON_True : cJSON_False;
    return i;
}

cJSON *cJSON_CreateNumber(int num) {
    cJSON *i = cJSON_New_Item();
    if (i) { i->type = cJSON_Number; i->valueint = num; }
    return i;
}

cJSON *cJSON_CreateString(const char *string) {
    cJSON *i = cJSON_New_Item();
    if (i) { i->type = cJSON_String; i->valuestring = cJSON_strdup(string); }
    return i;
}

cJSON *cJSON_CreateArray(void)  { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_Array;  return i; }
cJSON *cJSON_CreateObject(void) { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_Object; return i; }

/* ---- Add to array/object ---- */

static void suffix_object(cJSON *prev, cJSON *item) {
    prev->next = item;
    item->prev = prev;
}

int cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    if (!array || !item) return 0;
    cJSON *child = array->child;
    if (!child) { array->child = item; }
    else {
        while (child->next) child = child->next;
        suffix_object(child, item);
    }
    return 1;
}

int cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!object || !item || !string) return 0;
    item->string = cJSON_strdup(string);
    return cJSON_AddItemToArray(object, item);
}

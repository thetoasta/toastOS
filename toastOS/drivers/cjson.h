/*
 * cJSON - Ultralight JSON parser for toastOS
 * Based on cJSON by Dave Gamble (MIT License)
 * Adapted for toastOS kernel environment
 */

#ifndef CJSON_TOAST_H
#define CJSON_TOAST_H

#include "toast_libc.h"
#include "stdio.h"

/* cJSON Types */
#define cJSON_Invalid  0
#define cJSON_False    (1 << 0)
#define cJSON_True     (1 << 1)
#define cJSON_NULL     (1 << 2)
#define cJSON_Number   (1 << 3)
#define cJSON_String   (1 << 4)
#define cJSON_Array    (1 << 5)
#define cJSON_Object   (1 << 6)
#define cJSON_Raw      (1 << 7)

#define cJSON_IsReference  256
#define cJSON_StringIsConst 512

/* The cJSON structure */
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;   /* array or object items */
    int    type;
    char  *valuestring;
    int    valueint;
    char  *string;         /* key name if this is an object child */
} cJSON;

/* Supply a block of JSON, get a cJSON object representing it */
cJSON *cJSON_Parse(const char *value);

/* Render a cJSON tree to text */
char  *cJSON_Print(const cJSON *item);
char  *cJSON_PrintUnformatted(const cJSON *item);

/* Delete a cJSON tree */
void   cJSON_Delete(cJSON *item);

/* Get array/object size */
int    cJSON_GetArraySize(const cJSON *array);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);

/* Type checks */
int    cJSON_IsInvalid(const cJSON *item);
int    cJSON_IsFalse(const cJSON *item);
int    cJSON_IsTrue(const cJSON *item);
int    cJSON_IsBool(const cJSON *item);
int    cJSON_IsNull(const cJSON *item);
int    cJSON_IsNumber(const cJSON *item);
int    cJSON_IsString(const cJSON *item);
int    cJSON_IsArray(const cJSON *item);
int    cJSON_IsObject(const cJSON *item);

/* Create items */
cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateTrue(void);
cJSON *cJSON_CreateFalse(void);
cJSON *cJSON_CreateBool(int boolean);
cJSON *cJSON_CreateNumber(int num);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateObject(void);

/* Add items to arrays/objects */
int    cJSON_AddItemToArray(cJSON *array, cJSON *item);
int    cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);

#endif /* CJSON_TOAST_H */

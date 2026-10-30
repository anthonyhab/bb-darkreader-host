#ifndef BB_JSON_UTILS_H
#define BB_JSON_UTILS_H

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <json-c/json.h>

static inline char *bb_json_get(const char *json, const char *key) {
    if (!json || !key) return NULL;
    const size_t length = strlen(json);
    if (length > INT_MAX) return NULL;

    struct json_tokener *tokener = json_tokener_new_ex(32);
    if (!tokener) return NULL;
    json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);
    struct json_object *root = json_tokener_parse_ex(tokener, json, (int)length);
    const enum json_tokener_error error = json_tokener_get_error(tokener);
    size_t parsed = json_tokener_get_parse_end(tokener);
    json_tokener_free(tokener);
    while (parsed < length &&
           (json[parsed] == ' ' || json[parsed] == '\t' ||
            json[parsed] == '\r' || json[parsed] == '\n')) {
        parsed++;
    }

    char *result = NULL;
    struct json_object *value = NULL;
    if (error == json_tokener_success && parsed == length &&
        root && json_object_is_type(root, json_type_object) &&
        json_object_object_get_ex(root, key, &value) &&
        json_object_is_type(value, json_type_string)) {
        const char *text = json_object_get_string(value);
        const size_t size = (size_t)json_object_get_string_len(value);
        // Native request names are C strings: reject embedded NULs.
        if (text && strlen(text) == size) {
            result = malloc(size + 1);
            if (result) memcpy(result, text, size + 1);
        }
    }
    if (root) json_object_put(root);
    return result;
}

static inline bool bb_json_get_action(const char *json, char *action, size_t max_length) {
    if (!action || max_length == 0) return false;
    char *value = bb_json_get(json, "action");
    if (!value) return false;

    size_t length = strlen(value);
    if (length >= max_length) length = max_length - 1;
    memcpy(action, value, length);
    action[length] = '\0';
    free(value);
    return true;
}

static inline bool bb_json_has_action(const char *json, const char *action) {
    char value[64];
    return bb_json_get_action(json, value, sizeof(value)) &&
           strcmp(value, action) == 0;
}

#endif


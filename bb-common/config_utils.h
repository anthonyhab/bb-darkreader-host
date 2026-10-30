#ifndef BB_CONFIG_UTILS_H
#define BB_CONFIG_UTILS_H

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <json-c/json.h>

#define BB_CONFIG_DIR_SUFFIX "/.mozilla/native-messaging-hosts"

static inline int bb_config_write_string(FILE *file, const char *value) {
    if (!value) return -1;
    struct json_object *string = json_object_new_string(value);
    if (!string) return -1;
    const char *encoded = json_object_to_json_string_ext(string, JSON_C_TO_STRING_PLAIN);
    const int result = encoded && fputs(encoded, file) >= 0 ? 0 : -1;
    json_object_put(string);
    return result;
}

static inline int bb_config_ensure_many(
    const char *exe_path,
    const char *name,
    const char *const *allowed_exts,
    size_t allowed_ext_count
) {
    const char *home = getenv("HOME");
    if (!home || !exe_path || !name || !allowed_exts || allowed_ext_count == 0) {
        return -1;
    }

    char config_dir[PATH_MAX];
    char config_path[PATH_MAX];
    char temporary_path[PATH_MAX];
    if (snprintf(config_dir, sizeof(config_dir), "%s%s", home, BB_CONFIG_DIR_SUFFIX) >=
            (int)sizeof(config_dir) ||
        snprintf(config_path, sizeof(config_path), "%s/%s.json", config_dir, name) >=
            (int)sizeof(config_path) ||
        snprintf(temporary_path, sizeof(temporary_path), "%s.tmp.XXXXXX", config_path) >=
            (int)sizeof(temporary_path)) {
        return -1;
    }

    if (mkdir(config_dir, 0700) != 0 && errno != EEXIST) return -1;

    const int fd = mkstemp(temporary_path);
    if (fd < 0) return -1;
    FILE *file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(temporary_path);
        return -1;
    }

    // Serialize every manifest value as a JSON string, including executable paths.
    char description[PATH_MAX];
    const int description_length = snprintf(
        description, sizeof(description), "Native messaging host for %s", name
    );
    int failed = description_length < 0 || description_length >= (int)sizeof(description) ||
        fputs("{\n  \"name\": ", file) < 0 ||
        bb_config_write_string(file, name) != 0 ||
        fputs(",\n  \"description\": ", file) < 0 ||
        bb_config_write_string(file, description) != 0 ||
        fputs(",\n  \"path\": ", file) < 0 ||
        bb_config_write_string(file, exe_path) != 0 ||
        fputs(",\n  \"type\": \"stdio\",\n  \"allowed_extensions\": [", file) < 0;

    for (size_t i = 0; !failed && i < allowed_ext_count; i++) {
        if ((i && fputs(", ", file) < 0) ||
            bb_config_write_string(file, allowed_exts[i]) != 0) {
            failed = 1;
        }
    }
    if (!failed && fprintf(file, "]\n}\n") < 0) failed = 1;
    if (!failed && fchmod(fd, 0644) != 0) failed = 1;
    if (fclose(file) != 0) failed = 1;

    if (failed || rename(temporary_path, config_path) != 0) {
        unlink(temporary_path);
        return -1;
    }
    return 0;
}

static inline int bb_config_ensure(
    const char *exe_path,
    const char *name,
    const char *allowed_ext
) {
    const char *allowed_exts[] = {allowed_ext};
    return bb_config_ensure_many(exe_path, name, allowed_exts, 1);
}

static inline int bb_config_remove(const char *name) {
    const char *home = getenv("HOME");
    if (!home || !name) return -1;

    char config_path[PATH_MAX];
    if (snprintf(
            config_path,
            sizeof(config_path),
            "%s%s/%s.json",
            home,
            BB_CONFIG_DIR_SUFFIX,
            name
        ) >= (int)sizeof(config_path)) {
        return -1;
    }

    return unlink(config_path);
}

#endif


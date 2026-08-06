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

#define BB_CONFIG_DIR_SUFFIX "/.mozilla/native-messaging-hosts"

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
        snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", config_path) >=
            (int)sizeof(temporary_path)) {
        return -1;
    }

    if (mkdir(config_dir, 0700) != 0 && errno != EEXIST) return -1;

    FILE *file = fopen(temporary_path, "w");
    if (!file) return -1;

    int failed = fprintf(
        file,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"description\": \"Native messaging host for %s\",\n"
        "  \"path\": \"%s\",\n"
        "  \"type\": \"stdio\",\n"
        "  \"allowed_extensions\": [",
        name,
        name,
        exe_path
    ) < 0;

    for (size_t i = 0; !failed && i < allowed_ext_count; i++) {
        if (!allowed_exts[i] ||
            fprintf(file, "%s\"%s\"", i ? ", " : "", allowed_exts[i]) < 0) {
            failed = 1;
        }
    }
    if (!failed && fprintf(file, "]\n}\n") < 0) failed = 1;
    if (fclose(file) != 0) failed = 1;

    if (failed || chmod(temporary_path, 0644) != 0 ||
        rename(temporary_path, config_path) != 0) {
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

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json-c/json.h>

#include "bb-common/config_utils.h"
#include "bb-common/json_utils.h"
#include "bb-common/native_messaging.h"

#define DEFAULT_COLORS_SUFFIX "/.cache/wal/colors.json"
#define COLORS_FILE_SIZE (64 * 1024)
#define INOTIFY_BUFFER_SIZE (16 * 1024)
#define RESPONSE_SIZE 512
#define WATCH_RETRY_MS 250

static char last_theme_response[RESPONSE_SIZE];
static char last_error[128];

static int get_colors_path(char *path, size_t size) {
    const char *override = getenv("DARKREADER_COLORS_PATH");
    if (override && override[0]) {
        return snprintf(path, size, "%s", override) < (int)size ? 0 : -1;
    }

    const char *home = getenv("HOME");
    if (!home) return -1;
    return snprintf(path, size, "%s%s", home, DEFAULT_COLORS_SUFFIX) < (int)size ? 0 : -1;
}

static int valid_color(const char *color) {
    if (!color || strlen(color) != 7 || color[0] != '#') return 0;
    for (size_t i = 1; i < 7; i++) {
        const char c = color[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            return 0;
        }
    }
    return 1;
}

static int validate_relevant_keys(const char *source, size_t length) {
    static const char *keys[] = {
        "special",
        "colors",
        "background", "foreground", "color0", "color7", "color10",
    };
    int counts[sizeof(keys) / sizeof(keys[0])] = {0};

    for (size_t i = 0; i < length; i++) {
        if (source[i] != '"') continue;
        const size_t start = ++i;
        int escaped = 0;
        while (i < length && source[i] != '"') {
            if (source[i] == '\\') {
                escaped = 1;
                if (++i >= length) return -1;
            }
            i++;
        }
        if (i >= length) return -1;

        size_t next = i + 1;
        while (next < length &&
               (source[next] == ' ' || source[next] == '\t' ||
                source[next] == '\r' || source[next] == '\n')) {
            next++;
        }
        if (next >= length || source[next] != ':') continue;
        if (escaped) return -1;

        const size_t key_length = i - start;
        for (size_t key_index = 0; key_index < sizeof(keys) / sizeof(keys[0]); key_index++) {
            if (strlen(keys[key_index]) == key_length &&
                memcmp(source + start, keys[key_index], key_length) == 0 &&
                ++counts[key_index] > 1) {
                return -1;
            }
        }
    }
    return 0;
}

static int copy_object_color(
    struct json_object *object,
    const char *key,
    char target[8]
) {
    struct json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return -1;
    }
    const char *color = json_object_get_string(value);
    if (!valid_color(color)) return -1;
    memcpy(target, color, 8);
    return 0;
}

static int read_theme_response(const char *path, char *response, size_t capacity) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return -1;

    struct stat status;
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode)) {
        close(fd);
        return -1;
    }

    FILE *file = fdopen(fd, "r");
    if (!file) {
        close(fd);
        return -1;
    }

    char source[COLORS_FILE_SIZE];
    const size_t length = fread(source, 1, sizeof(source) - 1, file);
    const int read_error = ferror(file);
    if (fclose(file) != 0 || read_error || length == sizeof(source) - 1) return -1;
    source[length] = '\0';
    if (validate_relevant_keys(source, length) != 0) return -1;

    struct json_tokener *tokener = json_tokener_new_ex(32);
    if (!tokener) return -1;
    json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);
    struct json_object *root = json_tokener_parse_ex(tokener, source, (int)length);
    const enum json_tokener_error parse_error = json_tokener_get_error(tokener);
    size_t parsed_length = json_tokener_get_parse_end(tokener);
    while (parsed_length < length &&
           (source[parsed_length] == ' ' || source[parsed_length] == '\t' ||
            source[parsed_length] == '\r' || source[parsed_length] == '\n')) {
        parsed_length++;
    }
    json_tokener_free(tokener);
    if (parse_error != json_tokener_success || parsed_length != length ||
        !root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }

    struct json_object *special = NULL;
    struct json_object *colors = NULL;
    if (json_object_object_get_ex(root, "special", &special) &&
        !json_object_is_type(special, json_type_object)) {
        json_object_put(root);
        return -1;
    }
    if (json_object_object_get_ex(root, "colors", &colors) &&
        !json_object_is_type(colors, json_type_object)) {
        json_object_put(root);
        return -1;
    }
    if (!special && !colors) {
        json_object_put(root);
        return -1;
    }

    char background[8];
    char foreground[8];
    char selection[8];
    const int valid_background = copy_object_color(special, "background", background) == 0 ||
                                 copy_object_color(colors, "color0", background) == 0;
    const int valid_foreground = copy_object_color(special, "foreground", foreground) == 0 ||
                                 copy_object_color(colors, "color7", foreground) == 0;
    if (!valid_background || !valid_foreground) {
        json_object_put(root);
        return -1;
    }
    if (copy_object_color(colors, "color10", selection) != 0) {
        memcpy(selection, foreground, sizeof(selection));
    }
    json_object_put(root);

    const int result = snprintf(
        response,
        capacity,
        "{\"type\":\"setTheme\",\"data\":{"
        "\"darkSchemeBackgroundColor\":\"%s\","
        "\"darkSchemeTextColor\":\"%s\","
        "\"selectionColor\":\"%s\"},\"isNative\":true}",
        background,
        foreground,
        selection
    );
    return result >= 0 && (size_t)result < capacity ? 0 : -1;
}

static void send_error(const char *message, int force) {
    if (!force && strcmp(last_error, message) == 0) return;
    snprintf(last_error, sizeof(last_error), "%s", message);

    char response[RESPONSE_SIZE];
    snprintf(
        response,
        sizeof(response),
        "{\"type\":\"error\",\"data\":{\"message\":\"%s\"},\"isNative\":true}",
        message
    );
    nm_send_str(response);
}

static void send_reset(void) {
    last_theme_response[0] = '\0';
    last_error[0] = '\0';
    nm_send_str("{\"type\":\"resetThemeVars\",\"isNative\":true}");
}

static int send_theme(const char *path, int force) {
    char response[RESPONSE_SIZE];
    if (read_theme_response(path, response, sizeof(response)) != 0) {
        send_error("Palette file is missing or invalid", force);
        return -1;
    }
    last_error[0] = '\0';
    if (!force && strcmp(response, last_theme_response) == 0) return 0;

    snprintf(last_theme_response, sizeof(last_theme_response), "%s", response);
    nm_send_str(response);
    return 0;
}

static int is_message_type(const char *message, const char *expected) {
    char quoted[64];
    if (snprintf(quoted, sizeof(quoted), "\"%s\"", expected) < (int)sizeof(quoted) &&
        strcmp(message, quoted) == 0) {
        return 1;
    }
    char *type = bb_json_get(message, "type");
    const int matches = type && strcmp(type, expected) == 0;
    free(type);
    return matches;
}

static int setup_inotify(const char *colors_path, char *filename, size_t filename_size) {
    char directory[PATH_MAX];
    if (snprintf(directory, sizeof(directory), "%s", colors_path) >= (int)sizeof(directory)) {
        return -1;
    }

    char *separator = strrchr(directory, '/');
    if (!separator || !separator[1]) return -1;
    snprintf(filename, filename_size, "%s", separator + 1);
    *separator = '\0';

    const int fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (fd < 0) return -1;
    if (inotify_add_watch(
            fd,
            directory,
            IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_DELETE |
                IN_DELETE_SELF | IN_MOVE_SELF
        ) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int handle_inotify(int fd, const char *filename, const char *colors_path) {
    char buffer[INOTIFY_BUFFER_SIZE];
    int should_send = 0;
    int should_reset = 0;
    int watch_lost = 0;

    for (;;) {
        const ssize_t length = read(fd, buffer, sizeof(buffer));
        if (length < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) break;
            return -1;
        }
        if (length == 0) break;

        size_t offset = 0;
        while (offset < (size_t)length) {
            const struct inotify_event *event =
                (const struct inotify_event *)(buffer + offset);
            if (event->mask & (IN_IGNORED | IN_DELETE_SELF | IN_MOVE_SELF | IN_Q_OVERFLOW)) {
                watch_lost = 1;
            }
            if (event->len && strcmp(event->name, filename) == 0) {
                if (event->mask & IN_DELETE) {
                    should_reset = 1;
                } else {
                    should_send = 1;
                }
            }
            offset += sizeof(*event) + event->len;
        }
    }

    if (should_reset) {
        send_reset();
    } else if (should_send) {
        send_theme(colors_path, 0);
    }
    return watch_lost ? -1 : 0;
}

static int install_manifest(const char *executable_path) {
    const char *allowed_extensions[] = {
        "coloreader@bb.hab.rip",
        "darkreader@alexhulbert.com",
    };
    const int result = bb_config_ensure_many(
        executable_path,
        "darkreader",
        allowed_extensions,
        sizeof(allowed_extensions) / sizeof(allowed_extensions[0])
    );
    if (result == 0) {
        printf("Dark Reader native messaging manifest installed. Restart Firefox.\n");
    }
    return result == 0 ? 0 : 1;
}

int main(int argc, char *argv[]) {
    char colors_path[PATH_MAX];
    if (get_colors_path(colors_path, sizeof(colors_path)) != 0) {
        fprintf(stderr, "Unable to determine colors.json path\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "start") != 0) {
        if (strcmp(argv[1], "install") == 0) {
            char executable_path[PATH_MAX];
            if (!realpath("/proc/self/exe", executable_path)) return 1;
            return install_manifest(executable_path);
        }
        if (strcmp(argv[1], "uninstall") == 0) {
            return bb_config_remove("darkreader") == 0 ? 0 : 1;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf(
                "bb-darkreader-host - Native messaging host for Dark Reader\n\n"
                "Usage: bb-darkreader-host [start|install|uninstall]\n"
            );
            return 0;
        }
        // Firefox passes the connecting extension origin as an argument.
        // Unrecognized arguments therefore enter native-host mode.
    }

    char watched_filename[NAME_MAX + 1] = "";
    int inotify_fd = setup_inotify(
        colors_path,
        watched_filename,
        sizeof(watched_filename)
    );
    int watch_needs_sync = inotify_fd < 0;

    for (;;) {
        if (inotify_fd < 0) {
            inotify_fd = setup_inotify(
                colors_path,
                watched_filename,
                sizeof(watched_filename)
            );
            if (inotify_fd >= 0 && watch_needs_sync) {
                send_theme(colors_path, 0);
                watch_needs_sync = 0;
            }
        }
        fd_set descriptors;
        FD_ZERO(&descriptors);
        FD_SET(STDIN_FILENO, &descriptors);
        int maximum = STDIN_FILENO;
        if (inotify_fd >= 0) {
            FD_SET(inotify_fd, &descriptors);
            if (inotify_fd > maximum) maximum = inotify_fd;
        }

        struct timeval retry_timeout = {
            .tv_sec = 0,
            .tv_usec = WATCH_RETRY_MS * 1000,
        };
        struct timeval *timeout = inotify_fd < 0 ? &retry_timeout : NULL;
        if (select(maximum + 1, &descriptors, NULL, NULL, timeout) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &descriptors)) {
            uint8_t message[MAX_MSG_SIZE + 1];
            const int length = nm_read_message(message, sizeof(message));
            if (length < 0) break;
            if (is_message_type((const char *)message, "init")) {
                send_theme(colors_path, 1);
            } else if (is_message_type((const char *)message, "reset")) {
                send_reset();
            } else {
                send_error("Unsupported native request", 1);
            }
        }
        if (inotify_fd >= 0 && FD_ISSET(inotify_fd, &descriptors) &&
            handle_inotify(inotify_fd, watched_filename, colors_path) != 0) {
            close(inotify_fd);
            inotify_fd = -1;
            watch_needs_sync = 1;
        }
    }

    if (inotify_fd >= 0) close(inotify_fd);
    return 0;
}

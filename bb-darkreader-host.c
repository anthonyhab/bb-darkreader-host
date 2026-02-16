#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/select.h>

#include "../bb-common/native_messaging.h"
#include "../bb-common/json_utils.h"
#include "../bb-common/config_utils.h"

#define COLORS_PATH "/.cache/wal/colors"
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_SIZE (1024 * (EVENT_SIZE + 16))

static void send_theme(const char *bg, const char *fg, const char *sel) {
    char json[256];
    int len = snprintf(json, sizeof(json),
        "{\"type\":\"setTheme\",\"data\":{"
        "\"darkSchemeBackgroundColor\":\"%s\","
        "\"darkSchemeTextColor\":\"%s\","
        "\"selectionColor\":\"%s\"},\"isNative\":true}",
        bg, fg, sel);

    nm_send_message(json, (size_t)len);
}

static void parse_and_send(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        send_theme("#000000", "#ffffff", "#ffffff");
        return;
    }
    
    char json[8192];
    size_t len = fread(json, 1, sizeof(json) - 1, f);
    json[len] = '\0';
    fclose(f);
    
    char bg[8] = "#000000", fg[8] = "#ffffff", sel[8] = "#ffffff";
    
    if (strstr(json, "\"special\"")) {
        char *v;
        if ((v = bb_json_get(json, "background"))) { memcpy(bg, v, 7); bg[7] = '\0'; free(v); }
        if ((v = bb_json_get(json, "foreground"))) { memcpy(fg, v, 7); fg[7] = '\0'; free(v); }
        if ((v = bb_json_get(json, "cursor")) && strlen(v) > 0) { memcpy(sel, v, 7); sel[7] = '\0'; free(v); }
        else if ((v = bb_json_get(json, "color2"))) { memcpy(sel, v, 7); sel[7] = '\0'; free(v); }
    }
    else if (strstr(json, "\"colors\"")) {
        char *v;
        if ((v = bb_json_get(json, "color0"))) { memcpy(bg, v, 7); bg[7] = '\0'; free(v); }
        if ((v = bb_json_get(json, "color7"))) { memcpy(fg, v, 7); fg[7] = '\0'; free(v); }
        if ((v = bb_json_get(json, "color2"))) { memcpy(sel, v, 7); sel[7] = '\0'; free(v); }
    }
    
    send_theme(bg, fg, sel);
}

int main(int argc, char *argv[]) {
    char exe_path[512];
    realpath("/proc/self/exe", exe_path);
    
    const char *home = getenv("HOME");
    if (!home) return 1;
    
    if (argc > 1) {
        if (strcmp(argv[1], "install") == 0) {
            bb_config_ensure(exe_path, "darkreader", "darkreader@alexhulbert.com");
            printf("Dark Reader config created at:\n");
            printf("  ~/.mozilla/native-messaging-hosts/darkreader.json\n\n");
            printf("Please restart Firefox to apply changes.\n");
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("bb-darkreader-host - Native messaging host for Dark Reader\n\n");
            printf("Usage:\n");
            printf("  bb-darkreader-host           Start daemon (for Firefox)\n");
            printf("  bb-darkreader-host install   Install manifest\n");
            return 0;
        }
    }
    
    char path[512];
    snprintf(path, sizeof(path), "%s%s", home, COLORS_PATH);

    parse_and_send(path);

    int fd = inotify_init();
    if (fd >= 0) inotify_add_watch(fd, path, IN_CLOSE_WRITE);

    char buf[BUF_SIZE];
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        FD_SET(STDIN_FILENO, &fds);
        
        if (select(fd > STDIN_FILENO ? fd + 1 : STDIN_FILENO + 1, &fds, NULL, NULL, NULL) < 0) break;
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            uint8_t msg[4096];
            if (nm_read_message(msg, sizeof(msg)) < 0) break;
        }
        if (FD_ISSET(fd, &fds)) { read(fd, buf, BUF_SIZE); parse_and_send(path); }
    }

    if (fd >= 0) close(fd);
    return 0;
}

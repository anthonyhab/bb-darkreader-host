#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <stdint.h>

#define COLORS_PATH_SUFFIX "/.cache/wal/colors"
#define CONFIG_DIR_SUFFIX "/.mozilla/native-messaging-hosts"
#define CONFIG_FILE "darkreader.json"
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_SIZE (1024 * (EVENT_SIZE + 16))
#define MAX_PATH 512
#define MAX_JSON_SIZE 8192

static void ensure_config_exists(const char *exe_path) {
    char config_dir[MAX_PATH];
    char config_path[MAX_PATH];
    const char *home = getenv("HOME");
    
    if (!home) return;
    
    snprintf(config_dir, sizeof(config_dir), "%s%s", home, CONFIG_DIR_SUFFIX);
    snprintf(config_path, sizeof(config_path), "%s/%s", config_dir, CONFIG_FILE);
    
    struct stat st;
    if (stat(config_path, &st) == 0) return;
    
    mkdir(config_dir, 0700);
    
    FILE *f = fopen(config_path, "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"darkreader\",\n");
    fprintf(f, "  \"description\": \"Native messaging host for syncing pywal colors with Dark Reader\",\n");
    fprintf(f, "  \"path\": \"%s\",\n", exe_path);
    fprintf(f, "  \"type\": \"stdio\",\n");
    fprintf(f, "  \"allowed_extensions\": [\"darkreader@alexhulbert.com\"]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    chmod(config_path, 0644);
}

static void send_theme(const char *bg, const char *fg, const char *sel) {
    char json[512];
    int len = snprintf(json, sizeof(json),
        "{\"type\":\"setTheme\",\"data\":{"
        "\"darkSchemeBackgroundColor\":\"%s\","
        "\"darkSchemeTextColor\":\"%s\","
        "\"selectionColor\":\"%s\"},\"isNative\":true}",
        bg, fg, sel);

    uint32_t msg_len = len;
    fwrite(&msg_len, 4, 1, stdout);
    fwrite(json, 1, len, stdout);
    fflush(stdout);
}

static char *find_json_value(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    
    char *pos = strstr(json, search);
    if (!pos) return NULL;
    
    pos = strchr(pos, ':');
    if (!pos) return NULL;
    pos++;
    
    while (*pos == ' ' || *pos == '\t') pos++;
    
    if (*pos == '"') {
        pos++;
        char *end = strchr(pos, '"');
        if (!end) return NULL;
        size_t len = end - pos;
        char *result = malloc(len + 1);
        strncpy(result, pos, len);
        result[len] = '\0';
        return result;
    }
    
    return NULL;
}

static void parse_and_send_json(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    
    char json[MAX_JSON_SIZE];
    size_t len = fread(json, 1, MAX_JSON_SIZE - 1, f);
    json[len] = '\0';
    fclose(f);
    
    char bg[32] = "#000000";
    char fg[32] = "#ffffff";
    char sel[32] = "#ffffff";
    
    if (strstr(json, "\"special\"")) {
        char *bg_val = find_json_value(json, "background");
        char *fg_val = find_json_value(json, "foreground");
        char *sel_val = find_json_value(json, "cursor");
        
        if (bg_val) { strncpy(bg, bg_val, 31); free(bg_val); }
        if (fg_val) { strncpy(fg, fg_val, 31); free(fg_val); }
        if (sel_val && strlen(sel_val) > 0) { 
            strncpy(sel, sel_val, 31); 
        } else {
            // Fallback to color2 if cursor is empty
            char *c2 = find_json_value(json, "color2");
            if (c2) { strncpy(sel, c2, 31); free(c2); }
        }
        if (sel_val) free(sel_val);
    }
    else if (strstr(json, "\"colors\"")) {
        char *c0 = find_json_value(json, "color0");
        char *c7 = find_json_value(json, "color7");
        char *c2 = find_json_value(json, "color2");
        
        if (c0) { strncpy(bg, c0, 31); free(c0); }
        if (c7) { strncpy(fg, c7, 31); free(c7); }
        if (c2) { strncpy(sel, c2, 31); free(c2); }
    }
    
    send_theme(bg, fg, sel);
}

static void parse_and_send_line(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char colors[16][32];
    int i = 0;
    while (i < 16 && fgets(colors[i], sizeof(colors[i]), f)) {
        colors[i][strcspn(colors[i], "\n")] = 0;
        i++;
    }
    fclose(f);

    if (i >= 8) {
        send_theme(colors[0], colors[7], colors[2]);
    }
}

static void read_and_send(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    
    char first[2];
    size_t n = fread(first, 1, 1, f);
    fclose(f);
    
    if (n > 0 && first[0] == '{') {
        parse_and_send_json(path);
    } else {
        parse_and_send_line(path);
    }
}

int main(int argc, char *argv[]) {
    char exe_path[MAX_PATH];
    realpath("/proc/self/exe", exe_path);
    
    // Handle --setup command
    if (argc > 1 && strcmp(argv[1], "--setup") == 0) {
        ensure_config_exists(exe_path);
        printf("Native messaging config created at:\n");
        printf("  ~/.mozilla/native-messaging-hosts/darkreader.json\n");
        printf("\nPlease restart Firefox to apply changes.\n");
        return 0;
    }
    
    // Normal daemon mode
    char path[MAX_PATH];
    const char *home = getenv("HOME");
    
    if (!home) return 1;
    
    ensure_config_exists(exe_path);
    
    snprintf(path, sizeof(path), "%s%s", home, COLORS_PATH_SUFFIX);

    read_and_send(path);

    int fd = inotify_init();
    if (fd < 0) return 1;

    int wd = inotify_add_watch(fd, path, IN_MODIFY | IN_CLOSE_WRITE);
    if (wd < 0) return 1;

    char buf[BUF_SIZE];
    while (1) {
        int len = read(fd, buf, BUF_SIZE);
        if (len <= 0) break;
        read_and_send(path);
    }

    return 0;
}

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Paths.h"

#define ACTIVE_LINK MISTER_PAT_ROOT "/mister-pats-gui.so"
#define PREFIX "mister-pats-gui-"
#define SUFFIX ".so"

typedef struct {
    char name[NAME_MAX + 1];
    unsigned version[3];
} Candidate;

static int parse_component(const char **cursor, unsigned *value, char delimiter) {
    const char *p = *cursor;
    unsigned number = 0;
    if (*p < '0' || *p > '9') return 0;
    if (*p == '0' && p[1] >= '0' && p[1] <= '9') return 0;
    do {
        number = number * 10u + (unsigned)(*p - '0');
        if (number > 65535u) return 0;
        ++p;
    } while (*p >= '0' && *p <= '9');
    if (*p != delimiter) return 0;
    *value = number;
    *cursor = p + 1;
    return 1;
}

static int parse_name(const char *name, unsigned version[3]) {
    const size_t prefix_length = sizeof(PREFIX) - 1;
    if (strncmp(name, PREFIX, prefix_length) != 0) return 0;
    const char *p = name + prefix_length;
    if (!parse_component(&p, &version[0], '.') ||
        !parse_component(&p, &version[1], '.') ||
        !parse_component(&p, &version[2], '.')) return 0;
    return strcmp(p, "so") == 0;
}

static int regular_library(const char *name) {
    char path[PATH_MAX];
    struct stat st;
    if (snprintf(path, sizeof(path), "%s/%s", MISTER_PAT_ROOT, name) >= (int)sizeof(path))
        return 0;
    return lstat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int compare_candidates(const void *left, const void *right) {
    const Candidate *a = (const Candidate *)left;
    const Candidate *b = (const Candidate *)right;
    for (int i = 0; i < 3; ++i) {
        if (a->version[i] != b->version[i])
            return a->version[i] < b->version[i] ? 1 : -1;
    }
    return 0;
}

static int launch(const char *name, int argc, char **argv, int *load_error, int *started) {
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/%s", MISTER_PAT_ROOT, name) >= (int)sizeof(path)) {
        fprintf(stderr, "loader: path too long: %s\n", name);
        if (*load_error < 1) *load_error = 1;
        return -1;
    }
    void *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "loader: dlopen %s: %s\n", path, dlerror());
        if (*load_error < 1) *load_error = 1;
        return -1;
    }
    dlerror();
    int (*entry)(int, char **) = (int (*)(int, char **))dlsym(library, "mister_gui_main_v1");
    const char *symbol_error = dlerror();
    if (symbol_error || !entry) {
        fprintf(stderr, "loader: dlsym %s: %s\n", path,
                symbol_error ? symbol_error : "missing entry point");
        *load_error = 2;
        dlclose(library);
        return -1;
    }
    fprintf(stderr, "loader: starting %s\n", path);
    /* The GUI remains loaded for the lifetime of this process. */
    *started = 1;
    return entry(argc, argv);
}

int main(int argc, char **argv) {
    int invalid = 0;
    int load_error = 0;
    int attempted = 0;
    char active[NAME_MAX + 1] = {0};
    unsigned active_version[3];
    struct stat link_stat;

    if (lstat(ACTIVE_LINK, &link_stat) == 0) {
        if (S_ISLNK(link_stat.st_mode)) {
            ssize_t length = readlink(ACTIVE_LINK, active, sizeof(active) - 1);
            if (length >= 0) active[length] = '\0';
            if (length < 0 || !parse_name(active, active_version) || !regular_library(active)) {
                fprintf(stderr, "loader: invalid active link %s -> %s\n", ACTIVE_LINK,
                        length < 0 ? strerror(errno) : active);
                active[0] = '\0';
                invalid = 1;
            }
        } else {
            fprintf(stderr, "loader: active path is not a symlink: %s\n", ACTIVE_LINK);
            invalid = 1;
        }
    } else if (errno != ENOENT) {
        fprintf(stderr, "loader: cannot inspect %s: %s\n", ACTIVE_LINK, strerror(errno));
        invalid = 1;
    }

    if (active[0]) {
        attempted = 1;
        int started = 0;
        int result = launch(active, argc, argv, &load_error, &started);
        if (started) return result;
    }

    DIR *directory = opendir(MISTER_PAT_ROOT);
    if (!directory) {
        fprintf(stderr, "loader: cannot open %s: %s\n", MISTER_PAT_ROOT, strerror(errno));
        return load_error == 2 ? 73 : load_error == 1 ? 72 : invalid ? 71 : 70;
    }
    Candidate *candidates = NULL;
    size_t count = 0;
    size_t capacity = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        unsigned version[3];
        if (strcmp(entry->d_name, active) == 0 || !parse_name(entry->d_name, version)) continue;
        if (!regular_library(entry->d_name)) {
            invalid = 1;
            continue;
        }
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 8;
            Candidate *grown = (Candidate *)realloc(candidates, next * sizeof(*candidates));
            if (!grown) {
                fprintf(stderr, "loader: out of memory while scanning libraries\n");
                free(candidates);
                closedir(directory);
                return 74;
            }
            candidates = grown;
            capacity = next;
        }
        strcpy(candidates[count].name, entry->d_name);
        memcpy(candidates[count].version, version, sizeof(version));
        ++count;
    }
    closedir(directory);
    qsort(candidates, count, sizeof(*candidates), compare_candidates);
    for (size_t i = 0; i < count; ++i) {
        attempted = 1;
        int started = 0;
        int result = launch(candidates[i].name, argc, argv, &load_error, &started);
        if (started) {
            free(candidates);
            return result;
        }
    }
    free(candidates);
    fprintf(stderr, "loader: no usable GUI library, active=%s\n", active[0] ? active : "(none)");
    if (load_error == 2) return 73;
    if (load_error == 1) return 72;
    return invalid ? 71 : attempted ? 72 : 70;
}

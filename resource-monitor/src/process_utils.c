#define _POSIX_C_SOURCE 200809L

#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/**
 * Verifica se um processo existe
 */
int process_exists(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return (access(path, F_OK) == 0) ? 1 : 0;
}

/**
 * Obtém o nome do processo
 */
int get_process_name(pid_t pid, char *name, size_t size) {
    if (name == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    if (fgets(name, size, fp) == NULL) {
        fclose(fp);
        return -1;
    }

    size_t len = strlen(name);
    if (len > 0 && name[len - 1] == '\n') {
        name[len - 1] = '\0';
    }

    fclose(fp);
    return 0;
}

#define _POSIX_C_SOURCE 200809L

#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/**
 * Valida se uma string é um número válido
 */
int is_valid_number(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    
    return 1;
}

/**
 * Valida se um PID é válido
 */
int is_valid_pid(pid_t pid) {
    return (pid > 0 && pid <= 2147483647);
}

/**
 * Sanitiza nome de arquivo para evitar path traversal
 */
int sanitize_filename(const char *input, char *output, size_t size) {
    if (input == NULL || output == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    // Não permitir ".." ou "/" no nome
    if (strstr(input, "..") != NULL || strchr(input, '/') != NULL) {
        errno = EINVAL;
        return -1;
    }
    
    strncpy(output, input, size - 1);
    output[size - 1] = '\0';
    
    return 0;
}

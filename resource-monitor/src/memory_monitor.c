#define _POSIX_C_SOURCE 199309L
#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/**
 * Lê métricas de memória de /proc/[pid]/status e /proc/[pid]/stat
 * 
 * @param pid Process ID a ser monitorado
 * @param metrics Ponteiro para estrutura que receberá as métricas
 * @return 0 em sucesso, -1 em erro
 */
int collect_memory_metrics(pid_t pid, memory_metrics_t *metrics) {
    if (metrics == NULL) {
        fprintf(stderr, "Error: metrics pointer is NULL\n");
        errno = EINVAL;
        return -1;
    }

    // Inicializar estrutura
    memset(metrics, 0, sizeof(memory_metrics_t));

    // === Ler /proc/[pid]/status para informações detalhadas ===
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long value;

        // VmRSS: Resident Set Size (memória física usada)
        if (sscanf(line, "VmRSS: %lu kB", &value) == 1) {
            metrics->rss = value * 1024; // Converter para bytes
            continue;
        }

        // VmSize: Virtual Memory Size (memória virtual total)
        if (sscanf(line, "VmSize: %lu kB", &value) == 1) {
            metrics->vsz = value * 1024; // Converter para bytes
            continue;
        }

        // VmSwap: Quantidade em swap
        if (sscanf(line, "VmSwap: %lu kB", &value) == 1) {
            metrics->swap = value * 1024; // Converter para bytes
            continue;
        }
    }
    fclose(fp);

    // === Ler /proc/[pid]/stat para page faults ===
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        return -1;
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "Error reading %s: %s\n", path, strerror(errno));
        fclose(fp);
        return -1;
    }
    fclose(fp);

    // Parsear page faults
    // Campos: 10=minflt, 12=majflt
    char *comm_end = strrchr(line, ')');
    if (comm_end == NULL) {
        fprintf(stderr, "Error: malformed stat file\n");
        errno = EINVAL;
        return -1;
    }

    // Variáveis temporárias para campos que não usamos
    char state;
    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned int flags;
    unsigned long minflt, cminflt, majflt;
    
    int parsed = sscanf(comm_end + 2,
        "%c "                    // state
        "%d %d %d %d %d "        // ppid, pgrp, session, tty_nr, tpgid
        "%u "                    // flags
        "%lu "                   // minflt (10)
        "%lu "                   // cminflt (11)
        "%lu",                   // majflt (12)
        &state,
        &ppid, &pgrp, &session, &tty_nr, &tpgid,
        &flags,
        &minflt,
        &cminflt,
        &majflt
    );

    if (parsed == 10) {
        // Total de page faults = minor + major
        metrics->page_faults = minflt + majflt;
    } else {
        metrics->page_faults = 0;
    }

    return 0;
}

/**
 * Formata tamanho de memória para string legível (KB, MB, GB)
 */
static void format_memory_size(uint64_t bytes, char *buffer, size_t size) {
    if (bytes < 1024) {
        snprintf(buffer, size, "%lu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, size, "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, size, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/**
 * Imprime métricas de memória formatadas
 */
void print_memory_metrics(const memory_metrics_t *metrics) {
    if (metrics == NULL) {
        return;
    }

    char rss_str[64], vsz_str[64], swap_str[64];
    format_memory_size(metrics->rss, rss_str, sizeof(rss_str));
    format_memory_size(metrics->vsz, vsz_str, sizeof(vsz_str));
    format_memory_size(metrics->swap, swap_str, sizeof(swap_str));

    printf("Memory Metrics:\n");
    printf("  RSS (Physical):   %s (%lu bytes)\n", rss_str, metrics->rss);
    printf("  VSZ (Virtual):    %s (%lu bytes)\n", vsz_str, metrics->vsz);
    printf("  Swap:             %s (%lu bytes)\n", swap_str, metrics->swap);
    printf("  Page Faults:      %lu\n", metrics->page_faults);
}

/**
 * Calcula percentual de memória usada em relação ao total do sistema
 */
double get_memory_usage_percent(const memory_metrics_t *metrics) {
    if (metrics == NULL || metrics->rss == 0) {
        return 0.0;
    }

    // Ler memória total do sistema de /proc/meminfo
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1.0;
    }

    uint64_t total_memory = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long value;
        if (sscanf(line, "MemTotal: %lu kB", &value) == 1) {
            total_memory = value * 1024; // Converter para bytes
            break;
        }
    }
    fclose(fp);

    if (total_memory == 0) {
        return -1.0;
    }

    return ((double)metrics->rss / (double)total_memory) * 100.0;
}

/**
 * Monitora múltiplas leituras e detecta memory leak
 * Retorna taxa de crescimento em bytes/segundo
 */
typedef struct {
    uint64_t initial_rss;
    uint64_t last_rss;
    time_t start_time;
    time_t last_time;
    int initialized;
} memory_leak_detector_t;

static memory_leak_detector_t leak_detector = {0};

double detect_memory_leak(const memory_metrics_t *metrics) {
    if (metrics == NULL) {
        return 0.0;
    }

    time_t current_time = time(NULL);

    if (!leak_detector.initialized) {
        leak_detector.initial_rss = metrics->rss;
        leak_detector.last_rss = metrics->rss;
        leak_detector.start_time = current_time;
        leak_detector.last_time = current_time;
        leak_detector.initialized = 1;
        return 0.0;
    }

    time_t elapsed = current_time - leak_detector.start_time;
    if (elapsed == 0) {
        return 0.0;
    }

    int64_t growth = (int64_t)metrics->rss - (int64_t)leak_detector.initial_rss;
    double rate = (double)growth / (double)elapsed;

    leak_detector.last_rss = metrics->rss;
    leak_detector.last_time = current_time;

    return rate;
}

void reset_memory_leak_detector(void) {
    memset(&leak_detector, 0, sizeof(leak_detector));
}
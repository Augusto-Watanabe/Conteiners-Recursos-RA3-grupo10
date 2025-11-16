#define _POSIX_C_SOURCE 199309L

#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

// Estrutura para armazenar estado anterior (para calcular taxas)
typedef struct {
    uint64_t last_bytes_read;
    uint64_t last_bytes_written;
    struct timespec last_timestamp;
    int initialized;
} io_state_t;

// Estado global para cálculo de taxas
static io_state_t io_state = {0};

/**
 * Lê o arquivo /proc/[pid]/io e extrai métricas de I/O
 * 
 * @param pid Process ID a ser monitorado
 * @param metrics Ponteiro para estrutura que receberá as métricas
 * @return 0 em sucesso, -1 em erro
 */
int collect_io_metrics(pid_t pid, io_metrics_t *metrics) {
    if (metrics == NULL) {
        fprintf(stderr, "Error: metrics pointer is NULL\n");
        errno = EINVAL;
        return -1;
    }

    // Inicializar estrutura
    memset(metrics, 0, sizeof(io_metrics_t));

    // Construir caminho do arquivo /proc/[pid]/io
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);

    // Abrir arquivo (requer permissões)
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        // Nota: /proc/[pid]/io requer permissões especiais
        if (errno == EACCES) {
            fprintf(stderr, "Error: Permission denied. Try running with sudo.\n");
        } else {
            fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        }
        return -1;
    }

    // Ler e parsear cada linha
    char line[256];
    int fields_found = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long long value;

        // rchar: caracteres lidos (incluindo cache)
        if (sscanf(line, "rchar: %llu", &value) == 1) {
            // Não usamos rchar diretamente, preferimos read_bytes
            fields_found++;
            continue;
        }

        // wchar: caracteres escritos (incluindo cache)
        if (sscanf(line, "wchar: %llu", &value) == 1) {
            // Não usamos wchar diretamente, preferimos write_bytes
            fields_found++;
            continue;
        }

        // syscr: número de syscalls de leitura
        if (sscanf(line, "syscr: %llu", &value) == 1) {
            metrics->syscalls_read = value;
            fields_found++;
            continue;
        }

        // syscw: número de syscalls de escrita
        if (sscanf(line, "syscw: %llu", &value) == 1) {
            metrics->syscalls_write = value;
            fields_found++;
            continue;
        }

        // read_bytes: bytes realmente lidos do disco
        if (sscanf(line, "read_bytes: %llu", &value) == 1) {
            metrics->bytes_read = value;
            fields_found++;
            continue;
        }

        // write_bytes: bytes realmente escritos no disco
        if (sscanf(line, "write_bytes: %llu", &value) == 1) {
            metrics->bytes_written = value;
            fields_found++;
            continue;
        }
    }
    fclose(fp);

    // Verificar se conseguimos ler os campos essenciais
    if (fields_found < 4) {
        fprintf(stderr, "Warning: Could not read all I/O fields (got %d)\n", fields_found);
    }

    // Calcular taxas (requer duas leituras para ter delta)
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    if (io_state.initialized) {
        // Calcular tempo decorrido em segundos
        double elapsed_time = (current_time.tv_sec - io_state.last_timestamp.tv_sec) +
                             (current_time.tv_nsec - io_state.last_timestamp.tv_nsec) / 1e9;

        if (elapsed_time > 0) {
            // Calcular deltas
            uint64_t delta_read = metrics->bytes_read - io_state.last_bytes_read;
            uint64_t delta_written = metrics->bytes_written - io_state.last_bytes_written;

            // Calcular taxas (bytes por segundo)
            metrics->read_rate = (double)delta_read / elapsed_time;
            metrics->write_rate = (double)delta_written / elapsed_time;
        } else {
            metrics->read_rate = 0.0;
            metrics->write_rate = 0.0;
        }
    } else {
        // Primeira leitura, não podemos calcular taxas
        metrics->read_rate = 0.0;
        metrics->write_rate = 0.0;
    }

    // Atualizar estado para próxima leitura
    io_state.last_bytes_read = metrics->bytes_read;
    io_state.last_bytes_written = metrics->bytes_written;
    io_state.last_timestamp = current_time;
    io_state.initialized = 1;

    return 0;
}

/**
 * Reseta o estado interno do monitor de I/O
 * Útil ao mudar de processo monitorado
 */
void reset_io_monitor(void) {
    memset(&io_state, 0, sizeof(io_state));
}

/**
 * Formata tamanho de dados para string legível (B, KB, MB, GB)
 */
static void format_data_size(uint64_t bytes, char *buffer, size_t size) {
    if (bytes < 1024) {
        snprintf(buffer, size, "%lu B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, size, "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        snprintf(buffer, size, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/**
 * Formata taxa de transferência para string legível (B/s, KB/s, MB/s)
 */
static void format_transfer_rate(double bytes_per_sec, char *buffer, size_t size) {
    if (bytes_per_sec < 0) {
        snprintf(buffer, size, "N/A");
    } else if (bytes_per_sec < 1024) {
        snprintf(buffer, size, "%.2f B/s", bytes_per_sec);
    } else if (bytes_per_sec < 1024 * 1024) {
        snprintf(buffer, size, "%.2f KB/s", bytes_per_sec / 1024.0);
    } else {
        snprintf(buffer, size, "%.2f MB/s", bytes_per_sec / (1024.0 * 1024.0));
    }
}

/**
 * Imprime métricas de I/O formatadas
 */
void print_io_metrics(const io_metrics_t *metrics) {
    if (metrics == NULL) {
        return;
    }

    char read_str[64], written_str[64];
    char read_rate_str[64], write_rate_str[64];

    format_data_size(metrics->bytes_read, read_str, sizeof(read_str));
    format_data_size(metrics->bytes_written, written_str, sizeof(written_str));
    format_transfer_rate(metrics->read_rate, read_rate_str, sizeof(read_rate_str));
    format_transfer_rate(metrics->write_rate, write_rate_str, sizeof(write_rate_str));

    printf("I/O Metrics:\n");
    printf("  Bytes Read:       %s (%lu bytes)\n", read_str, metrics->bytes_read);
    printf("  Bytes Written:    %s (%lu bytes)\n", written_str, metrics->bytes_written);
    printf("  Read Syscalls:    %lu\n", metrics->syscalls_read);
    printf("  Write Syscalls:   %lu\n", metrics->syscalls_write);
    printf("  Read Rate:        %s\n", read_rate_str);
    printf("  Write Rate:       %s\n", write_rate_str);
}

/**
 * Calcula throughput total (leitura + escrita)
 */
double get_total_io_throughput(const io_metrics_t *metrics) {
    if (metrics == NULL) {
        return 0.0;
    }
    return metrics->read_rate + metrics->write_rate;
}

/**
 * Calcula número médio de bytes por syscall
 */
void get_io_efficiency(const io_metrics_t *metrics, 
                       double *avg_read_size, 
                       double *avg_write_size) {
    if (metrics == NULL || avg_read_size == NULL || avg_write_size == NULL) {
        return;
    }

    if (metrics->syscalls_read > 0) {
        *avg_read_size = (double)metrics->bytes_read / (double)metrics->syscalls_read;
    } else {
        *avg_read_size = 0.0;
    }

    if (metrics->syscalls_write > 0) {
        *avg_write_size = (double)metrics->bytes_written / (double)metrics->syscalls_write;
    } else {
        *avg_write_size = 0.0;
    }
}

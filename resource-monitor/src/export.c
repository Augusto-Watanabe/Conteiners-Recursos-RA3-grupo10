#define _POSIX_C_SOURCE 200809L

#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/**
 * Exporta métricas para arquivo CSV
 */
int export_metrics_csv(const char *filename,
                       pid_t pid,
                       const cpu_metrics_t *cpu,
                       const memory_metrics_t *mem,
                       const io_metrics_t *io) {
    if (filename == NULL) {
        fprintf(stderr, "Error: filename is NULL\n");
        errno = EINVAL;
        return -1;
    }

    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", filename, strerror(errno));
        return -1;
    }

    // Verificar se arquivo está vazio (para escrever header)
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);

    if (size == 0) {
        // Escrever header
        fprintf(fp, "timestamp,pid,");
        fprintf(fp, "cpu_user_time,cpu_system_time,cpu_total_time,cpu_percent,num_threads,context_switches,");
        fprintf(fp, "mem_rss,mem_vsz,mem_swap,mem_page_faults,");
        fprintf(fp, "io_bytes_read,io_bytes_written,io_syscalls_read,io_syscalls_write,io_read_rate,io_write_rate\n");
    }

    // Obter timestamp
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Escrever dados
    fprintf(fp, "%s,%d,", timestamp, pid);

    // CPU
    if (cpu != NULL) {
        fprintf(fp, "%lu,%lu,%lu,%.2f,%u,%lu,",
                cpu->user_time, cpu->system_time, cpu->total_time,
                cpu->cpu_percent, cpu->num_threads, cpu->context_switches);
    } else {
        fprintf(fp, ",,,,,,");
    }

    // Memory
    if (mem != NULL) {
        fprintf(fp, "%lu,%lu,%lu,%lu,",
                mem->rss, mem->vsz, mem->swap, mem->page_faults);
    } else {
        fprintf(fp, ",,,,");
    }

    // I/O
    if (io != NULL) {
        fprintf(fp, "%lu,%lu,%lu,%lu,%.2f,%.2f",
                io->bytes_read, io->bytes_written,
                io->syscalls_read, io->syscalls_write,
                io->read_rate, io->write_rate);
    } else {
        fprintf(fp, ",,,,,,");
    }

    fprintf(fp, "\n");
    fclose(fp);

    return 0;
}

/**
 * Exporta métricas para arquivo JSON
 */
int export_metrics_json(const char *filename,
                        pid_t pid,
                        const cpu_metrics_t *cpu,
                        const memory_metrics_t *mem,
                        const io_metrics_t *io) {
    if (filename == NULL) {
        fprintf(stderr, "Error: filename is NULL\n");
        errno = EINVAL;
        return -1;
    }

    FILE *fp = fopen(filename, "a");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", filename, strerror(errno));
        return -1;
    }

    // Obter timestamp
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Escrever JSON
    fprintf(fp, "{\n");
    fprintf(fp, "  \"timestamp\": \"%s\",\n", timestamp);
    fprintf(fp, "  \"pid\": %d,\n", pid);

    // CPU
    fprintf(fp, "  \"cpu\": {\n");
    if (cpu != NULL) {
        fprintf(fp, "    \"user_time\": %lu,\n", cpu->user_time);
        fprintf(fp, "    \"system_time\": %lu,\n", cpu->system_time);
        fprintf(fp, "    \"total_time\": %lu,\n", cpu->total_time);
        fprintf(fp, "    \"cpu_percent\": %.2f,\n", cpu->cpu_percent);
        fprintf(fp, "    \"num_threads\": %u,\n", cpu->num_threads);
        fprintf(fp, "    \"context_switches\": %lu\n", cpu->context_switches);
    } else {
        fprintf(fp, "    \"error\": \"not collected\"\n");
    }
    fprintf(fp, "  },\n");

    // Memory
    fprintf(fp, "  \"memory\": {\n");
    if (mem != NULL) {
        fprintf(fp, "    \"rss\": %lu,\n", mem->rss);
        fprintf(fp, "    \"vsz\": %lu,\n", mem->vsz);
        fprintf(fp, "    \"swap\": %lu,\n", mem->swap);
        fprintf(fp, "    \"page_faults\": %lu\n", mem->page_faults);
    } else {
        fprintf(fp, "    \"error\": \"not collected\"\n");
    }
    fprintf(fp, "  },\n");

    // I/O
    fprintf(fp, "  \"io\": {\n");
    if (io != NULL) {
        fprintf(fp, "    \"bytes_read\": %lu,\n", io->bytes_read);
        fprintf(fp, "    \"bytes_written\": %lu,\n", io->bytes_written);
        fprintf(fp, "    \"syscalls_read\": %lu,\n", io->syscalls_read);
        fprintf(fp, "    \"syscalls_write\": %lu,\n", io->syscalls_write);
        fprintf(fp, "    \"read_rate\": %.2f,\n", io->read_rate);
        fprintf(fp, "    \"write_rate\": %.2f\n", io->write_rate);
    } else {
        fprintf(fp, "    \"error\": \"not collected\"\n");
    }
    fprintf(fp, "  }\n");

    fprintf(fp, "}\n");
    fclose(fp);

    return 0;
}

/**
 * Imprime resumo das métricas no terminal
 */
void print_metrics_summary(pid_t pid,
                          const cpu_metrics_t *cpu,
                          const memory_metrics_t *mem,
                          const io_metrics_t *io) {
    printf("PID: %d\n", pid);

    if (cpu != NULL) {
        printf("  CPU: %.2f%% | Threads: %u\n",
               cpu->cpu_percent, cpu->num_threads);
    }

    if (mem != NULL) {
        printf("  MEM: %.2f MB (RSS) | %.2f MB (VSZ)\n",
               mem->rss / (1024.0 * 1024.0),
               mem->vsz / (1024.0 * 1024.0));
    }

    if (io != NULL) {
        printf("  I/O: R: %.2f KB/s | W: %.2f KB/s\n",
               io->read_rate / 1024.0,
               io->write_rate / 1024.0);
    }
}

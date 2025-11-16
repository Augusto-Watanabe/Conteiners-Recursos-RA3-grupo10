#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "monitor.h"

// Flag para controlar loop de monitoramento
static volatile int keep_running = 1;

// Handler para SIGINT (Ctrl+C)
void sigint_handler(int sig) {
    (void)sig;
    keep_running = 0;
    printf("\n\nStopping monitoring...\n");
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <PID>\n\n", program_name);
    printf("Options:\n");
    printf("  -i, --interval <seconds>  Monitoring interval (default: 1)\n");
    printf("  -c, --count <n>          Number of samples (default: infinite)\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s 1234                  Monitor process 1234\n", program_name);
    printf("  %s -i 2 -c 10 1234       Monitor every 2s for 10 samples\n", program_name);
    printf("  %s self                  Monitor this process\n", program_name);
}

int main(int argc, char *argv[]) {
    // Parâmetros padrão
    pid_t target_pid = 0;
    int interval = 1;
    int count = -1; // -1 = infinito
    
    // Parsear argumentos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interval") == 0) {
            if (i + 1 < argc) {
                interval = atoi(argv[++i]);
                if (interval <= 0) {
                    fprintf(stderr, "Error: interval must be positive\n");
                    return EXIT_FAILURE;
                }
            } else {
                fprintf(stderr, "Error: -i requires an argument\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--count") == 0) {
            if (i + 1 < argc) {
                count = atoi(argv[++i]);
                if (count <= 0) {
                    fprintf(stderr, "Error: count must be positive\n");
                    return EXIT_FAILURE;
                }
            } else {
                fprintf(stderr, "Error: -c requires an argument\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "self") == 0) {
            target_pid = getpid();
        } else {
            // Assumir que é o PID
            target_pid = atoi(argv[i]);
            if (target_pid <= 0) {
                fprintf(stderr, "Error: invalid PID '%s'\n", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    // Validar PID
    if (target_pid == 0) {
        fprintf(stderr, "Error: no PID specified\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Verificar se processo existe
    if (!process_exists(target_pid)) {
        fprintf(stderr, "Error: process %d does not exist\n", target_pid);
        return EXIT_FAILURE;
    }

    // Obter nome do processo
    char process_name[256];
    if (get_process_name(target_pid, process_name, sizeof(process_name)) == 0) {
        printf("Monitoring process: %s (PID: %d)\n", process_name, target_pid);
    } else {
        printf("Monitoring PID: %d\n", target_pid);
    }
    
    printf("Interval: %d second(s)\n", interval);
    if (count > 0) {
        printf("Samples: %d\n", count);
    } else {
        printf("Samples: infinite (press Ctrl+C to stop)\n");
    }
    printf("\n");

    // Configurar handler para Ctrl+C
    signal(SIGINT, sigint_handler);

    // Estruturas para métricas
    cpu_metrics_t cpu_metrics;
    memory_metrics_t mem_metrics;

    // Loop de monitoramento
    int samples = 0;
    while (keep_running && (count < 0 || samples < count)) {
        printf("=== Sample %d ===\n", samples + 1);

        // Verificar se processo ainda existe
        if (!process_exists(target_pid)) {
            printf("\nProcess terminated.\n");
            break;
        }

        // Coletar CPU
        if (collect_cpu_metrics(target_pid, &cpu_metrics) == 0) {
            print_cpu_metrics(&cpu_metrics);
        } else {
            fprintf(stderr, "Warning: failed to collect CPU metrics\n");
        }

        printf("\n");

        // Coletar Memória
        if (collect_memory_metrics(target_pid, &mem_metrics) == 0) {
            print_memory_metrics(&mem_metrics);
            
            double mem_percent = get_memory_usage_percent(&mem_metrics);
            if (mem_percent >= 0) {
                printf("  System Usage:     %.2f%%\n", mem_percent);
            }

            double leak_rate = detect_memory_leak(&mem_metrics);
            if (samples > 0) { // Só mostra após primeira iteração
                printf("  Growth Rate:      %.2f KB/s", leak_rate / 1024.0);
                if (leak_rate > 1024 * 10) { // Mais de 10 KB/s
                    printf(" ⚠️  Possible memory leak!");
                }
                printf("\n");
            }
        } else {
            fprintf(stderr, "Warning: failed to collect memory metrics\n");
        }

        printf("\n");

        samples++;

        // Aguardar próximo intervalo (se não for última amostra)
        if (count < 0 || samples < count) {
            sleep(interval);
        }
    }

    printf("\nMonitoring stopped. Total samples: %d\n", samples);

    return EXIT_SUCCESS;
}

// ============================================================================
// Implementação das funções utilitárias
// ============================================================================

int process_exists(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    return (access(path, F_OK) == 0) ? 1 : 0;
}

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

    // Remover newline
    size_t len = strlen(name);
    if (len > 0 && name[len - 1] == '\n') {
        name[len - 1] = '\0';
    }

    fclose(fp);
    return 0;
}
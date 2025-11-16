#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include "monitor.h"

static volatile int keep_running = 1;

void sigint_handler(int sig) {
    (void)sig;
    keep_running = 0;
    printf("\n\nStopping monitoring...\n");
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] <PID>\n\n", program_name);
    printf("Monitor system resources of a process\n\n");
    
    printf("Options:\n");
    printf("  -i, --interval <sec>   Monitoring interval in seconds (default: 1)\n");
    printf("  -c, --count <n>        Number of samples to collect (default: infinite)\n");
    printf("  -m, --mode <mode>      Monitoring mode: all, cpu, mem, io (default: all)\n");
    printf("  -o, --output <file>    Export data to file\n");
    printf("  -f, --format <fmt>     Export format: csv, json (default: csv)\n");
    printf("  -q, --quiet            Quiet mode (no terminal output)\n");
    printf("  -s, --summary          Summary mode (compact output)\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -v, --version          Show version information\n\n");
    
    printf("Arguments:\n");
    printf("  <PID>                  Process ID to monitor\n");
    printf("  self                   Monitor this process\n\n");
    
    printf("Examples:\n");
    printf("  %s 1234                          Monitor process 1234\n", program_name);
    printf("  %s -i 2 -c 10 1234               Monitor every 2s for 10 samples\n", program_name);
    printf("  %s -m io 1234                    Monitor only I/O metrics\n", program_name);
    printf("  %s -o data.csv 1234              Export to CSV file\n", program_name);
    printf("  %s -o data.json -f json 1234     Export to JSON file\n", program_name);
    printf("  %s -s -c 60 self                 Summary mode, 60 samples\n", program_name);
    printf("\n");
}

void print_version(void) {
    printf("Resource Monitor v1.0\n");
    printf("Compiled on %s %s\n", __DATE__, __TIME__);
}

int main(int argc, char *argv[]) {
    // Parâmetros padrão
    pid_t target_pid = 0;
    int interval = 1;
    int count = -1;
    char mode[16] = "all";
    char output_file[256] = "";
    char format[16] = "csv";
    int quiet = 0;
    int summary = 0;

    // Opções longas
    static struct option long_options[] = {
        {"interval", required_argument, 0, 'i'},
        {"count",    required_argument, 0, 'c'},
        {"mode",     required_argument, 0, 'm'},
        {"output",   required_argument, 0, 'o'},
        {"format",   required_argument, 0, 'f'},
        {"quiet",    no_argument,       0, 'q'},
        {"summary",  no_argument,       0, 's'},
        {"help",     no_argument,       0, 'h'},
        {"version",  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    // Parsear argumentos
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "i:c:m:o:f:qshv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'i':
                interval = atoi(optarg);
                if (interval <= 0) {
                    fprintf(stderr, "Error: interval must be positive\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'c':
                count = atoi(optarg);
                if (count <= 0) {
                    fprintf(stderr, "Error: count must be positive\n");
                    return EXIT_FAILURE;
                }
                break;
            case 'm':
                strncpy(mode, optarg, sizeof(mode) - 1);
                mode[sizeof(mode) - 1] = '\0';
                if (strcmp(mode, "all") != 0 && strcmp(mode, "cpu") != 0 &&
                    strcmp(mode, "mem") != 0 && strcmp(mode, "io") != 0) {
                    fprintf(stderr, "Error: invalid mode '%s'\n", mode);
                    return EXIT_FAILURE;
                }
                break;
            case 'o':
                strncpy(output_file, optarg, sizeof(output_file) - 1);
                output_file[sizeof(output_file) - 1] = '\0';
                break;
            case 'f':
                strncpy(format, optarg, sizeof(format) - 1);
                format[sizeof(format) - 1] = '\0';
                if (strcmp(format, "csv") != 0 && strcmp(format, "json") != 0) {
                    fprintf(stderr, "Error: invalid format '%s'\n", format);
                    return EXIT_FAILURE;
                }
                break;
            case 'q':
                quiet = 1;
                break;
            case 's':
                summary = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            case 'v':
                print_version();
                return EXIT_SUCCESS;
            case '?':
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            default:
                abort();
        }
    }

    // Obter PID dos argumentos restantes
    if (optind < argc) {
        if (strcmp(argv[optind], "self") == 0) {
            target_pid = getpid();
        } else {
            target_pid = atoi(argv[optind]);
            if (target_pid <= 0) {
                fprintf(stderr, "Error: invalid PID '%s'\n", argv[optind]);
                return EXIT_FAILURE;
            }
        }
    } else {
        fprintf(stderr, "Error: no PID specified\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Verificar se processo existe
    if (!process_exists(target_pid)) {
        fprintf(stderr, "Error: process %d does not exist\n", target_pid);
        fprintf(stderr, "Tip: Use 'ps aux | grep <name>' to find process IDs\n");
        return EXIT_FAILURE;
    }

    // Obter nome do processo
    char process_name[256];
    if (get_process_name(target_pid, process_name, sizeof(process_name)) != 0) {
        snprintf(process_name, sizeof(process_name), "unknown");
    }

    // Header de informações
    if (!quiet) {
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║            Resource Monitor - Process Profiler             ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n");
        printf("\n");
        printf("Target Process: %s (PID: %d)\n", process_name, target_pid);
        printf("Monitoring Mode: %s\n", mode);
        printf("Sample Interval: %d second(s)\n", interval);
        
        if (count > 0) {
            printf("Total Samples: %d\n", count);
        } else {
            printf("Total Samples: infinite (press Ctrl+C to stop)\n");
        }
        
        if (strlen(output_file) > 0) {
            printf("Export File: %s (format: %s)\n", output_file, format);
        }
        
        printf("\n");
    }

    // Configurar handler para Ctrl+C
    signal(SIGINT, sigint_handler);

    // Estruturas para métricas
    cpu_metrics_t cpu_metrics;
    memory_metrics_t mem_metrics;
    io_metrics_t io_metrics;

    // Determinar o que monitorar
    int monitor_cpu = (strcmp(mode, "all") == 0 || strcmp(mode, "cpu") == 0);
    int monitor_mem = (strcmp(mode, "all") == 0 || strcmp(mode, "mem") == 0);
    int monitor_io = (strcmp(mode, "all") == 0 || strcmp(mode, "io") == 0);

    // Variáveis para controle
    int samples = 0;
    int errors = 0;
    int io_permission_warned = 0;

    // Loop de monitoramento
    while (keep_running && (count < 0 || samples < count)) {
        // Verificar se processo ainda existe
        if (!process_exists(target_pid)) {
            if (!quiet) {
                printf("\n⚠️  Process terminated after %d samples.\n", samples);
            }
            break;
        }

        // Ponteiros para métricas (NULL se não coletadas)
        cpu_metrics_t *cpu_ptr = NULL;
        memory_metrics_t *mem_ptr = NULL;
        io_metrics_t *io_ptr = NULL;

        // Coletar CPU
        if (monitor_cpu) {
            if (collect_cpu_metrics(target_pid, &cpu_metrics) == 0) {
                cpu_ptr = &cpu_metrics;
                if (!quiet && !summary) {
                    if (samples > 0) printf("\n");
                    printf("=== Sample %d ===\n", samples + 1);
                    print_cpu_metrics(&cpu_metrics);
                }
            } else {
                errors++;
                if (!quiet) {
                    fprintf(stderr, "Warning: failed to collect CPU metrics\n");
                }
            }
        }

        // Coletar Memória
        if (monitor_mem) {
            if (collect_memory_metrics(target_pid, &mem_metrics) == 0) {
                mem_ptr = &mem_metrics;
                if (!quiet && !summary) {
                    printf("\n");
                    print_memory_metrics(&mem_metrics);
                    double mem_percent = get_memory_usage_percent(&mem_metrics);
                    if (mem_percent >= 0) {
                        printf("  System Usage:     %.2f%%\n", mem_percent);
                    }
                }
            } else {
                errors++;
                if (!quiet) {
                    fprintf(stderr, "Warning: failed to collect memory metrics\n");
                }
            }
        }

        // Coletar I/O
        if (monitor_io) {
            if (collect_io_metrics(target_pid, &io_metrics) == 0) {
                io_ptr = &io_metrics;
                if (!quiet && !summary) {
                    printf("\n");
                    print_io_metrics(&io_metrics);
                }
            } else {
                if (!io_permission_warned && !quiet) {
                    fprintf(stderr, "\n⚠️  Warning: I/O monitoring requires root permissions (sudo)\n");
                    fprintf(stderr, "   I/O metrics will not be collected.\n\n");
                    io_permission_warned = 1;
                }
                errors++;
            }
        }

        // Modo summary
        if (!quiet && summary && samples > 0) {
            if (samples % 10 == 0) {
                printf("\n");
            }
            print_metrics_summary(target_pid, cpu_ptr, mem_ptr, io_ptr);
        }

        // Exportar dados
        if (strlen(output_file) > 0) {
            if (strcmp(format, "csv") == 0) {
                if (export_metrics_csv(output_file, target_pid, cpu_ptr, mem_ptr, io_ptr) != 0) {
                    fprintf(stderr, "Error: failed to export to CSV\n");
                }
            } else if (strcmp(format, "json") == 0) {
                if (export_metrics_json(output_file, target_pid, cpu_ptr, mem_ptr, io_ptr) != 0) {
                    fprintf(stderr, "Error: failed to export to JSON\n");
                }
            }
        }

        samples++;

        // Aguardar próximo intervalo (se não for última amostra)
        if (count < 0 || samples < count) {
            sleep(interval);
        }
    }

    // Relatório final
    if (!quiet) {
        printf("\n");
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║                    Monitoring Summary                      ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n");
        printf("Total Samples Collected: %d\n", samples);
        printf("Errors Encountered: %d\n", errors);
        
        if (strlen(output_file) > 0) {
            printf("Data exported to: %s\n", output_file);
        }
        
        printf("\n✓ Monitoring completed successfully.\n");
    }

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

    size_t len = strlen(name);
    if (len > 0 && name[len - 1] == '\n') {
        name[len - 1] = '\0';
    }

    fclose(fp);
    return 0;
}

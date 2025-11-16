#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "monitor.h"

static volatile int keep_running = 1;

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
    printf("  -m, --mode <mode>        Monitoring mode: all, cpu, mem, io (default: all)\n");
    printf("  -h, --help               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s 1234                  Monitor process 1234\n", program_name);
    printf("  %s -i 2 -c 10 1234       Monitor every 2s for 10 samples\n", program_name);
    printf("  %s -m io 1234            Monitor only I/O metrics\n", program_name);
    printf("  %s self                  Monitor this process\n", program_name);
}

int main(int argc, char *argv[]) {
    pid_t target_pid = 0;
    int interval = 1;
    int count = -1;
    char mode[16] = "all";
    
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
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--count") == 0) {
            if (i + 1 < argc) {
                count = atoi(argv[++i]);
                if (count <= 0) {
                    fprintf(stderr, "Error: count must be positive\n");
                    return EXIT_FAILURE;
                }
            }
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) {
                strncpy(mode, argv[++i], sizeof(mode) - 1);
                mode[sizeof(mode) - 1] = '\0';
            }
        } else if (strcmp(argv[i], "self") == 0) {
            target_pid = getpid();
        } else {
            target_pid = atoi(argv[i]);
            if (target_pid <= 0) {
                fprintf(stderr, "Error: invalid PID '%s'\n", argv[i]);
                return EXIT_FAILURE;
            }
        }
    }

    if (target_pid == 0) {
        fprintf(stderr, "Error: no PID specified\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (!process_exists(target_pid)) {
        fprintf(stderr, "Error: process %d does not exist\n", target_pid);
        return EXIT_FAILURE;
    }

    char process_name[256];
    if (get_process_name(target_pid, process_name, sizeof(process_name)) == 0) {
        printf("Monitoring process: %s (PID: %d)\n", process_name, target_pid);
    } else {
        printf("Monitoring PID: %d\n", target_pid);
    }
    
    printf("Mode: %s | Interval: %d second(s)\n", mode, interval);
    if (count > 0) {
        printf("Samples: %d\n", count);
    } else {
        printf("Samples: infinite (press Ctrl+C to stop)\n");
    }
    printf("\n");

    signal(SIGINT, sigint_handler);

    cpu_metrics_t cpu_metrics;
    memory_metrics_t mem_metrics;
    io_metrics_t io_metrics;

    int monitor_cpu = (strcmp(mode, "all") == 0 || strcmp(mode, "cpu") == 0);
    int monitor_mem = (strcmp(mode, "all") == 0 || strcmp(mode, "mem") == 0);
    int monitor_io = (strcmp(mode, "all") == 0 || strcmp(mode, "io") == 0);

    int samples = 0;
    while (keep_running && (count < 0 || samples < count)) {
        printf("=== Sample %d ===\n", samples + 1);

        if (!process_exists(target_pid)) {
            printf("\nProcess terminated.\n");
            break;
        }

        if (monitor_cpu) {
            if (collect_cpu_metrics(target_pid, &cpu_metrics) == 0) {
                print_cpu_metrics(&cpu_metrics);
                printf("\n");
            }
        }

        if (monitor_mem) {
            if (collect_memory_metrics(target_pid, &mem_metrics) == 0) {
                print_memory_metrics(&mem_metrics);
                double mem_percent = get_memory_usage_percent(&mem_metrics);
                if (mem_percent >= 0) {
                    printf("  System Usage:     %.2f%%\n", mem_percent);
                }
                printf("\n");
            }
        }

        if (monitor_io) {
            if (collect_io_metrics(target_pid, &io_metrics) == 0) {
                print_io_metrics(&io_metrics);
                printf("\n");
            } else if (samples == 0) {
                fprintf(stderr, "Warning: I/O monitoring requires root permissions\n\n");
            }
        }

        samples++;

        if (count < 0 || samples < count) {
            sleep(interval);
        }
    }

    printf("\nMonitoring stopped. Total samples: %d\n", samples);
    return EXIT_SUCCESS;
}

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

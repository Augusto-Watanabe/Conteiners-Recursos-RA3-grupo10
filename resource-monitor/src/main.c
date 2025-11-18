#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <getopt.h>
#include "monitor.h"
#include "cgroup.h"
#include "namespace.h"

static volatile int keep_running = 1;

void sigint_handler(int sig) {
    (void)sig;
    keep_running = 0;
    printf("\n\nStopping monitoring...\n");
}

void print_usage(const char *program_name) {
    printf("Resource Monitor & Cgroup Manager\n\n");
    
    printf("Usage (Monitoring Mode):\n");
    printf("  %s [OPTIONS] <PID | self>\n\n", program_name);
    
    printf("Usage (Execution Mode):\n");
    printf("  %s [CGROUP_OPTIONS] -- <command> [args...]\n\n", program_name);
    
    printf("Monitoring Options:\n");
    printf("  -i, --interval <sec>   Monitoring interval in seconds (default: 1)\n");
    printf("  -c, --count <n>        Number of samples to collect (default: infinite)\n");
    printf("  -m, --mode <mode>      Monitoring mode: all, cpu, mem, io (default: all)\n");
    printf("  -o, --output <file>    Export data to file\n");
    printf("  -f, --format <fmt>     Export format: csv, json (default: csv)\n");
    printf("  -q, --quiet            Quiet mode (no terminal output)\n");
    printf("  -s, --summary          Show a compact summary instead of detailed reports\n");
    printf("  -N, --namespace        Show namespace information before monitoring\n");
    printf("  -C, --compare <pid2>   Compare namespaces with another PID and exit\n");
    printf("\n");
    
    printf("Cgroup Execution Options:\n");
    printf("      --cgroup-name <name> Name for the new cgroup (default: monitor_cgroup_XXXX)\n");
    printf("      --cpu-limit <cores>  CPU limit in cores (e.g., 0.5, 1.0)\n");
    printf("      --mem-limit <MB>     Memory limit in Megabytes (e.g., 512)\n");
    printf("\n");
    
    printf("General Options:\n");
    printf("  -h, --help             Show this help message\n");
    printf("  -v, --version          Show version information\n\n");
    
    printf("Examples:\n");
    printf("  %s 1234                                Monitor process 1234\n", program_name);
    printf("  %s -N 1                                Show namespace info for init process\n", program_name);
    printf("  %s -C 5678 1234                      Compare namespaces of two processes\n", program_name);
    printf("  %s --cpu-limit 0.5 -- ./my_app        Run './my_app' with a 0.5 CPU core limit\n", program_name);
    printf("  %s --mem-limit 256 -- stress -m 1      Run 'stress' with a 256MB memory limit\n", program_name);
    printf("\n");
}

void print_version(void) {
    printf("Resource Monitor v1.0\n");
    printf("With Namespace Analysis Support\n");
    printf("Compiled on %s %s\n", __DATE__, __TIME__);
}

int run_command_in_cgroup(int argc, char *argv[], const char* cgroup_name, double cpu_limit, uint64_t mem_limit_mb) {
    if (geteuid() != 0) {
        fprintf(stderr, "Error: Cgroup execution mode requires root privileges (sudo).\n");
        return EXIT_FAILURE;
    }

    int version = detect_cgroup_version();
    if (version < 0) {
        fprintf(stderr, "Error: Could not detect cgroup version.\n");
        return EXIT_FAILURE;
    }

    char final_cgroup_name[256];
    if (cgroup_name == NULL) {
        snprintf(final_cgroup_name, sizeof(final_cgroup_name), "monitor_cgroup_%d", getpid());
    } else {
        strncpy(final_cgroup_name, cgroup_name, sizeof(final_cgroup_name) - 1);
    }

    printf("Setting up cgroup '%s' (v%d)...\n", final_cgroup_name, version);

    // Create cgroups (CPU and Memory for v1)
    char cpu_cgroup_path[PATH_MAX], mem_cgroup_path[PATH_MAX];
    if (create_cgroup_for_controllers(final_cgroup_name, cpu_cgroup_path, sizeof(cpu_cgroup_path), mem_cgroup_path, sizeof(mem_cgroup_path)) != 0) {
        fprintf(stderr, "Error creating cgroup: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    printf("✓ Cgroup created.\n");

    // Apply limits
    if (cpu_limit > 0) {
        if (set_cgroup_cpu_limit(cpu_cgroup_path, cpu_limit) == 0) {
            printf("✓ CPU limit set to %.2f cores.\n", cpu_limit);
        } else {
            fprintf(stderr, "Error setting CPU limit: %s\n", strerror(errno));
        }
    }
    if (mem_limit_mb > 0) {
        if (set_cgroup_memory_limit(mem_cgroup_path, mem_limit_mb * 1024 * 1024) == 0) {
            printf("✓ Memory limit set to %lu MB.\n", mem_limit_mb);
        } else {
            fprintf(stderr, "Error setting memory limit: %s\n", strerror(errno));
        }
    }

    printf("\n--- Running Command: ");
    for (int i = 0; i < argc; i++) printf("%s ", argv[i]);
    printf("---\n\n");

    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (child_pid == 0) { // Child process
        // Move self to the cgroup
        if (move_process_to_cgroup(getpid(), cpu_cgroup_path) != 0 || move_process_to_cgroup(getpid(), mem_cgroup_path) != 0) {
            perror("Failed to move child to cgroup");
            exit(EXIT_FAILURE);
        }
        execvp(argv[0], argv);
        // execvp only returns on error
        fprintf(stderr, "Error executing command '%s': %s\n", argv[0], strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Parent process
    waitpid(child_pid, NULL, 0);

    printf("\n--- Command Finished. Cgroup Usage Report ---\n");
    cgroup_metrics_t final_metrics;
    if (read_cgroup_metrics_from_path(cpu_cgroup_path, mem_cgroup_path, &final_metrics) == 0) {
        print_cgroup_metrics(&final_metrics);
    }

    printf("--- Cleaning up cgroups ---\n");
    cleanup_cgroup(final_cgroup_name);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    pid_t target_pid = 0;
    int interval = 1;
    int count = -1;
    char mode[16] = "all";
    char output_file[256] = "";
    char format[16] = "csv";
    int quiet = 0;
    int summary = 0;
    int show_namespace = 0;
    pid_t compare_pid = 0;
    
    // Cgroup execution mode options
    char *cgroup_name = NULL;
    double cpu_limit = 0.0;
    uint64_t mem_limit_mb = 0;

    static struct option long_options[] = {
        {"interval",  required_argument, 0, 'i'},
        {"count",     required_argument, 0, 'c'},
        {"mode",      required_argument, 0, 'm'},
        {"output",    required_argument, 0, 'o'},
        {"format",    required_argument, 0, 'f'},
        {"quiet",     no_argument,       0, 'q'},
        {"summary",   no_argument,       0, 's'},
        {"namespace", no_argument,       0, 'N'},
        {"compare",   required_argument, 0, 'C'},
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'v'},
        // Cgroup options
        {"cgroup-name", required_argument, 0, 256},
        {"cpu-limit",   required_argument, 0, 257},
        {"mem-limit",   required_argument, 0, 258},
        {0, 0, 0, 0}
    };

    // --- Argument Parsing ---
    // Find "--" to separate modes
    int double_dash_index = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            double_dash_index = i;
            break;
        }
    }

    int argc_opts = (double_dash_index != -1) ? double_dash_index : argc;

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc_opts, argv, "i:c:m:o:f:qsNC:hv", long_options, &option_index)) != -1) {
        switch (opt) {
            // Monitoring options
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
            case 'N':
                show_namespace = 1;
                break;
            case 'C':
                compare_pid = atoi(optarg);
                if (compare_pid <= 0) {
                    fprintf(stderr, "Error: invalid PID '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            
            // Cgroup execution options (long only)
            case 256: // --cgroup-name
                cgroup_name = optarg;
                break;
            case 257: // --cpu-limit
                cpu_limit = atof(optarg);
                if (cpu_limit <= 0) {
                    fprintf(stderr, "Error: CPU limit must be positive.\n");
                    return EXIT_FAILURE;
                }
                break;
            case 258: // --mem-limit
                mem_limit_mb = atoll(optarg);
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

    // --- Mode Dispatch ---
    if (double_dash_index != -1) {
        // Execution Mode
        // Check if monitoring-specific arguments were passed erroneously
        if (optind < double_dash_index) {
            fprintf(stderr, "Error: Monitoring arguments (like PIDs) cannot be mixed with execution mode (--).\n");
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        return run_command_in_cgroup(argc - double_dash_index - 1, &argv[double_dash_index + 1], cgroup_name, cpu_limit, mem_limit_mb);
    } else {
        // Monitoring Mode
        if (optind >= argc) {
            fprintf(stderr, "Error: no PID specified for monitoring mode.\n");
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        if (strcmp(argv[optind], "self") == 0) {
            target_pid = getpid();
        } else {
            target_pid = atoi(argv[optind]);
            if (target_pid <= 0) {
                fprintf(stderr, "Error: invalid PID '%s'\n", argv[optind]);
                return EXIT_FAILURE;
            }
        }

        // Verificar se processo existe
        if (!process_exists(target_pid)) {
            fprintf(stderr, "Error: process %d does not exist\n", target_pid);
            fprintf(stderr, "Tip: Use 'ps aux | grep <name>' to find process IDs\n");
            return EXIT_FAILURE;
        }

        // Modo comparação de namespaces
        if (compare_pid > 0) {
            if (!process_exists(compare_pid)) {
                fprintf(stderr, "Error: process %d does not exist\n", compare_pid);
                return EXIT_FAILURE;
            }
            
            namespace_comparison_t comparisons[MAX_NAMESPACES];
            int comp_count;
            
            if (compare_process_namespaces(target_pid, compare_pid, comparisons, &comp_count) == 0) {
                print_namespace_comparison(target_pid, compare_pid, comparisons, comp_count);
            } else {
                fprintf(stderr, "Error comparing namespaces\n");
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        // Se só quer ver namespace, mostrar e sair
        if (show_namespace && count < 0) {
            process_namespaces_t ns_info;
            if (list_process_namespaces(target_pid, &ns_info) == 0) {
                print_process_namespaces(&ns_info);
            } else {
                fprintf(stderr, "Error listing namespaces\n");
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }

        // Obter nome do processo
        char process_name[256];
        if (get_process_name(target_pid, process_name, sizeof(process_name)) != 0) {
            snprintf(process_name, sizeof(process_name), "unknown");
        }

        // Header
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
            
            // Mostrar namespace info no início se solicitado
            if (show_namespace) {
                printf("\n");
                process_namespaces_t ns_info;
                if (list_process_namespaces(target_pid, &ns_info) == 0) {
                    print_process_namespaces(&ns_info);
                }
            }
            
            printf("\n");
        }

        signal(SIGINT, sigint_handler);

        cpu_metrics_t cpu_metrics;
        memory_metrics_t mem_metrics;
        io_metrics_t io_metrics;

        int monitor_cpu = (strcmp(mode, "all") == 0 || strcmp(mode, "cpu") == 0);
        int monitor_mem = (strcmp(mode, "all") == 0 || strcmp(mode, "mem") == 0);
        int monitor_io = (strcmp(mode, "all") == 0 || strcmp(mode, "io") == 0);

        int samples = 0;
        int errors = 0;
        int io_permission_warned = 0;

        while (keep_running && (count < 0 || samples < count)) {
            if (!process_exists(target_pid)) {
                if (!quiet) {
                    printf("\n⚠️  Process terminated after %d samples.\n", samples);
                }
                break;
            }

            cpu_metrics_t *cpu_ptr = NULL;
            memory_metrics_t *mem_ptr = NULL;
            io_metrics_t *io_ptr = NULL;

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
                }
            }

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
                }
            }

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

            if (!quiet && summary && samples > 0) {
                if (samples % 10 == 0) {
                    printf("\n");
                }
                print_metrics_summary(target_pid, cpu_ptr, mem_ptr, io_ptr);
            }

            if (strlen(output_file) > 0) {
                if (strcmp(format, "csv") == 0) {
                    export_metrics_csv(output_file, target_pid, cpu_ptr, mem_ptr, io_ptr);
                } else if (strcmp(format, "json") == 0) {
                    export_metrics_json(output_file, target_pid, cpu_ptr, mem_ptr, io_ptr);
                }
            }

            samples++;

            if (count < 0 || samples < count) {
                sleep(interval);
            }
        }

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
}

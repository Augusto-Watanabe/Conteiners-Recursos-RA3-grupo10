#define _GNU_SOURCE
#include "cgroup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>

// Nomes dos controladores
static const char* controller_names[CGROUP_CONTROLLER_COUNT] = {
    "cpu",
    "memory",
    "blkio",
    "pids",
    "cpuset",
    "io"
};

/**
 * Converte controlador para string
 */
const char* cgroup_controller_to_string(cgroup_controller_t controller) {
    if (controller >= 0 && controller < CGROUP_CONTROLLER_COUNT) {
        return controller_names[controller];
    }
    return "unknown";
}

/**
 * Detecta versão de cgroup
 */
int detect_cgroup_version(void) {
    struct stat st;
    
    // Se /sys/fs/cgroup/cgroup.controllers existe, é v2
    if (stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0) {
        return 2;
    }
    
    // Se /sys/fs/cgroup/cpu existe, é v1
    if (stat("/sys/fs/cgroup/cpu", &st) == 0) {
        return 1;
    }
    
    return -1;
}

/**
 * Lê um valor de um arquivo de cgroup
 */
static int read_cgroup_file_uint64(const char *path, uint64_t *value) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    
    int ret = fscanf(fp, "%lu", value);
    fclose(fp);
    
    return (ret == 1) ? 0 : -1;
}

/**
 * Lê um valor int64 de um arquivo
 */
static int read_cgroup_file_int64(const char *path, int64_t *value) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    
    int ret = fscanf(fp, "%ld", value);
    fclose(fp);
    
    return (ret == 1) ? 0 : -1;
}

/**
 * Obtém caminho do cgroup de um processo
 */
int get_process_cgroup_path(pid_t pid, const char *controller,
                            char *path, size_t size) {
    if (path == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/cgroup", pid);
    
    FILE *fp = fopen(proc_path, "r");
    if (fp == NULL) {
        return -1;
    }
    
    char line[1024];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = 0;
        
        char *colon1 = strchr(line, ':');
        if (colon1 == NULL) continue;
        
        char *colon2 = strchr(colon1 + 1, ':');
        if (colon2 == NULL) continue;
        
        int hierarchy = atoi(line);
        char *cgroup_relative_path = colon2 + 1;
        
        if (controller == NULL && hierarchy == 0) {
            if (strlen(cgroup_relative_path) == 0 || strcmp(cgroup_relative_path, "/") == 0) {
                snprintf(path, size, "/sys/fs/cgroup");
            } else {
                snprintf(path, size, "/sys/fs/cgroup%s", cgroup_relative_path);
            }
            found = 1;
            break;
        }
        
        if (controller != NULL) {
            char controllers[256];
            size_t len = colon2 - (colon1 + 1);
            if (len >= sizeof(controllers)) len = sizeof(controllers) - 1;
            strncpy(controllers, colon1 + 1, len);
            controllers[len] = '\0';
            
            if (strstr(controllers, controller) != NULL) {
                snprintf(path, size, "/sys/fs/cgroup/%s%s", controller, cgroup_relative_path);
                found = 1;
                break;
            }
        }
    }
    
    fclose(fp);
    return found ? 0 : -1;
}

/**
 * Lê métricas de CPU
 */
int read_cgroup_cpu_metrics(const char *cgroup_path, cgroup_cpu_metrics_t *metrics) {
    if (cgroup_path == NULL || metrics == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(metrics, 0, sizeof(cgroup_cpu_metrics_t));
    
    int version = detect_cgroup_version();
    char file_path[PATH_MAX];
    
    if (version == 2) {
        // cgroup v2
        snprintf(file_path, sizeof(file_path), "%s/cpu.stat", cgroup_path);
        
        FILE *fp = fopen(file_path, "r");
        if (fp == NULL) {
            return -1;
        }
        
        char line[256];
        while (fgets(line, sizeof(line), fp) != NULL) {
            uint64_t value;
            
            if (sscanf(line, "usage_usec %lu", &value) == 1) {
                metrics->usage_usec = value;
            } else if (sscanf(line, "user_usec %lu", &value) == 1) {
                metrics->user_usec = value;
            } else if (sscanf(line, "system_usec %lu", &value) == 1) {
                metrics->system_usec = value;
            } else if (sscanf(line, "nr_periods %lu", &value) == 1) {
                metrics->nr_periods = value;
            } else if (sscanf(line, "nr_throttled %lu", &value) == 1) {
                metrics->nr_throttled = value;
            } else if (sscanf(line, "throttled_usec %lu", &value) == 1) {
                metrics->throttled_usec = value;
            }
        }
        fclose(fp);
        
        // Ler limites
        snprintf(file_path, sizeof(file_path), "%s/cpu.max", cgroup_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            char quota_str[32], period_str[32];
            if (fscanf(fp, "%31s %31s", quota_str, period_str) == 2) {
                if (strcmp(quota_str, "max") == 0) {
                    metrics->quota = -1;
                } else {
                    metrics->quota = atoll(quota_str);
                }
                metrics->period = atoll(period_str);
            }
            fclose(fp);
        }
        
    } else if (version == 1) {
        // cgroup v1
        snprintf(file_path, sizeof(file_path), "%s/cpuacct.usage", cgroup_path);
        uint64_t usage_ns;
        if (read_cgroup_file_uint64(file_path, &usage_ns) == 0) {
            metrics->usage_usec = usage_ns / 1000;
        }
        
        snprintf(file_path, sizeof(file_path), "%s/cpu.stat", cgroup_path);
        FILE *fp = fopen(file_path, "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                uint64_t value;
                if (sscanf(line, "nr_periods %lu", &value) == 1) {
                    metrics->nr_periods = value;
                } else if (sscanf(line, "nr_throttled %lu", &value) == 1) {
                    metrics->nr_throttled = value;
                } else if (sscanf(line, "throttled_time %lu", &value) == 1) {
                    metrics->throttled_usec = value / 1000;
                }
            }
            fclose(fp);
        }
        
        // Ler limites
        snprintf(file_path, sizeof(file_path), "%s/cpu.cfs_quota_us", cgroup_path);
        read_cgroup_file_int64(file_path, &metrics->quota);
        
        snprintf(file_path, sizeof(file_path), "%s/cpu.cfs_period_us", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->period);
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * Lê métricas de memória
 */
int read_cgroup_memory_metrics(const char *cgroup_path, cgroup_memory_metrics_t *metrics) {
    if (cgroup_path == NULL || metrics == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(metrics, 0, sizeof(cgroup_memory_metrics_t));
    
    int version = detect_cgroup_version();
    char file_path[PATH_MAX];
    
    if (version == 2) {
        // cgroup v2
        snprintf(file_path, sizeof(file_path), "%s/memory.current", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->current);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.peak", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->peak);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.max", cgroup_path);
        uint64_t limit;
        if (read_cgroup_file_uint64(file_path, &limit) == 0) {
            metrics->limit = limit;
        } else {
            metrics->limit = UINT64_MAX; // sem limite
        }
        
        snprintf(file_path, sizeof(file_path), "%s/memory.swap.current", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->swap_current);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.swap.max", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->swap_limit);
        
        // Ler estatísticas detalhadas
        snprintf(file_path, sizeof(file_path), "%s/memory.stat", cgroup_path);
        FILE *fp = fopen(file_path, "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                uint64_t value;
                if (sscanf(line, "cache %lu", &value) == 1) {
                    metrics->cache = value;
                } else if (sscanf(line, "rss %lu", &value) == 1) {
                    metrics->rss = value;
                } else if (sscanf(line, "rss_huge %lu", &value) == 1) {
                    metrics->rss_huge = value;
                } else if (sscanf(line, "mapped_file %lu", &value) == 1) {
                    metrics->mapped_file = value;
                } else if (sscanf(line, "dirty %lu", &value) == 1) {
                    metrics->dirty = value;
                } else if (sscanf(line, "writeback %lu", &value) == 1) {
                    metrics->writeback = value;
                } else if (sscanf(line, "pgfault %lu", &value) == 1) {
                    metrics->pgfault = value;
                } else if (sscanf(line, "pgmajfault %lu", &value) == 1) {
                    metrics->pgmajfault = value;
                } else if (sscanf(line, "anon %lu", &value) == 1) {
                    metrics->anon = value;
                } else if (sscanf(line, "file %lu", &value) == 1) {
                    metrics->file = value;
                }
            }
            fclose(fp);
        }
        
    } else if (version == 1) {
        // cgroup v1
        snprintf(file_path, sizeof(file_path), "%s/memory.usage_in_bytes", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->current);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.max_usage_in_bytes", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->peak);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.limit_in_bytes", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->limit);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.memsw.usage_in_bytes", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->swap_current);
        
        snprintf(file_path, sizeof(file_path), "%s/memory.memsw.limit_in_bytes", cgroup_path);
        read_cgroup_file_uint64(file_path, &metrics->swap_limit);
        
        // Ler estatísticas
        snprintf(file_path, sizeof(file_path), "%s/memory.stat", cgroup_path);
        FILE *fp = fopen(file_path, "r");
        if (fp != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                uint64_t value;
                if (sscanf(line, "cache %lu", &value) == 1) {
                    metrics->cache = value;
                } else if (sscanf(line, "rss %lu", &value) == 1) {
                    metrics->rss = value;
                } else if (sscanf(line, "rss_huge %lu", &value) == 1) {
                    metrics->rss_huge = value;
                } else if (sscanf(line, "mapped_file %lu", &value) == 1) {
                    metrics->mapped_file = value;
                } else if (sscanf(line, "pgfault %lu", &value) == 1) {
                    metrics->pgfault = value;
                } else if (sscanf(line, "pgmajfault %lu", &value) == 1) {
                    metrics->pgmajfault = value;
                }
            }
            fclose(fp);
        }
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * Lê métricas de Block I/O
 */
int read_cgroup_blkio_metrics(const char *cgroup_path, cgroup_blkio_metrics_t *metrics) {
    if (cgroup_path == NULL || metrics == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(metrics, 0, sizeof(cgroup_blkio_metrics_t));
    
    int version = detect_cgroup_version();
    char file_path[PATH_MAX];
    
    if (version == 2) {
        // cgroup v2 usa io.stat
        snprintf(file_path, sizeof(file_path), "%s/io.stat", cgroup_path);
        
        FILE *fp = fopen(file_path, "r");
        if (fp == NULL) {
            return -1;
        }
        
        char line[256];
        while (fgets(line, sizeof(line), fp) != NULL) {
            uint64_t rbytes, wbytes, rios, wios, dbytes, dios;
            
            // Formato: 8:0 rbytes=X wbytes=Y rios=Z wios=W dbytes=D dios=E
            if (sscanf(line, "%*d:%*d rbytes=%lu wbytes=%lu rios=%lu wios=%lu dbytes=%lu dios=%lu",
                      &rbytes, &wbytes, &rios, &wios, &dbytes, &dios) >= 4) {
                metrics->rbytes += rbytes;
                metrics->wbytes += wbytes;
                metrics->rios += rios;
                metrics->wios += wios;
                if (sscanf(line, "%*d:%*d %*[^d]dbytes=%lu dios=%lu", &dbytes, &dios) == 2) {
                    metrics->dbytes += dbytes;
                    metrics->dios += dios;
                }
            }
        }
        fclose(fp);
        
    } else if (version == 1) {
        // cgroup v1 usa blkio.throttle.io_service_bytes
        snprintf(file_path, sizeof(file_path), "%s/blkio.throttle.io_service_bytes", cgroup_path);
        
        FILE *fp = fopen(file_path, "r");
        if (fp == NULL) {
            return -1;
        }
        
        char line[256];
        while (fgets(line, sizeof(line), fp) != NULL) {
            char op[16];
            uint64_t value;
            
            if (sscanf(line, "%*d:%*d %15s %lu", op, &value) == 2) {
                if (strcmp(op, "Read") == 0) {
                    metrics->rbytes += value;
                } else if (strcmp(op, "Write") == 0) {
                    metrics->wbytes += value;
                }
            }
        }
        fclose(fp);
        
        // Ler operações
        snprintf(file_path, sizeof(file_path), "%s/blkio.throttle.io_serviced", cgroup_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL) {
                char op[16];
                uint64_t value;
                
                if (sscanf(line, "%*d:%*d %15s %lu", op, &value) == 2) {
                    if (strcmp(op, "Read") == 0) {
                        metrics->rios += value;
                    } else if (strcmp(op, "Write") == 0) {
                        metrics->wios += value;
                    }
                }
            }
            fclose(fp);
        }
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * Lê métricas de PIDs
 */
int read_cgroup_pids_metrics(const char *cgroup_path, cgroup_pids_metrics_t *metrics) {
    if (cgroup_path == NULL || metrics == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(metrics, 0, sizeof(cgroup_pids_metrics_t));
    
    char file_path[PATH_MAX];
    
    // Funciona tanto em v1 quanto v2
    snprintf(file_path, sizeof(file_path), "%s/pids.current", cgroup_path);
    read_cgroup_file_uint64(file_path, &metrics->current);
    
    snprintf(file_path, sizeof(file_path), "%s/pids.max", cgroup_path);
    if (read_cgroup_file_uint64(file_path, &metrics->limit) != 0) {
        metrics->limit = UINT64_MAX; // sem limite
    }
    
    return 0;
}

/**
 * Lê todas as métricas de um processo
 */
int read_cgroup_metrics(pid_t pid, cgroup_metrics_t *metrics) {
    if (metrics == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(metrics, 0, sizeof(cgroup_metrics_t));
    
    metrics->info.pid = pid;
    metrics->info.version = detect_cgroup_version();
    
    // Obter caminho do cgroup
    char cgroup_path[512];
    if (get_process_cgroup_path(pid, NULL, cgroup_path, sizeof(cgroup_path)) != 0) {
        // Tentar com controlador cpu para v1
        if (get_process_cgroup_path(pid, "cpu", cgroup_path, sizeof(cgroup_path)) != 0) {
            return -1;
        }
    }
    
    strncpy(metrics->info.path, cgroup_path, sizeof(metrics->info.path) - 1);
    
    // Tentar ler cada tipo de métrica
    metrics->has_cpu = (read_cgroup_cpu_metrics(cgroup_path, &metrics->cpu) == 0);
    
    // Para memória, pode precisar de caminho diferente em v1
    if (metrics->info.version == 1) {
        char mem_path[512];
        if (get_process_cgroup_path(pid, "memory", mem_path, sizeof(mem_path)) == 0) {
            metrics->has_memory = (read_cgroup_memory_metrics(mem_path, &metrics->memory) == 0);
        }
        
        char blkio_path[512];
        if (get_process_cgroup_path(pid, "blkio", blkio_path, sizeof(blkio_path)) == 0) {
metrics->has_blkio = (read_cgroup_blkio_metrics(blkio_path, &metrics->blkio) == 0);
}
} else {
// cgroup v2 usa mesmo caminho
metrics->has_memory = (read_cgroup_memory_metrics(cgroup_path, &metrics->memory) == 0);
metrics->has_blkio = (read_cgroup_blkio_metrics(cgroup_path, &metrics->blkio) == 0);
}metrics->has_pids = (read_cgroup_pids_metrics(cgroup_path, &metrics->pids) == 0);

return 0;}
// ============================================================================
// Funções de Manipulação
// ============================================================================
int create_cgroup(const char *name, cgroup_controller_t controller) {
if (name == NULL) {
errno = EINVAL;
return -1;
}int version = detect_cgroup_version();
char path[PATH_MAX];

if (version == 2) {
    snprintf(path, sizeof(path), "/sys/fs/cgroup/%s", name);
} else if (version == 1) {
    snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s",
            cgroup_controller_to_string(controller), name);
} else {
    return -1;
}

if (mkdir(path, 0755) != 0) {
    if (errno != EEXIST) {
        return -1;
    }
}

return 0;}
int remove_cgroup(const char *path) {
if (path == NULL) {
errno = EINVAL;
return -1;
}return rmdir(path);}
int move_process_to_cgroup(pid_t pid, const char *cgroup_path) {
if (cgroup_path == NULL) {
errno = EINVAL;
return -1;
}char procs_path[PATH_MAX];
snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cgroup_path);

FILE *fp = fopen(procs_path, "w");
if (fp == NULL) {
    return -1;
}

fprintf(fp, "%d", pid);
fclose(fp);

return 0;}
int set_cgroup_cpu_limit(const char *cgroup_path, double cpu_cores) {
if (cgroup_path == NULL || cpu_cores <= 0) {
errno = EINVAL;
return -1;
}int version = detect_cgroup_version();
char file_path[PATH_MAX];

if (version == 2) {
    // cgroup v2: cpu.max
    snprintf(file_path, sizeof(file_path), "%s/cpu.max", cgroup_path);
    
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        return -1;
    }
    
    // Formato: quota period
    // Exemplo: 50000 100000 = 0.5 core
    uint64_t period = 100000; // 100ms
    uint64_t quota = (uint64_t)(cpu_cores * period);
    
    fprintf(fp, "%lu %lu", quota, period);
    fclose(fp);
    
} else if (version == 1) {
    // cgroup v1: cpu.cfs_quota_us e cpu.cfs_period_us
    uint64_t period = 100000; // 100ms
    uint64_t quota = (uint64_t)(cpu_cores * period);
    
    snprintf(file_path, sizeof(file_path), "%s/cpu.cfs_period_us", cgroup_path);
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%lu", period);
    fclose(fp);
    
    snprintf(file_path, sizeof(file_path), "%s/cpu.cfs_quota_us", cgroup_path);
    fp = fopen(file_path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%lu", quota);
    fclose(fp);
    
} else {
    return -1;
}

return 0;}
int set_cgroup_memory_limit(const char *cgroup_path, uint64_t bytes) {
if (cgroup_path == NULL || bytes == 0) {
errno = EINVAL;
return -1;
}int version = detect_cgroup_version();
char file_path[PATH_MAX];

if (version == 2) {
    snprintf(file_path, sizeof(file_path), "%s/memory.max", cgroup_path);
} else if (version == 1) {
    snprintf(file_path, sizeof(file_path), "%s/memory.limit_in_bytes", cgroup_path);
} else {
    return -1;
}

FILE *fp = fopen(file_path, "w");
if (fp == NULL) {
    return -1;
}

fprintf(fp, "%lu", bytes);
fclose(fp);

return 0;}
int set_cgroup_io_limit(const char *cgroup_path, const char *device,
uint64_t rbps, uint64_t wbps) {
if (cgroup_path == NULL || device == NULL) {
errno = EINVAL;
return -1;
}int version = detect_cgroup_version();
char file_path[PATH_MAX];

if (version == 2) {
    snprintf(file_path, sizeof(file_path), "%s/io.max", cgroup_path);
    
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        return -1;
    }
    
    // Formato: device rbps=X wbps=Y
    fprintf(fp, "%s rbps=%lu wbps=%lu", device, rbps, wbps);
    fclose(fp);
    
} else if (version == 1) {
    // cgroup v1: blkio.throttle.read_bps_device e write_bps_device
    snprintf(file_path, sizeof(file_path), "%s/blkio.throttle.read_bps_device", cgroup_path);
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%s %lu", device, rbps);
    fclose(fp);
    
    snprintf(file_path, sizeof(file_path), "%s/blkio.throttle.write_bps_device", cgroup_path);
    fp = fopen(file_path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%s %lu", device, wbps);
    fclose(fp);
    
} else {
    return -1;
}

return 0;}
// ============================================================================
// Funções de Impressão
// ============================================================================
void print_cgroup_info(const cgroup_info_t *info) {
if (info == NULL) {
return;
}printf("Cgroup Information:\n");
printf("  Path:    %s\n", info->path);
printf("  Version: %d\n", info->version);
if (info->pid > 0) {
    printf("  PID:     %d\n", info->pid);
}}
void print_cgroup_cpu_metrics(const cgroup_cpu_metrics_t *metrics) {
if (metrics == NULL) {
return;
}printf("CPU Metrics:\n");
printf("  Usage:      %.2f seconds\n", metrics->usage_usec / 1000000.0);
printf("  User:       %.2f seconds\n", metrics->user_usec / 1000000.0);
printf("  System:     %.2f seconds\n", metrics->system_usec / 1000000.0);

if (metrics->nr_periods > 0) {
    printf("  Periods:    %lu\n", metrics->nr_periods);
    printf("  Throttled:  %lu (%.2f%%)\n", 
           metrics->nr_throttled,
           (metrics->nr_throttled * 100.0) / metrics->nr_periods);
    printf("  Throttle Time: %.2f seconds\n", metrics->throttled_usec / 1000000.0);
}

if (metrics->quota > 0) {
    double cpu_limit = (double)metrics->quota / metrics->period;
    printf("  Limit:      %.2f cores\n", cpu_limit);
} else if (metrics->quota == -1) {
    printf("  Limit:      unlimited\n");
}}
void print_cgroup_memory_metrics(const cgroup_memory_metrics_t *metrics) {
if (metrics == NULL) {
return;
}printf("Memory Metrics:\n");
printf("  Current:    %.2f MB\n", metrics->current / (1024.0 * 1024.0));
printf("  Peak:       %.2f MB\n", metrics->peak / (1024.0 * 1024.0));

if (metrics->limit < UINT64_MAX) {
    printf("  Limit:      %.2f MB (%.2f%% used)\n",
           metrics->limit / (1024.0 * 1024.0),
           (metrics->current * 100.0) / metrics->limit);
} else {
    printf("  Limit:      unlimited\n");
}

printf("  RSS:        %.2f MB\n", metrics->rss / (1024.0 * 1024.0));
printf("  Cache:      %.2f MB\n", metrics->cache / (1024.0 * 1024.0));
printf("  Swap:       %.2f MB\n", metrics->swap_current / (1024.0 * 1024.0));

if (metrics->pgfault > 0) {
    printf("  Page Faults: %lu (major: %lu)\n", 
           metrics->pgfault, metrics->pgmajfault);
}}
void print_cgroup_blkio_metrics(const cgroup_blkio_metrics_t *metrics) {
if (metrics == NULL) {
return;
}printf("Block I/O Metrics:\n");
printf("  Read:       %.2f MB (%lu ops)\n",
       metrics->rbytes / (1024.0 * 1024.0), metrics->rios);
printf("  Write:      %.2f MB (%lu ops)\n",
       metrics->wbytes / (1024.0 * 1024.0), metrics->wios);

if (metrics->dbytes > 0) {
    printf("  Discard:    %.2f MB (%lu ops)\n",
           metrics->dbytes / (1024.0 * 1024.0), metrics->dios);
}}
void print_cgroup_pids_metrics(const cgroup_pids_metrics_t *metrics) {
if (metrics == NULL) {
return;
}printf("PIDs Metrics:\n");
printf("  Current:    %lu\n", metrics->current);

if (metrics->limit < UINT64_MAX) {
    printf("  Limit:      %lu (%.2f%% used)\n",
           metrics->limit,
           (metrics->current * 100.0) / metrics->limit);
} else {
    printf("  Limit:      unlimited\n");
}}
void print_cgroup_metrics(const cgroup_metrics_t *metrics) {
if (metrics == NULL) {
return;
}printf("\n");
printf("╔════════════════════════════════════════════════════════════╗\n");
printf("║              Cgroup Metrics Report                         ║\n");
printf("╚════════════════════════════════════════════════════════════╝\n");
printf("\n");

print_cgroup_info(&metrics->info);
printf("\n");

if (metrics->has_cpu) {
    print_cgroup_cpu_metrics(&metrics->cpu);
    printf("\n");
}

if (metrics->has_memory) {
    print_cgroup_memory_metrics(&metrics->memory);
    printf("\n");
}

if (metrics->has_blkio) {
    print_cgroup_blkio_metrics(&metrics->blkio);
    printf("\n");
}

if (metrics->has_pids) {
    print_cgroup_pids_metrics(&metrics->pids);
    printf("\n");
}
}
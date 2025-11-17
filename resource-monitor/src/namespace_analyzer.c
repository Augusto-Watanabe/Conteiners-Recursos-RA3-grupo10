#define _GNU_SOURCE
#include "namespace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <sched.h>
#include <sys/wait.h>
#include "monitor.h"

// Mapeamento de tipos de namespace para strings
static const char* ns_type_names[MAX_NAMESPACES] = {
    "cgroup",
    "ipc",
    "mnt",
    "net",
    "pid",
    "time",
    "user",
    "uts"
};

/**
 * Converte tipo de namespace para string
 */
const char* namespace_type_to_string(namespace_type_t type) {
    if (type >= 0 && type < MAX_NAMESPACES) {
        return ns_type_names[type];
    }
    return "unknown";
}

/**
 * Lê o inode de um namespace
 */
static int read_namespace_inode(const char *path, ino_t *inode) {
    struct stat st;
    
    if (stat(path, &st) != 0) {
        return -1;
    }
    
    *inode = st.st_ino;
    return 0;
}

/**
 * Lista todos os namespaces de um processo
 */
int list_process_namespaces(pid_t pid, process_namespaces_t *ns_info) {
    if (ns_info == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(ns_info, 0, sizeof(process_namespaces_t));
    ns_info->pid = pid;
    
    for (int i = 0; i < MAX_NAMESPACES; i++) {
        namespace_info_t *ns = &ns_info->namespaces[i];
        
        ns->type = (namespace_type_t)i;
        strncpy(ns->type_name, ns_type_names[i], sizeof(ns->type_name) - 1);
        
        snprintf(ns->path, sizeof(ns->path), "/proc/%d/ns/%s", pid, ns_type_names[i]);
        
        if (read_namespace_inode(ns->path, &ns->inode) == 0) {
            ns->available = 1;
            ns_info->count++;
        } else {
            ns->available = 0;
            ns->inode = 0;
        }
    }
    
    return 0;
}

/**
 * Compara namespaces entre dois processos
 */
int compare_process_namespaces(pid_t pid1, pid_t pid2,
                               namespace_comparison_t *comparisons,
                               int *count) {
    if (comparisons == NULL || count == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    process_namespaces_t ns1, ns2;
    
    if (list_process_namespaces(pid1, &ns1) != 0) {
        return -1;
    }
    
    if (list_process_namespaces(pid2, &ns2) != 0) {
        return -1;
    }
    
    *count = 0;
    
    for (int i = 0; i < MAX_NAMESPACES; i++) {
        if (ns1.namespaces[i].available && ns2.namespaces[i].available) {
            namespace_comparison_t *comp = &comparisons[*count];
            
            comp->type = (namespace_type_t)i;
            strncpy(comp->type_name, ns_type_names[i], sizeof(comp->type_name) - 1);
            comp->inode_pid1 = ns1.namespaces[i].inode;
            comp->inode_pid2 = ns2.namespaces[i].inode;
            comp->shared = (comp->inode_pid1 == comp->inode_pid2) ? 1 : 0;
            
            (*count)++;
        }
    }
    
    return 0;
}

/**
 * Encontra processos em um namespace específico
 */
int find_processes_in_namespace(ino_t ns_inode,
                               namespace_type_t ns_type,
                               pid_t *pids,
                               int max_pids,
                               int *count) {
    if (pids == NULL || count == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    *count = 0;
    
    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL && *count < max_pids) {
        if (entry->d_type != DT_DIR) {
            continue;
        }
        
        char *endptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 0) {
            continue;
        }
        
        char ns_path[256];
        snprintf(ns_path, sizeof(ns_path), "/proc/%ld/ns/%s",
                pid, ns_type_names[ns_type]);
        
        ino_t inode;
        if (read_namespace_inode(ns_path, &inode) == 0) {
            if (inode == ns_inode) {
                pids[*count] = (pid_t)pid;
                (*count)++;
            }
        }
    }
    
    closedir(proc_dir);
    return 0;
}

/**
 * Verifica se processo está isolado do init
 */
int is_process_isolated(pid_t pid, namespace_type_t ns_type) {
    char path_init[256], path_pid[256];
    ino_t inode_init, inode_pid;
    
    snprintf(path_init, sizeof(path_init), "/proc/1/ns/%s", ns_type_names[ns_type]);
    if (read_namespace_inode(path_init, &inode_init) != 0) {
        return -1;
    }
    
    snprintf(path_pid, sizeof(path_pid), "/proc/%d/ns/%s", pid, ns_type_names[ns_type]);
    if (read_namespace_inode(path_pid, &inode_pid) != 0) {
        return -1;
    }
    
    return (inode_init != inode_pid) ? 1 : 0;
}

/**
 * Obtém estatísticas de namespaces no sistema
 */
int get_namespace_statistics(namespace_statistics_t *stats) {
    if (stats == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(stats, 0, sizeof(namespace_statistics_t));
    
    ino_t unique_inodes[MAX_NAMESPACES][1024];
    int unique_counts[MAX_NAMESPACES] = {0};
    
    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type != DT_DIR) {
            continue;
        }
        
        char *endptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 0) {
            continue;
        }
        
        stats->total_processes_analyzed++;
        
        for (int i = 0; i < MAX_NAMESPACES; i++) {
            char ns_path[256];
            snprintf(ns_path, sizeof(ns_path), "/proc/%ld/ns/%s",
                    pid, ns_type_names[i]);
            
            ino_t inode;
            if (read_namespace_inode(ns_path, &inode) == 0) {
                int found = 0;
                for (int j = 0; j < unique_counts[i]; j++) {
                    if (unique_inodes[i][j] == inode) {
                        found = 1;
                        break;
                    }
                }
                
                if (!found && unique_counts[i] < 1024) {
                    unique_inodes[i][unique_counts[i]++] = inode;
                }
            }
        }
    }
    
    closedir(proc_dir);
    
    stats->unique_cgroup_namespaces = unique_counts[NS_CGROUP];
    stats->unique_ipc_namespaces = unique_counts[NS_IPC];
    stats->unique_mnt_namespaces = unique_counts[NS_MNT];
    stats->unique_net_namespaces = unique_counts[NS_NET];
    stats->unique_pid_namespaces = unique_counts[NS_PID];
    stats->unique_time_namespaces = unique_counts[NS_TIME];
    stats->unique_user_namespaces = unique_counts[NS_USER];
    stats->unique_uts_namespaces = unique_counts[NS_UTS];
    
    return 0;
}

/**
 * Mede tempo de criação de namespace
 */
long measure_namespace_creation_time(namespace_type_t ns_type) {
    struct timespec start, end;
    
    int flags = 0;
    switch (ns_type) {
        case NS_CGROUP: flags = CLONE_NEWCGROUP; break;
        case NS_IPC:    flags = CLONE_NEWIPC; break;
        case NS_MNT:    flags = CLONE_NEWNS; break;
        case NS_NET:    flags = CLONE_NEWNET; break;
        case NS_PID:    flags = CLONE_NEWPID; break;
        case NS_USER:   flags = CLONE_NEWUSER; break;
        case NS_UTS:    flags = CLONE_NEWUTS; break;
        case NS_TIME:   return -1;
        default:
            errno = EINVAL;
            return -1;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    
    if (pid == 0) {
        if (unshare(flags) == 0) {
            exit(0);
        } else {
            exit(1);
        }
    }
    
    int status;
    waitpid(pid, &status, 0);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    if (WEXITSTATUS(status) != 0) {
        return -1;
    }
    
    long elapsed = (end.tv_sec - start.tv_sec) * 1000000L +
                   (end.tv_nsec - start.tv_nsec) / 1000L;
    
    return elapsed;
}

void print_process_namespaces(const process_namespaces_t *ns_info) {
    if (ns_info == NULL) {
        return;
    }
    
    printf("Namespaces for PID %d:\n", ns_info->pid);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("%-10s %-12s %-20s\n", "Type", "Available", "Inode");
    printf("───────────────────────────────────────────────────────────\n");
    
    for (int i = 0; i < MAX_NAMESPACES; i++) {
        const namespace_info_t *ns = &ns_info->namespaces[i];
        
        if (ns->available) {
            printf("%-10s %-12s %-20lu\n",
                   ns->type_name,
                   "Yes",
                   ns->inode);
        } else {
            printf("%-10s %-12s %-20s\n",
                   ns->type_name,
                   "No",
                   "N/A");
        }
    }
    
    printf("───────────────────────────────────────────────────────────\n");
    printf("Total available: %d/%d\n", ns_info->count, MAX_NAMESPACES);
}

void print_namespace_comparison(pid_t pid1, pid_t pid2,
                                const namespace_comparison_t *comparisons,
                                int count) {
    printf("\nNamespace Comparison: PID %d vs PID %d\n", pid1, pid2);
    printf("═══════════════════════════════════════════════════════════\n");
    printf("%-10s %-12s %-20s %-20s\n", "Type", "Status", "PID1 Inode", "PID2 Inode");
    printf("───────────────────────────────────────────────────────────\n");
    
    int shared_count = 0;
    int isolated_count = 0;
    
    for (int i = 0; i < count; i++) {
        const namespace_comparison_t *comp = &comparisons[i];
        
        printf("%-10s %-12s %-20lu %-20lu\n",
               comp->type_name,
               comp->shared ? "Shared" : "Isolated",
               comp->inode_pid1,
               comp->inode_pid2);
        
        if (comp->shared) {
            shared_count++;
        } else {
            isolated_count++;
        }
    }
    
    printf("───────────────────────────────────────────────────────────\n");
    printf("Shared: %d | Isolated: %d | Total: %d\n",
           shared_count, isolated_count, count);
}

void print_namespace_statistics(const namespace_statistics_t *stats) {
    if (stats == NULL) {
        return;
    }
    
    printf("\nSystem Namespace Statistics\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Total Processes Analyzed: %d\n\n", stats->total_processes_analyzed);
    
    printf("Unique Namespaces per Type:\n");
    printf("  cgroup: %d\n", stats->unique_cgroup_namespaces);
    printf("  ipc:    %d\n", stats->unique_ipc_namespaces);
    printf("  mnt:    %d\n", stats->unique_mnt_namespaces);
    printf("  net:    %d\n", stats->unique_net_namespaces);
    printf("  pid:    %d\n", stats->unique_pid_namespaces);
    printf("  time:   %d\n", stats->unique_time_namespaces);
    printf("  user:   %d\n", stats->unique_user_namespaces);
    printf("  uts:    %d\n", stats->unique_uts_namespaces);
}

// Note: As funções de relatório (generate_namespace_report, map_processes_by_namespace, 
// generate_process_namespace_report) serão implementadas posteriormente se necessário
// para evitar complexidade desnecessária neste momento.

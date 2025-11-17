#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <sys/types.h>
#include <stdint.h>

// Número máximo de tipos de namespaces no Linux
#define MAX_NAMESPACES 8

// Tipos de namespaces disponíveis no Linux
typedef enum {
    NS_CGROUP = 0,
    NS_IPC,
    NS_MNT,
    NS_NET,
    NS_PID,
    NS_TIME,
    NS_USER,
    NS_UTS
} namespace_type_t;

// Informações sobre um namespace específico
typedef struct {
    namespace_type_t type;
    ino_t inode;
    char type_name[16];
    char path[256];
    int available;
} namespace_info_t;

// Conjunto completo de namespaces de um processo
typedef struct {
    pid_t pid;
    int count;
    namespace_info_t namespaces[MAX_NAMESPACES];
} process_namespaces_t;

// Resultado da comparação entre namespaces
typedef struct {
    namespace_type_t type;
    char type_name[16];
    int shared;
    ino_t inode_pid1;
    ino_t inode_pid2;
} namespace_comparison_t;

// Estatísticas de namespaces no sistema
typedef struct {
    int total_processes_analyzed;
    int unique_pid_namespaces;
    int unique_net_namespaces;
    int unique_mnt_namespaces;
    int unique_ipc_namespaces;
    int unique_uts_namespaces;
    int unique_user_namespaces;
    int unique_cgroup_namespaces;
    int unique_time_namespaces;
} namespace_statistics_t;

// ============================================================================
// Funções principais
// ============================================================================

int list_process_namespaces(pid_t pid, process_namespaces_t *ns_info);

int compare_process_namespaces(pid_t pid1, pid_t pid2, 
                               namespace_comparison_t *comparisons,
                               int *count);

int find_processes_in_namespace(ino_t ns_inode, 
                               namespace_type_t ns_type,
                               pid_t *pids, 
                               int max_pids,
                               int *count);

int get_namespace_statistics(namespace_statistics_t *stats);

int is_process_isolated(pid_t pid, namespace_type_t ns_type);

// ============================================================================
// Funções de impressão
// ============================================================================

void print_process_namespaces(const process_namespaces_t *ns_info);

void print_namespace_comparison(pid_t pid1, pid_t pid2,
                               const namespace_comparison_t *comparisons,
                               int count);

void print_namespace_statistics(const namespace_statistics_t *stats);

const char* namespace_type_to_string(namespace_type_t type);

long measure_namespace_creation_time(namespace_type_t ns_type);

#endif // NAMESPACE_H

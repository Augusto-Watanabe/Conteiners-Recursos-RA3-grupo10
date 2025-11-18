#ifndef CGROUP_H
#define CGROUP_H

#include <stdint.h>
#include <sys/types.h>

// ============================================================================
// Tipos de Controladores de Cgroup
// ============================================================================

typedef enum {
    CGROUP_CPU = 0,
    CGROUP_MEMORY,
    CGROUP_BLKIO,
    CGROUP_PIDS,
    CGROUP_CPUSET,
    CGROUP_IO,
    CGROUP_CONTROLLER_COUNT
} cgroup_controller_t;

// ============================================================================
// Estruturas de Métricas
// ============================================================================

/**
 * Métricas de CPU do cgroup
 */
typedef struct {
    uint64_t usage_usec;        // Tempo total de CPU em microssegundos
    uint64_t user_usec;         // Tempo em user mode
    uint64_t system_usec;       // Tempo em system mode
    uint64_t nr_periods;        // Número de períodos
    uint64_t nr_throttled;      // Número de vezes que foi limitado
    uint64_t throttled_usec;    // Tempo total limitado em microssegundos
    int64_t quota;              // Quota configurada (-1 = sem limite)
    uint64_t period;            // Período em microssegundos
} cgroup_cpu_metrics_t;

/**
 * Métricas de Memória do cgroup
 */
typedef struct {
    uint64_t current;           // Uso atual de memória
    uint64_t peak;              // Pico de uso
    uint64_t limit;             // Limite configurado
    uint64_t swap_current;      // Uso atual de swap
    uint64_t swap_limit;        // Limite de swap
    uint64_t cache;             // Memória em cache
    uint64_t rss;               // Resident Set Size
    uint64_t rss_huge;          // RSS de huge pages
    uint64_t mapped_file;       // Arquivos mapeados
    uint64_t dirty;             // Páginas dirty
    uint64_t writeback;         // Páginas em writeback
    uint64_t pgfault;           // Page faults
    uint64_t pgmajfault;        // Major page faults
    uint64_t anon;              // Memória anônima
    uint64_t file;              // Memória de arquivo
} cgroup_memory_metrics_t;

/**
 * Métricas de Block I/O do cgroup
 */
typedef struct {
    uint64_t rbytes;            // Bytes lidos
    uint64_t wbytes;            // Bytes escritos
    uint64_t rios;              // Operações de leitura
    uint64_t wios;              // Operações de escrita
    uint64_t dbytes;            // Bytes descartados
    uint64_t dios;              // Operações de descarte
} cgroup_blkio_metrics_t;

/**
 * Métricas de PIDs do cgroup
 */
typedef struct {
    uint64_t current;           // Número atual de PIDs
    uint64_t limit;             // Limite de PIDs
} cgroup_pids_metrics_t;

/**
 * Informações sobre um cgroup
 */
typedef struct {
    char path[512];             // Caminho do cgroup
    char name[256];             // Nome do cgroup
    int version;                // Versão (1 ou 2)
    pid_t pid;                  // PID do processo (se aplicável)
    int controllers_available;  // Bitmask de controladores disponíveis
} cgroup_info_t;

/**
 * Conjunto completo de métricas de um cgroup
 */
typedef struct {
    cgroup_info_t info;
    cgroup_cpu_metrics_t cpu;
    cgroup_memory_metrics_t memory;
    cgroup_blkio_metrics_t blkio;
    cgroup_pids_metrics_t pids;
    int has_cpu;
    int has_memory;
    int has_blkio;
    int has_pids;
} cgroup_metrics_t;

// ============================================================================
// Funções de Leitura de Métricas
// ============================================================================

/**
 * Detecta a versão de cgroup do sistema
 * @return 1 para cgroup v1, 2 para cgroup v2, -1 em erro
 */
int detect_cgroup_version(void);

/**
 * Obtém o caminho do cgroup de um processo
 * @param pid Process ID
 * @param controller Nome do controlador (NULL para cgroup v2)
 * @param path Buffer para receber o caminho
 * @param size Tamanho do buffer
 * @return 0 em sucesso, -1 em erro
 */
int get_process_cgroup_path(pid_t pid, const char *controller, 
                            char *path, size_t size);

/**
 * Lê métricas de CPU de um cgroup
 * @param cgroup_path Caminho do cgroup
 * @param metrics Estrutura para receber as métricas
 * @return 0 em sucesso, -1 em erro
 */
int read_cgroup_cpu_metrics(const char *cgroup_path, cgroup_cpu_metrics_t *metrics);

/**
 * Lê métricas de memória de um cgroup
 * @param cgroup_path Caminho do cgroup
 * @param metrics Estrutura para receber as métricas
 * @return 0 em sucesso, -1 em erro
 */
int read_cgroup_memory_metrics(const char *cgroup_path, cgroup_memory_metrics_t *metrics);

/**
 * Lê métricas de Block I/O de um cgroup
 * @param cgroup_path Caminho do cgroup
 * @param metrics Estrutura para receber as métricas
 * @return 0 em sucesso, -1 em erro
 */
int read_cgroup_blkio_metrics(const char *cgroup_path, cgroup_blkio_metrics_t *metrics);

/**
 * Lê métricas de PIDs de um cgroup
 * @param cgroup_path Caminho do cgroup
 * @param metrics Estrutura para receber as métricas
 * @return 0 em sucesso, -1 em erro
 */
int read_cgroup_pids_metrics(const char *cgroup_path, cgroup_pids_metrics_t *metrics);

/**
 * Lê todas as métricas de um cgroup
 * @param pid Process ID
 * @param metrics Estrutura para receber todas as métricas
 * @return 0 em sucesso, -1 em erro
 */
int read_cgroup_metrics(pid_t pid, cgroup_metrics_t *metrics);

// ============================================================================
// Funções de Manipulação de Cgroups
// ============================================================================

/**
 * Cria um novo cgroup
 * @param name Nome do cgroup
 * @param controller Controlador a usar
 * @return 0 em sucesso, -1 em erro
 */
int create_cgroup(const char *name, cgroup_controller_t controller);

/**
 * Remove um cgroup
 * @param path Caminho do cgroup
 * @return 0 em sucesso, -1 em erro
 */
int remove_cgroup(const char *path);

/**
 * Move um processo para um cgroup
 * @param pid Process ID
 * @param cgroup_path Caminho do cgroup
 * @return 0 em sucesso, -1 em erro
 */
int move_process_to_cgroup(pid_t pid, const char *cgroup_path);

/**
 * Define limite de CPU (em cores)
 * @param cgroup_path Caminho do cgroup
 * @param cpu_cores Número de cores (ex: 0.5, 1.0, 2.0)
 * @return 0 em sucesso, -1 em erro
 */
int set_cgroup_cpu_limit(const char *cgroup_path, double cpu_cores);

/**
 * Define limite de memória
 * @param cgroup_path Caminho do cgroup
 * @param bytes Limite em bytes
 * @return 0 em sucesso, -1 em erro
 */
int set_cgroup_memory_limit(const char *cgroup_path, uint64_t bytes);

/**
 * Define limite de I/O
 * @param cgroup_path Caminho do cgroup
 * @param device Device (ex: "8:0" para /dev/sda)
 * @param rbps Read bytes per second
 * @param wbps Write bytes per second
 * @return 0 em sucesso, -1 em erro
 */
int set_cgroup_io_limit(const char *cgroup_path, const char *device,
                       uint64_t rbps, uint64_t wbps);

// ============================================================================
// Funções de Impressão
// ============================================================================

void print_cgroup_info(const cgroup_info_t *info);
void print_cgroup_cpu_metrics(const cgroup_cpu_metrics_t *metrics);
void print_cgroup_memory_metrics(const cgroup_memory_metrics_t *metrics);
void print_cgroup_blkio_metrics(const cgroup_blkio_metrics_t *metrics);
void print_cgroup_pids_metrics(const cgroup_pids_metrics_t *metrics);
void print_cgroup_metrics(const cgroup_metrics_t *metrics);

/**
 * Converte tipo de controlador para string
 */
const char* cgroup_controller_to_string(cgroup_controller_t controller);

#endif // CGROUP_H
// ============================================================================
// Funções de Relatório
// ============================================================================

/**
 * Gera relatório de utilização vs limites de um processo
 */
int generate_cgroup_utilization_report(pid_t pid, const char *output_file);

/**
 * Compara utilização de múltiplos processos
 */
int compare_cgroup_utilization(pid_t *pids, int count, const char *output_file);


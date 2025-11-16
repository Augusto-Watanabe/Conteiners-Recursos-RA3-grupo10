#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <sys/types.h>

// ============================================================================
// CPU MONITORING
// ============================================================================

/**
 * Estrutura para métricas de CPU
 */
typedef struct {
    uint64_t user_time;        // Tempo em user mode (clock ticks)
    uint64_t system_time;      // Tempo em kernel mode (clock ticks)
    uint64_t total_time;       // Tempo total de CPU (clock ticks)
    uint32_t num_threads;      // Número de threads
    uint64_t context_switches; // Número total de context switches
    double cpu_percent;        // Percentual de uso de CPU
} cpu_metrics_t;

/**
 * Coleta métricas de CPU de um processo
 * 
 * @param pid Process ID a ser monitorado
 * @param metrics Ponteiro para estrutura que receberá as métricas
 * @return 0 em sucesso, -1 em erro (errno será setado)
 */
int collect_cpu_metrics(pid_t pid, cpu_metrics_t *metrics);

/**
 * Reseta o estado interno do monitor de CPU
 * Útil ao mudar de processo monitorado
 */
void reset_cpu_monitor(void);

/**
 * Converte clock ticks para microssegundos
 * 
 * @param ticks Número de clock ticks
 * @return Tempo em microssegundos
 */
uint64_t ticks_to_microseconds(uint64_t ticks);

/**
 * Imprime métricas de CPU formatadas
 * 
 * @param metrics Ponteiro para estrutura de métricas
 */
void print_cpu_metrics(const cpu_metrics_t *metrics);

// ============================================================================
// MEMORY MONITORING
// ============================================================================

/**
 * Estrutura para métricas de memória
 */
typedef struct {
    uint64_t rss;           // Resident Set Size - memória física (bytes)
    uint64_t vsz;           // Virtual Size - memória virtual (bytes)
    uint64_t page_faults;   // Número total de page faults
    uint64_t swap;          // Quantidade em swap (bytes)
} memory_metrics_t;

/**
 * Coleta métricas de memória de um processo
 * 
 * @param pid Process ID a ser monitorado
 * @param metrics Ponteiro para estrutura que receberá as métricas
 * @return 0 em sucesso, -1 em erro (errno será setado)
 */
int collect_memory_metrics(pid_t pid, memory_metrics_t *metrics);

/**
 * Calcula percentual de memória usada em relação ao total do sistema
 * 
 * @param metrics Ponteiro para estrutura de métricas
 * @return Percentual de uso (0-100), ou -1.0 em erro
 */
double get_memory_usage_percent(const memory_metrics_t *metrics);

/**
 * Detecta possível memory leak calculando taxa de crescimento
 * 
 * @param metrics Ponteiro para estrutura de métricas
 * @return Taxa de crescimento em bytes/segundo (negativo = liberando memória)
 */
double detect_memory_leak(const memory_metrics_t *metrics);

/**
 * Reseta o detector de memory leak
 */
void reset_memory_leak_detector(void);

/**
 * Imprime métricas de memória formatadas
 * 
 * @param metrics Ponteiro para estrutura de métricas
 */
void print_memory_metrics(const memory_metrics_t *metrics);

// ============================================================================
// I/O MONITORING
// ============================================================================

/**
 * Estrutura para métricas de I/O
 */
typedef struct {
    uint64_t bytes_read;      // Bytes lidos
    uint64_t bytes_written;   // Bytes escritos
    uint64_t syscalls_read;   // Número de syscalls de leitura
    uint64_t syscalls_write;  // Número de syscalls de escrita
    double read_rate;         // Taxa de leitura (bytes/s)
    double write_rate;        // Taxa de escrita (bytes/s)
} io_metrics_t;

/**
 * Coleta métricas de I/O de um processo
 * 
 * @param pid Process ID a ser monitorado
 * @param metrics Ponteiro para estrutura que receberá as métricas
 * @return 0 em sucesso, -1 em erro (errno será setado)
 */
int collect_io_metrics(pid_t pid, io_metrics_t *metrics);

/**
 * Reseta o estado interno do monitor de I/O
 */
void reset_io_monitor(void);

/**
 * Imprime métricas de I/O formatadas
 * 
 * @param metrics Ponteiro para estrutura de métricas
 */
void print_io_metrics(const io_metrics_t *metrics);

// ============================================================================
// NETWORK MONITORING
// ============================================================================

/**
 * Estrutura para métricas de rede
 */
typedef struct {
    uint64_t bytes_rx;        // Bytes recebidos
    uint64_t bytes_tx;        // Bytes transmitidos
    uint64_t packets_rx;      // Pacotes recebidos
    uint64_t packets_tx;      // Pacotes transmitidos
    uint32_t num_connections; // Número de conexões ativas
} network_metrics_t;

/**
 * Coleta métricas de rede de um processo
 * 
 * @param pid Process ID a ser monitorado
 * @param metrics Ponteiro para estrutura que receberá as métricas
 * @return 0 em sucesso, -1 em erro (errno será setado)
 */
int collect_network_metrics(pid_t pid, network_metrics_t *metrics);

/**
 * Imprime métricas de rede formatadas
 * 
 * @param metrics Ponteiro para estrutura de métricas
 */
void print_network_metrics(const network_metrics_t *metrics);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * Verifica se um processo existe
 * 
 * @param pid Process ID a verificar
 * @return 1 se existe, 0 se não existe
 */
int process_exists(pid_t pid);

/**
 * Obtém o nome do processo
 * 
 * @param pid Process ID
 * @param name Buffer para receber o nome
 * @param size Tamanho do buffer
 * @return 0 em sucesso, -1 em erro
 */
int get_process_name(pid_t pid, char *name, size_t size);

#endif // MONITOR_H
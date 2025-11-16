#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <sys/types.h>

// ============================================================================
// CPU MONITORING
// ============================================================================

typedef struct {
    uint64_t user_time;
    uint64_t system_time;
    uint64_t total_time;
    uint32_t num_threads;
    uint64_t context_switches;
    double cpu_percent;
} cpu_metrics_t;

int collect_cpu_metrics(pid_t pid, cpu_metrics_t *metrics);
void reset_cpu_monitor(void);
uint64_t ticks_to_microseconds(uint64_t ticks);
void print_cpu_metrics(const cpu_metrics_t *metrics);

// ============================================================================
// MEMORY MONITORING
// ============================================================================

typedef struct {
    uint64_t rss;
    uint64_t vsz;
    uint64_t page_faults;
    uint64_t swap;
} memory_metrics_t;

int collect_memory_metrics(pid_t pid, memory_metrics_t *metrics);
double get_memory_usage_percent(const memory_metrics_t *metrics);
double detect_memory_leak(const memory_metrics_t *metrics);
void reset_memory_leak_detector(void);
void print_memory_metrics(const memory_metrics_t *metrics);

// ============================================================================
// I/O MONITORING
// ============================================================================

typedef struct {
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t syscalls_read;
    uint64_t syscalls_write;
    double read_rate;
    double write_rate;
} io_metrics_t;

int collect_io_metrics(pid_t pid, io_metrics_t *metrics);
void reset_io_monitor(void);
void print_io_metrics(const io_metrics_t *metrics);
double get_total_io_throughput(const io_metrics_t *metrics);
void get_io_efficiency(const io_metrics_t *metrics, double *avg_read_size, double *avg_write_size);

// ============================================================================
// NETWORK MONITORING
// ============================================================================

typedef struct {
    uint64_t bytes_rx;
    uint64_t bytes_tx;
    uint64_t packets_rx;
    uint64_t packets_tx;
    uint32_t num_connections;
} network_metrics_t;

int collect_network_metrics(pid_t pid, network_metrics_t *metrics);
void print_network_metrics(const network_metrics_t *metrics);

// ============================================================================
// UTILITIES
// ============================================================================

int process_exists(pid_t pid);
int get_process_name(pid_t pid, char *name, size_t size);

#endif // MONITOR_H

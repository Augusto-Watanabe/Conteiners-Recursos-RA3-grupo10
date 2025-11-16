#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../include/monitor.h"

// Função que consome CPU
void cpu_intensive_task(int iterations) {
    volatile double result = 0.0;
    for (int i = 0; i < iterations; i++) {
        result += i * 0.001;
    }
}

// Função que aloca memória
void* memory_intensive_task(size_t size) {
    void *ptr = malloc(size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

int main(void) {
    printf("=== Resource Profiler Test ===\n\n");

    pid_t my_pid = getpid();
    printf("Monitoring PID: %d\n\n", my_pid);

    cpu_metrics_t cpu_metrics;
    memory_metrics_t mem_metrics;

    // === Teste 1: Leitura inicial ===
    printf("--- Test 1: Initial Reading ---\n");
    
    if (collect_cpu_metrics(my_pid, &cpu_metrics) == 0) {
        print_cpu_metrics(&cpu_metrics);
    } else {
        fprintf(stderr, "Failed to collect CPU metrics\n");
    }
    printf("\n");

    if (collect_memory_metrics(my_pid, &mem_metrics) == 0) {
        print_memory_metrics(&mem_metrics);
        double mem_percent = get_memory_usage_percent(&mem_metrics);
        if (mem_percent >= 0) {
            printf("  Memory Usage:     %.2f%% of system memory\n", mem_percent);
        }
    } else {
        fprintf(stderr, "Failed to collect memory metrics\n");
    }
    printf("\n");

    // === Teste 2: Após trabalho de CPU ===
    printf("--- Test 2: After CPU Work ---\n");
    printf("Performing CPU-intensive task...\n");
    
    cpu_intensive_task(50000000);
    sleep(1);

    if (collect_cpu_metrics(my_pid, &cpu_metrics) == 0) {
        print_cpu_metrics(&cpu_metrics);
    }
    printf("\n");

    // === Teste 3: Após alocação de memória ===
    printf("--- Test 3: After Memory Allocation ---\n");
    printf("Allocating 50 MB...\n");
    
    #define MB (1024 * 1024)
    void *mem1 = memory_intensive_task(50 * MB);
    
    if (collect_memory_metrics(my_pid, &mem_metrics) == 0) {
        print_memory_metrics(&mem_metrics);
        double mem_percent = get_memory_usage_percent(&mem_metrics);
        if (mem_percent >= 0) {
            printf("  Memory Usage:     %.2f%% of system memory\n", mem_percent);
        }
    }
    printf("\n");

    // === Teste 4: Monitoramento contínuo ===
    printf("--- Test 4: Continuous Monitoring (5 iterations) ---\n");
    
    for (int i = 0; i < 5; i++) {
        printf("\nIteration %d:\n", i + 1);
        
        cpu_intensive_task(10000000);
        void *temp = memory_intensive_task(10 * MB);
        
        if (collect_cpu_metrics(my_pid, &cpu_metrics) == 0) {
            printf("  CPU%%: %.2f%% | Threads: %u | Switches: %lu\n",
                   cpu_metrics.cpu_percent,
                   cpu_metrics.num_threads,
                   cpu_metrics.context_switches);
        }
        
        if (collect_memory_metrics(my_pid, &mem_metrics) == 0) {
            double leak_rate = detect_memory_leak(&mem_metrics);
            printf("  RSS: %.2f MB | VSZ: %.2f MB | Leak Rate: %.2f KB/s\n",
                   mem_metrics.rss / (1024.0 * 1024.0),
                   mem_metrics.vsz / (1024.0 * 1024.0),
                   leak_rate / 1024.0);
        }
        
        sleep(1);
        free(temp);
    }
    printf("\n");

    // === Teste 5: Verificar PID inexistente ===
    printf("--- Test 5: Invalid PID Test ---\n");
    pid_t invalid_pid = 999999;
    printf("Testing with PID: %d (should fail gracefully)\n", invalid_pid);
    
    if (collect_cpu_metrics(invalid_pid, &cpu_metrics) != 0) {
        printf("✓ Correctly failed to collect metrics for invalid PID\n");
    }
    printf("\n");

    free(mem1);

    printf("=== All Tests Completed ===\n");
    return EXIT_SUCCESS;
}

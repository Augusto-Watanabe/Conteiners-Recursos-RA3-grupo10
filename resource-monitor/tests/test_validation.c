#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "../include/monitor.h"

#define COLOR_GREEN "\033[0;32m"
#define COLOR_RED "\033[0;31m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_RESET "\033[0m"

int tests_passed = 0;
int tests_failed = 0;

void print_test_result(const char *test_name, int passed) {
    if (passed) {
        printf("[%sPASS%s] %s\n", COLOR_GREEN, COLOR_RESET, test_name);
        tests_passed++;
    } else {
        printf("[%sFAIL%s] %s\n", COLOR_RED, COLOR_RESET, test_name);
        tests_failed++;
    }
}

void test_process_exists(void) {
    pid_t my_pid = getpid();
    int exists = process_exists(my_pid);
    print_test_result("process_exists() with valid PID", exists == 1);
    
    int not_exists = process_exists(999999);
    print_test_result("process_exists() with invalid PID", not_exists == 0);
}

void test_get_process_name(void) {
    char name[256];
    pid_t my_pid = getpid();
    
    int result = get_process_name(my_pid, name, sizeof(name));
    print_test_result("get_process_name() with valid PID", result == 0 && strlen(name) > 0);
    
    result = get_process_name(999999, name, sizeof(name));
    print_test_result("get_process_name() with invalid PID", result != 0);
    
    result = get_process_name(my_pid, NULL, 0);
    print_test_result("get_process_name() with NULL buffer", result != 0);
}

void test_cpu_metrics(void) {
    pid_t my_pid = getpid();
    cpu_metrics_t metrics;
    
    int result = collect_cpu_metrics(my_pid, &metrics);
    print_test_result("collect_cpu_metrics() with valid PID", result == 0);
    
    if (result == 0) {
        // Para unsigned, apenas verificamos se são valores razoáveis
        print_test_result("CPU metrics have valid values", 
                         metrics.total_time > 0 &&
                         metrics.num_threads > 0);
    }
    
    result = collect_cpu_metrics(999999, &metrics);
    print_test_result("collect_cpu_metrics() with invalid PID", result != 0);
    
    result = collect_cpu_metrics(my_pid, NULL);
    print_test_result("collect_cpu_metrics() with NULL metrics", result != 0);
}

void test_memory_metrics(void) {
    pid_t my_pid = getpid();
    memory_metrics_t metrics;
    
    int result = collect_memory_metrics(my_pid, &metrics);
    print_test_result("collect_memory_metrics() with valid PID", result == 0);
    
    if (result == 0) {
        print_test_result("Memory metrics have valid values",
                         metrics.rss > 0 && metrics.vsz > 0);
    }
    
    result = collect_memory_metrics(999999, &metrics);
    print_test_result("collect_memory_metrics() with invalid PID", result != 0);
    
    result = collect_memory_metrics(my_pid, NULL);
    print_test_result("collect_memory_metrics() with NULL metrics", result != 0);
}

void test_io_metrics(void) {
    pid_t my_pid = getpid();
    io_metrics_t metrics;
    
    int result = collect_io_metrics(my_pid, &metrics);
    
    // I/O pode falhar sem root, então só testamos se temos permissão
    if (geteuid() == 0) {
        print_test_result("collect_io_metrics() with valid PID (as root)", result == 0);
    } else {
        printf("[%sSKIP%s] collect_io_metrics() - requires root\n", 
               COLOR_YELLOW, COLOR_RESET);
    }
    
    result = collect_io_metrics(my_pid, NULL);
    print_test_result("collect_io_metrics() with NULL metrics", result != 0);
}

void test_cpu_percentage_calculation(void) {
    pid_t my_pid = getpid();
    cpu_metrics_t metrics1, metrics2;
    
    // Primeira leitura
    if (collect_cpu_metrics(my_pid, &metrics1) != 0) {
        print_test_result("CPU percentage calculation", 0);
        return;
    }
    
    // Fazer algum trabalho
    volatile double x = 0;
    for (int i = 0; i < 10000000; i++) {
        x += i * 0.001;
    }
    
    sleep(1);
    
    // Segunda leitura
    if (collect_cpu_metrics(my_pid, &metrics2) != 0) {
        print_test_result("CPU percentage calculation", 0);
        return;
    }
    
    // CPU% deve ser >= 0 na segunda leitura
    print_test_result("CPU percentage calculation", 
                     metrics2.cpu_percent >= 0.0 && metrics2.cpu_percent <= 100.0);
}

void test_memory_leak_detection(void) {
    pid_t my_pid = getpid();
    memory_metrics_t metrics;
    
    reset_memory_leak_detector();
    
    // Primeira leitura
    if (collect_memory_metrics(my_pid, &metrics) != 0) {
        print_test_result("Memory leak detection", 0);
        return;
    }
    
    double rate1 = detect_memory_leak(&metrics);
    
    // Alocar memória
    void *mem = malloc(10 * 1024 * 1024); // 10 MB
    if (mem == NULL) {
        print_test_result("Memory leak detection", 0);
        return;
    }
    memset(mem, 0, 10 * 1024 * 1024);
    
    sleep(1);
    
    // Segunda leitura
    if (collect_memory_metrics(my_pid, &metrics) != 0) {
        free(mem);
        print_test_result("Memory leak detection", 0);
        return;
    }
    
    double rate2 = detect_memory_leak(&metrics);
    
    // Taxa deve ter aumentado
    print_test_result("Memory leak detection", rate2 > rate1);
    
    free(mem);
}

void test_export_csv(void) {
    const char *test_file = "/tmp/test_metrics.csv";
    pid_t my_pid = getpid();
    
    cpu_metrics_t cpu;
    memory_metrics_t mem;
    io_metrics_t io;
    
    // Coletar métricas
    int has_cpu = (collect_cpu_metrics(my_pid, &cpu) == 0);
    int has_mem = (collect_memory_metrics(my_pid, &mem) == 0);
    int has_io = (collect_io_metrics(my_pid, &io) == 0);
    
    // Exportar
    int result = export_metrics_csv(test_file, my_pid,
                                    has_cpu ? &cpu : NULL,
                                    has_mem ? &mem : NULL,
                                    has_io ? &io : NULL);
    
    print_test_result("export_metrics_csv()", result == 0);
    
    // Verificar se arquivo existe
    if (result == 0) {
        FILE *fp = fopen(test_file, "r");
        print_test_result("CSV file created", fp != NULL);
        if (fp) {
            fclose(fp);
            unlink(test_file);
        }
    }
}

void test_export_json(void) {
    const char *test_file = "/tmp/test_metrics.json";
    pid_t my_pid = getpid();
    
    cpu_metrics_t cpu;
    memory_metrics_t mem;
    io_metrics_t io;
    
    int has_cpu = (collect_cpu_metrics(my_pid, &cpu) == 0);
    int has_mem = (collect_memory_metrics(my_pid, &mem) == 0);
    int has_io = (collect_io_metrics(my_pid, &io) == 0);
    
    int result = export_metrics_json(test_file, my_pid,
                                     has_cpu ? &cpu : NULL,
                                     has_mem ? &mem : NULL,
                                     has_io ? &io : NULL);
    
    print_test_result("export_metrics_json()", result == 0);
    
    if (result == 0) {
        FILE *fp = fopen(test_file, "r");
        print_test_result("JSON file created", fp != NULL);
        if (fp) {
            fclose(fp);
            unlink(test_file);
        }
    }
}

void test_concurrent_monitoring(void) {
    pid_t child = fork();
    
    if (child < 0) {
        print_test_result("Concurrent monitoring", 0);
        return;
    }
    
    if (child == 0) {
        // Processo filho - fazer algum trabalho
        volatile double x = 0;
        for (int i = 0; i < 100000000; i++) {
            x += i * 0.001;
        }
        exit(0);
    }
    
    // Processo pai - monitorar filho
    sleep(1); // Dar tempo para o filho começar
    
    cpu_metrics_t cpu;
    memory_metrics_t mem;
    
    int cpu_ok = (collect_cpu_metrics(child, &cpu) == 0);
    int mem_ok = (collect_memory_metrics(child, &mem) == 0);
    
    wait(NULL); // Aguardar filho terminar
    
    print_test_result("Monitor child process", cpu_ok && mem_ok);
}

void test_long_running_process(void) {
    pid_t my_pid = getpid();
    int success = 1;
    
    printf("\nTesting long-running monitoring (10 samples)...\n");
    
    for (int i = 0; i < 10; i++) {
        cpu_metrics_t cpu;
        memory_metrics_t mem;
        
        if (collect_cpu_metrics(my_pid, &cpu) != 0) {
            success = 0;
            break;
        }
        
        if (collect_memory_metrics(my_pid, &mem) != 0) {
            success = 0;
            break;
        }
        
        printf("  Sample %d: CPU=%.2f%% MEM=%.2f MB\n", 
               i + 1, cpu.cpu_percent, mem.rss / (1024.0 * 1024.0));
        
        sleep(1);
    }
    
    print_test_result("Long-running monitoring stability", success);
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          Resource Monitor - Validation Test Suite         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (geteuid() != 0) {
        printf("%sWarning: Not running as root. Some tests will be skipped.%s\n\n", 
               COLOR_YELLOW, COLOR_RESET);
    }
    
    printf("Running unit tests...\n\n");
    
    test_process_exists();
    test_get_process_name();
    test_cpu_metrics();
    test_memory_metrics();
    test_io_metrics();
    test_cpu_percentage_calculation();
    test_memory_leak_detection();
    test_export_csv();
    test_export_json();
    test_concurrent_monitoring();
    test_long_running_process();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                      Test Summary                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Tests Passed: %s%d%s\n", COLOR_GREEN, tests_passed, COLOR_RESET);
    printf("Tests Failed: %s%d%s\n", tests_failed > 0 ? COLOR_RED : COLOR_RESET, 
           tests_failed, COLOR_RESET);
    printf("Total Tests:  %d\n", tests_passed + tests_failed);
    printf("\n");
    
    if (tests_failed == 0) {
        printf("%s✓ All tests passed!%s\n\n", COLOR_GREEN, COLOR_RESET);
        return EXIT_SUCCESS;
    } else {
        printf("%s✗ Some tests failed.%s\n\n", COLOR_RED, COLOR_RESET);
        return EXIT_FAILURE;
    }
}

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "../include/monitor.h"

#define TEST_FILE "/tmp/io_test_file.dat"
#define BUFFER_SIZE (1024 * 1024)  // 1 MB

/**
 * Realiza operações de I/O para testar o monitor
 */
void perform_io_operations(int num_iterations) {
    char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return;
    }

    memset(buffer, 'A', BUFFER_SIZE);

    printf("Writing %d MB to disk...\n", num_iterations);
    
    // Escrever arquivo
    int fd = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        free(buffer);
        return;
    }

    for (int i = 0; i < num_iterations; i++) {
        if (write(fd, buffer, BUFFER_SIZE) != BUFFER_SIZE) {
            perror("write");
            break;
        }
    }
    
    // Usar fsync em vez de sync
    fsync(fd);
    close(fd);

    printf("Reading %d MB from disk...\n", num_iterations);

    // Ler arquivo
    fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open");
        free(buffer);
        return;
    }

    for (int i = 0; i < num_iterations; i++) {
        if (read(fd, buffer, BUFFER_SIZE) != BUFFER_SIZE) {
            perror("read");
            break;
        }
    }
    close(fd);

    free(buffer);
}

int main(void) {
    printf("=== I/O Monitor Test ===\n\n");

    pid_t my_pid = getpid();
    printf("Monitoring PID: %d\n\n", my_pid);

    io_metrics_t io_metrics;

    // === Teste 1: Leitura inicial ===
    printf("--- Test 1: Initial Reading ---\n");
    
    if (collect_io_metrics(my_pid, &io_metrics) == 0) {
        print_io_metrics(&io_metrics);
    } else {
        fprintf(stderr, "Failed to collect I/O metrics\n");
        fprintf(stderr, "Note: This test requires root permissions (sudo)\n");
        return EXIT_FAILURE;
    }
    printf("\n");

    // === Teste 2: Após operações de I/O ===
    printf("--- Test 2: After I/O Operations ---\n");
    
    perform_io_operations(10);  // 10 MB
    sleep(1);

    if (collect_io_metrics(my_pid, &io_metrics) == 0) {
        print_io_metrics(&io_metrics);
        
        double avg_read, avg_write;
        get_io_efficiency(&io_metrics, &avg_read, &avg_write);
        printf("\nI/O Efficiency:\n");
        printf("  Avg Read Size:    %.2f bytes/syscall\n", avg_read);
        printf("  Avg Write Size:   %.2f bytes/syscall\n", avg_write);
        
        double total_throughput = get_total_io_throughput(&io_metrics);
        printf("  Total Throughput: %.2f MB/s\n", total_throughput / (1024.0 * 1024.0));
    }
    printf("\n");

    // === Teste 3: Monitoramento contínuo ===
    printf("--- Test 3: Continuous Monitoring (5 iterations) ---\n");
    
    for (int i = 0; i < 5; i++) {
        printf("\nIteration %d:\n", i + 1);
        
        // Fazer operações de I/O
        perform_io_operations(5);  // 5 MB
        
        if (collect_io_metrics(my_pid, &io_metrics) == 0) {
            printf("  Read:  %.2f MB (%.2f MB/s)\n",
                   io_metrics.bytes_read / (1024.0 * 1024.0),
                   io_metrics.read_rate / (1024.0 * 1024.0));
            printf("  Write: %.2f MB (%.2f MB/s)\n",
                   io_metrics.bytes_written / (1024.0 * 1024.0),
                   io_metrics.write_rate / (1024.0 * 1024.0));
        }
        
        sleep(1);
    }
    printf("\n");

    // Limpar arquivo de teste
    unlink(TEST_FILE);

    printf("=== All Tests Completed ===\n");
    return EXIT_SUCCESS;
}

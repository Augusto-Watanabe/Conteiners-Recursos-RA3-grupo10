#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

/**
 * @file cpu_workload.c
 * @brief Um programa simples para gerar uma carga de trabalho de CPU consistente.
 *        Usado para medir o overhead de ferramentas de monitoramento.
 */

// Função que consome tempo de CPU de forma previsível.
void perform_calculations(long iterations) {
    volatile double result = 0.0;
    for (long i = 0; i < iterations; ++i) {
        // Operações matemáticas para manter a CPU ocupada.
        result += sin((double)i) * cos((double)i);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <iterations>\n", argv[0]);
        return EXIT_FAILURE;
    }

    long iterations = atol(argv[1]);
    if (iterations <= 0) {
        fprintf(stderr, "Error: Number of iterations must be positive.\n");
        return EXIT_FAILURE;
    }
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    perform_calculations(iterations);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // Print machine-readable output for the experiment script
    printf("WORKLOAD_RESULT:iterations=%ld,time_sec=%.4f\n", iterations, elapsed_sec);
    
    return EXIT_SUCCESS;
}

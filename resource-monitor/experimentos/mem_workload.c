#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * @file mem_workload.c
 * @brief A tool to incrementally allocate memory until failure.
 *        Used for Experiment 4: Memory Limit Enforcement.
 */

#define CHUNK_SIZE (1 * 1024 * 1024) // Allocate in 1 MB chunks

int main(void) {
    long total_allocated = 0;
    int chunk_count = 0;

    printf("Starting incremental memory allocation in 1MB chunks...\n");

    while (1) {
        char *mem = malloc(CHUNK_SIZE);
        if (mem == NULL) {
            printf("\n--- Allocation Failed ---\n");
            printf("malloc() returned NULL. Could not allocate more memory.\n");
            break;
        }

        // Touch the memory to ensure it's actually mapped by the kernel
        memset(mem, chunk_count, CHUNK_SIZE);

        total_allocated += CHUNK_SIZE;
        chunk_count++;

        printf("\rSuccessfully allocated: %ld MB", total_allocated / (1024 * 1024));
        fflush(stdout);

        // Use nanosleep for a 50ms pause (modern replacement for usleep)
        struct timespec sleep_time = {0, 50000000L}; // 50,000,000 nanoseconds = 50ms
        nanosleep(&sleep_time, NULL);
    }

    printf("\n--- Final Result ---\n");
    printf("MAX_ALLOCATED_MB:%ld\n", total_allocated / (1024 * 1024));

    return 0;
}

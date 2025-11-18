#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

/**
 * @file io_workload.c
 * @brief A tool to generate a direct I/O workload.
 *        Used for Experiment 5: I/O Throttling.
 */

#define FILE_SIZE (256 * 1024 * 1024) // 256 MB
#define BLOCK_SIZE (4 * 1024)        // 4 KB block size
#define FILE_PATH "/tmp/io_workload_testfile.tmp"

void cleanup_file() {
    remove(FILE_PATH);
}

int main(void) {
    int fd;
    void *buffer;
    ssize_t bytes_written, bytes_read;
    struct timespec start, end;
    double write_time, read_time;

    // O_DIRECT requires memory aligned buffers
    if (posix_memalign(&buffer, BLOCK_SIZE, BLOCK_SIZE) != 0) {
        perror("posix_memalign");
        return EXIT_FAILURE;
    }
    memset(buffer, 'A', BLOCK_SIZE);

    atexit(cleanup_file);

    // --- Write Phase ---
    fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    if (fd == -1) {
        perror("open for writing");
        free(buffer);
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < FILE_SIZE / BLOCK_SIZE; ++i) {
        bytes_written = write(fd, buffer, BLOCK_SIZE);
        if (bytes_written != BLOCK_SIZE) {
            perror("write");
            close(fd);
            free(buffer);
            return EXIT_FAILURE;
        }
    }
    // Ensure data is written to disk before closing
    fsync(fd);
    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd);

    write_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // --- Read Phase ---
    fd = open(FILE_PATH, O_RDONLY | O_DIRECT);
    if (fd == -1) {
        perror("open for reading");
        free(buffer);
        return EXIT_FAILURE;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (size_t i = 0; i < FILE_SIZE / BLOCK_SIZE; ++i) {
        bytes_read = read(fd, buffer, BLOCK_SIZE);
        if (bytes_read != BLOCK_SIZE) {
            perror("read");
            close(fd);
            free(buffer);
            return EXIT_FAILURE;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd);

    read_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    free(buffer);

    // --- Report Results ---
    double write_throughput_mbps = (FILE_SIZE / (1024.0 * 1024.0)) / write_time;
    double read_throughput_mbps = (FILE_SIZE / (1024.0 * 1024.0)) / read_time;

    // Print machine-readable output for the experiment script
    printf("WORKLOAD_RESULT:write_mbps=%.2f,read_mbps=%.2f,write_time=%.4f,read_time=%.4f\n",
           write_throughput_mbps, read_throughput_mbps, write_time, read_time);

    return EXIT_SUCCESS;
}

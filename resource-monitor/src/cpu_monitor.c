#define _POSIX_C_SOURCE 199309L

#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

typedef struct {
    uint64_t last_utime;
    uint64_t last_stime;
    uint64_t last_total_time;
    struct timespec last_timestamp;
    int initialized;
} cpu_state_t;

static cpu_state_t cpu_state = {0};

int collect_cpu_metrics(pid_t pid, cpu_metrics_t *metrics) {
    if (metrics == NULL) {
        fprintf(stderr, "Error: metrics pointer is NULL\n");
        errno = EINVAL;
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        return -1;
    }

    char line[4096];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "Error reading %s: %s\n", path, strerror(errno));
        fclose(fp);
        return -1;
    }
    fclose(fp);

    int parse_pid;
    char state;
    unsigned long utime, stime, cutime, cstime;
    long num_threads;
    unsigned long long starttime;
    
    char *comm_end = strrchr(line, ')');
    if (comm_end == NULL) {
        fprintf(stderr, "Error: malformed stat file\n");
        errno = EINVAL;
        return -1;
    }

    if (sscanf(line, "%d", &parse_pid) != 1) {
        fprintf(stderr, "Error parsing PID\n");
        errno = EINVAL;
        return -1;
    }

    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned int flags;
    unsigned long minflt, cminflt, majflt, cmajflt;
    long priority, nice, itrealvalue;
    
    int parsed = sscanf(comm_end + 2,
        "%c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %llu",
        &state,
        &ppid, &pgrp, &session, &tty_nr, &tpgid,
        &flags,
        &minflt, &cminflt, &majflt, &cmajflt,
        &utime, &stime,
        &cutime, &cstime,
        &priority, &nice,
        &num_threads,
        &itrealvalue,
        &starttime
    );

    if (parsed != 20) {
        fprintf(stderr, "Error: failed to parse stat fields (got %d/20)\n", parsed);
        errno = EINVAL;
        return -1;
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) {
        fprintf(stderr, "Error: invalid clock ticks\n");
        errno = EINVAL;
        return -1;
    }

    metrics->user_time = utime;
    metrics->system_time = stime;
    metrics->total_time = utime + stime;
    metrics->num_threads = (uint32_t)num_threads;

    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp != NULL) {
        uint64_t voluntary_ctxt_switches = 0;
        uint64_t nonvoluntary_ctxt_switches = 0;

        while (fgets(line, sizeof(line), fp) != NULL) {
            if (sscanf(line, "voluntary_ctxt_switches: %lu", &voluntary_ctxt_switches) == 1) {
                continue;
            }
            if (sscanf(line, "nonvoluntary_ctxt_switches: %lu", &nonvoluntary_ctxt_switches) == 1) {
                continue;
            }
        }
        fclose(fp);

        metrics->context_switches = voluntary_ctxt_switches + nonvoluntary_ctxt_switches;
    } else {
        metrics->context_switches = 0;
    }

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    if (cpu_state.initialized) {
        double elapsed_time = (current_time.tv_sec - cpu_state.last_timestamp.tv_sec) +
                             (current_time.tv_nsec - cpu_state.last_timestamp.tv_nsec) / 1e9;

        uint64_t delta_time = metrics->total_time - cpu_state.last_total_time;
        double delta_seconds = (double)delta_time / ticks_per_sec;

        if (elapsed_time > 0) {
            metrics->cpu_percent = (delta_seconds / elapsed_time) * 100.0;
        } else {
            metrics->cpu_percent = 0.0;
        }
    } else {
        metrics->cpu_percent = 0.0;
    }

    cpu_state.last_utime = metrics->user_time;
    cpu_state.last_stime = metrics->system_time;
    cpu_state.last_total_time = metrics->total_time;
    cpu_state.last_timestamp = current_time;
    cpu_state.initialized = 1;

    return 0;
}

void reset_cpu_monitor(void) {
    memset(&cpu_state, 0, sizeof(cpu_state));
}

uint64_t ticks_to_microseconds(uint64_t ticks) {
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) {
        return 0;
    }
    return (ticks * 1000000ULL) / ticks_per_sec;
}

void print_cpu_metrics(const cpu_metrics_t *metrics) {
    if (metrics == NULL) {
        return;
    }

    printf("CPU Metrics:\n");
    printf("  User Time:        %lu ticks (%.2f seconds)\n", 
           metrics->user_time,
           (double)metrics->user_time / sysconf(_SC_CLK_TCK));
    printf("  System Time:      %lu ticks (%.2f seconds)\n",
           metrics->system_time,
           (double)metrics->system_time / sysconf(_SC_CLK_TCK));
    printf("  Total Time:       %lu ticks (%.2f seconds)\n",
           metrics->total_time,
           (double)metrics->total_time / sysconf(_SC_CLK_TCK));
    printf("  Threads:          %u\n", metrics->num_threads);
    printf("  Context Switches: %lu\n", metrics->context_switches);
    printf("  CPU Usage:        %.2f%%\n", metrics->cpu_percent);
}

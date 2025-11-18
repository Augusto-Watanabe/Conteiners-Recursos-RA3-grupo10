#define _GNU_SOURCE
#include "cgroup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Gera relatório de utilização vs limites
 */
int generate_cgroup_utilization_report(pid_t pid, const char *output_file) {
    FILE *fp = stdout;
    
    if (output_file != NULL) {
        fp = fopen(output_file, "w");
        if (fp == NULL) {
            return -1;
        }
    }
    
    // Header
    time_t now = time(NULL);
    fprintf(fp, "╔════════════════════════════════════════════════════════════╗\n");
    fprintf(fp, "║         Cgroup Utilization Report                         ║\n");
    fprintf(fp, "╚════════════════════════════════════════════════════════════╝\n");
    fprintf(fp, "\nGenerated: %s", ctime(&now));
    fprintf(fp, "Process ID: %d\n\n", pid);
    
    // Read metrics
    cgroup_metrics_t metrics;
    if (read_cgroup_metrics(pid, &metrics) != 0) {
        fprintf(fp, "Error: Could not read cgroup metrics\n");
        if (output_file != NULL) fclose(fp);
        return -1;
    }
    
    // CPU Report
    if (metrics.has_cpu) {
        fprintf(fp, "CPU Resource Usage:\n");
        fprintf(fp, "───────────────────────────────────────\n");
        fprintf(fp, "  Total Usage:     %.2f seconds\n", 
                metrics.cpu.usage_usec / 1000000.0);
        fprintf(fp, "  User Mode:       %.2f seconds\n",
                metrics.cpu.user_usec / 1000000.0);
        fprintf(fp, "  System Mode:     %.2f seconds\n",
                metrics.cpu.system_usec / 1000000.0);
        
        if (metrics.cpu.quota > 0) {
            double limit_cores = (double)metrics.cpu.quota / metrics.cpu.period;
            fprintf(fp, "\n  Configured Limit: %.2f cores\n", limit_cores);
            
            if (metrics.cpu.nr_periods > 0) {
                double throttle_pct = (metrics.cpu.nr_throttled * 100.0) / metrics.cpu.nr_periods;
                fprintf(fp, "  Throttling:      %.2f%% (%lu/%lu periods)\n",
                        throttle_pct, metrics.cpu.nr_throttled, metrics.cpu.nr_periods);
                fprintf(fp, "  Throttle Time:   %.2f seconds\n",
                        metrics.cpu.throttled_usec / 1000000.0);
                
                if (throttle_pct > 50) {
                    fprintf(fp, "  ⚠ WARNING: Heavy throttling detected!\n");
                }
            }
        } else {
            fprintf(fp, "\n  Configured Limit: Unlimited\n");
        }
        fprintf(fp, "\n");
    }
    
    // Memory Report
    if (metrics.has_memory) {
        fprintf(fp, "Memory Resource Usage:\n");
        fprintf(fp, "───────────────────────────────────────\n");
        fprintf(fp, "  Current:         %.2f MB\n",
                metrics.memory.current / (1024.0 * 1024.0));
        fprintf(fp, "  Peak:            %.2f MB\n",
                metrics.memory.peak / (1024.0 * 1024.0));
        fprintf(fp, "  RSS:             %.2f MB\n",
                metrics.memory.rss / (1024.0 * 1024.0));
        fprintf(fp, "  Cache:           %.2f MB\n",
                metrics.memory.cache / (1024.0 * 1024.0));
        
        if (metrics.memory.limit < UINT64_MAX) {
            double usage_pct = (metrics.memory.current * 100.0) / metrics.memory.limit;
            fprintf(fp, "\n  Configured Limit: %.2f MB\n",
                    metrics.memory.limit / (1024.0 * 1024.0));
            fprintf(fp, "  Usage:           %.2f%%\n", usage_pct);
            
            if (usage_pct > 90) {
                fprintf(fp, "  ⚠ WARNING: Near memory limit!\n");
            }
            
            if (metrics.memory.pgmajfault > 100) {
                fprintf(fp, "  ⚠ WARNING: High major page faults (%lu)\n",
                        metrics.memory.pgmajfault);
            }
        } else {
            fprintf(fp, "\n  Configured Limit: Unlimited\n");
        }
        fprintf(fp, "\n");
    }
    
    // I/O Report
    if (metrics.has_blkio) {
        fprintf(fp, "Block I/O Usage:\n");
        fprintf(fp, "───────────────────────────────────────\n");
        fprintf(fp, "  Total Read:      %.2f MB (%lu ops)\n",
                metrics.blkio.rbytes / (1024.0 * 1024.0),
                metrics.blkio.rios);
        fprintf(fp, "  Total Write:     %.2f MB (%lu ops)\n",
                metrics.blkio.wbytes / (1024.0 * 1024.0),
                metrics.blkio.wios);
        
        if (metrics.blkio.rios > 0) {
            fprintf(fp, "  Avg Read Size:   %.2f KB\n",
                    (metrics.blkio.rbytes / (double)metrics.blkio.rios) / 1024.0);
        }
        if (metrics.blkio.wios > 0) {
            fprintf(fp, "  Avg Write Size:  %.2f KB\n",
                    (metrics.blkio.wbytes / (double)metrics.blkio.wios) / 1024.0);
        }
        fprintf(fp, "\n");
    }
    
    // PIDs Report
    if (metrics.has_pids) {
        fprintf(fp, "Process Limits:\n");
        fprintf(fp, "───────────────────────────────────────\n");
        fprintf(fp, "  Current PIDs:    %lu\n", metrics.pids.current);
        if (metrics.pids.limit < UINT64_MAX) {
            fprintf(fp, "  PID Limit:       %lu\n", metrics.pids.limit);
            fprintf(fp, "  Usage:           %.2f%%\n",
                    (metrics.pids.current * 100.0) / metrics.pids.limit);
        } else {
            fprintf(fp, "  PID Limit:       Unlimited\n");
        }
        fprintf(fp, "\n");
    }
    
    // Summary
    fprintf(fp, "Summary:\n");
    fprintf(fp, "───────────────────────────────────────\n");
    fprintf(fp, "  Cgroup Path:     %s\n", metrics.info.path);
    fprintf(fp, "  Cgroup Version:  v%d\n", metrics.info.version);
    fprintf(fp, "  Controllers:     ");
    if (metrics.has_cpu) fprintf(fp, "CPU ");
    if (metrics.has_memory) fprintf(fp, "Memory ");
    if (metrics.has_blkio) fprintf(fp, "BlkIO ");
    if (metrics.has_pids) fprintf(fp, "PIDs ");
    fprintf(fp, "\n\n");
    
    if (output_file != NULL) {
        fclose(fp);
    }
    
    return 0;
}

/**
 * Compara utilização de múltiplos processos
 */
int compare_cgroup_utilization(pid_t *pids, int count, const char *output_file) {
    FILE *fp = stdout;
    
    if (output_file != NULL) {
        fp = fopen(output_file, "w");
        if (fp == NULL) {
            return -1;
        }
    }
    
    fprintf(fp, "╔════════════════════════════════════════════════════════════╗\n");
    fprintf(fp, "║      Cgroup Utilization Comparison                        ║\n");
    fprintf(fp, "╚════════════════════════════════════════════════════════════╝\n\n");
    
    fprintf(fp, "Comparing %d processes:\n\n", count);
    
    // Table header
    fprintf(fp, "┌─────────┬──────────────┬──────────────┬──────────────┐\n");
    fprintf(fp, "│   PID   │  CPU (sec)   │  Memory (MB) │   I/O (MB)   │\n");
    fprintf(fp, "├─────────┼──────────────┼──────────────┼──────────────┤\n");
    
    double total_cpu = 0, total_mem = 0, total_io = 0;
    
    for (int i = 0; i < count; i++) {
        cgroup_metrics_t m;
        if (read_cgroup_metrics(pids[i], &m) == 0) {
            double cpu = m.has_cpu ? m.cpu.usage_usec / 1000000.0 : 0;
            double mem = m.has_memory ? m.memory.current / (1024.0 * 1024.0) : 0;
            double io = m.has_blkio ? (m.blkio.rbytes + m.blkio.wbytes) / (1024.0 * 1024.0) : 0;
            
            fprintf(fp, "│ %7d │ %12.2f │ %12.2f │ %12.2f │\n",
                    pids[i], cpu, mem, io);
            
            total_cpu += cpu;
            total_mem += mem;
            total_io += io;
        }
    }
    
    fprintf(fp, "├─────────┼──────────────┼──────────────┼──────────────┤\n");
    fprintf(fp, "│  TOTAL  │ %12.2f │ %12.2f │ %12.2f │\n",
            total_cpu, total_mem, total_io);
    fprintf(fp, "└─────────┴──────────────┴──────────────┴──────────────┘\n\n");
    
    if (output_file != NULL) {
        fclose(fp);
    }
    
    return 0;
}

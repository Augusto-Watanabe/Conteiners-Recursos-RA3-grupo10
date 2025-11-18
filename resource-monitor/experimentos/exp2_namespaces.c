#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <time.h>

/**
 * @file exp2_namespaces.c
 * @brief A tool to create new namespaces and verify resource visibility.
 *        Used for Experiment 2: Namespace Isolation.
 */

void print_header(const char *title) {
    printf("\n--- %s ---\n", title);
}

int child_main(void *arg) {
    char **argv = (char **)arg;
    int unshare_flags = 0;
    int do_pid_ns = 0, do_net_ns = 0, do_mnt_ns = 0;

    // Parse arguments to determine which namespaces to create
    for (int i = 1; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "--pid") == 0) {
            unshare_flags |= CLONE_NEWPID;
            do_pid_ns = 1;
        } else if (strcmp(argv[i], "--net") == 0) {
            unshare_flags |= CLONE_NEWNET;
            do_net_ns = 1;
        } else if (strcmp(argv[i], "--mnt") == 0) {
            unshare_flags |= CLONE_NEWNS;
            do_mnt_ns = 1;
        }
    }

    // Unshare from parent's namespaces
    if (unshare(unshare_flags) == -1) {
        perror("unshare");
        return 1;
    }

    // --- Verification Steps ---

    if (do_pid_ns) {
        print_header("Verifying PID Namespace");
        // To see the effect of a new PID namespace, we must fork again.
        // The new child will be PID 1 in the namespace.
        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            return 1;
        }
        if (child_pid == 0) {
            // Remount /proc to see the new PID tree
            if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
                perror("mount /proc");
            }
            printf("Inside new PID namespace. My PID is %d. Running 'ps aux':\n", getpid());
            system("ps aux");
            umount("/proc");
            exit(0);
        }
        waitpid(child_pid, NULL, 0);
    }

    if (do_net_ns) {
        print_header("Verifying Network Namespace");
        printf("Bringing up loopback interface...\n");
        system("ip link set lo up");
        printf("Running 'ip addr':\n");
        system("ip addr");
    }

    if (do_mnt_ns) {
        print_header("Verifying Mount Namespace");
        printf("Current mounts:\n");
        system("findmnt -n -o SOURCE,TARGET,FSTYPE | head -n 5");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc; // Mark as unused to prevent compiler error

    if (geteuid() != 0) {
        fprintf(stderr, "This program requires root privileges to create namespaces.\n");
        return EXIT_FAILURE;
    }

    char *stack = malloc(1024 * 1024); // 1MB stack for the new process
    if (stack == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    char *stack_top = stack + (1024 * 1024);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Use clone to create a new process in new namespaces
    pid_t pid = clone(child_main, stack_top, SIGCHLD, argv);
    if (pid == -1) {
        perror("clone");
        free(stack);
        return EXIT_FAILURE;
    }

    waitpid(pid, NULL, 0);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    printf("\n--- Measurement ---\n");
    printf("creation_time_ms:%.4f\n", elapsed_ms);

    free(stack);
    return 0;
}

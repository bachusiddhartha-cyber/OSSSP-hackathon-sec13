
/*
 Project: Linux Command Execution Time Profiler

 CO-1: System Calls and Linux Command Execution
 CO-2: Process Creation, Lifecycle and Termination
 CO-3: Signals and Process Termination
 CO-4: Memory Management and Resource Usage
 CO-5: File I/O and File Descriptors
 CO-6: Concurrency and Synchronization
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100

void save_report(const char *command, double elapsed, int status)
{
    FILE *file = fopen("execution_report.csv", "a");

    if (file == NULL)
    {
        perror("File error");
        return;
    }

    fprintf(file, "\"%s\",%.6f,%d\n",
            command, elapsed, status);

    fclose(file);
}

double get_elapsed_time(struct timespec start,
                        struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

void show_memory_usage(void)
{
    struct rusage usage;

    if (getrusage(RUSAGE_CHILDREN, &usage) == 0)
    {
        printf("Peak Child Memory: %ld KB\n",
               usage.ru_maxrss);
    }
}

void execute_command(char *args[])
{
    struct timespec start, end;
    int status;

    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        return;
    }

    if (pid == 0)
    {
        execvp(args[0], args);

        perror("Command execution failed");
        _exit(127);
    }
    else
    {
        pid_t result;

        do
        {
            result = waitpid(pid, &status, 0);
        }
        while (result == -1 && errno == EINTR);

        if (result == -1)
        {
            perror("waitpid failed");
            return;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        double elapsed = get_elapsed_time(start, end);

        printf("\n========== EXECUTION REPORT ==========\n");

        printf("Command       : %s", args[0]);

        for (int i = 1; args[i] != NULL; i++)
            printf(" %s", args[i]);

        printf("\nProcess ID    : %ld\n", (long)pid);
        printf("Execution Time: %.6f seconds\n", elapsed);

        if (WIFEXITED(status))
        {
            int code = WEXITSTATUS(status);

            printf("Exit Status   : %d\n", code);
            printf("Result        : %s\n",
                   code == 0 ? "SUCCESS" : "FAILED");
        }
        else if (WIFSIGNALED(status))
        {
            printf("Terminated by Signal: %d\n",
                   WTERMSIG(status));
        }

        show_memory_usage();

        printf("======================================\n");

        int code = WIFEXITED(status)
                       ? WEXITSTATUS(status)
                       : -1;

        save_report(args[0], elapsed, code);
    }
}

/* CO-3: Demonstrate signal-based process termination */
void signal_demo(void)
{
    printf("\nStarting child process...\n");

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        return;
    }

    if (pid == 0)
    {
        execlp("sleep", "sleep", "10", (char *)NULL);
        perror("exec failed");
        _exit(127);
    }

    struct timespec delay = {1, 0};

    while (nanosleep(&delay, &delay) == -1 &&
           errno == EINTR)
    {
    }

    if (kill(pid, SIGTERM) == -1)
        perror("kill failed");

    int status;

    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid failed");
        return;
    }

    printf("Child PID: %ld\n", (long)pid);

    if (WIFSIGNALED(status))
        printf("Child terminated by signal %d (SIGTERM).\n",
               WTERMSIG(status));
}

/* CO-6: Run two child processes concurrently */
void parallel_demo(void)
{
    struct timespec start, end;
    pid_t pids[2];
    int status;

    printf("\nStarting concurrent processes...\n");

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 2; i++)
    {
        pids[i] = fork();

        if (pids[i] < 0)
        {
            perror("fork failed");

            for (int j = 0; j < i; j++)
            {
                kill(pids[j], SIGTERM);
                waitpid(pids[j], NULL, 0);
            }

            return;
        }

        if (pids[i] == 0)
        {
            if (i == 0)
                execlp("sleep", "sleep", "2", (char *)NULL);
            else
                execlp("sleep", "sleep", "3", (char *)NULL);

            perror("exec failed");
            _exit(127);
        }
    }

    for (int i = 0; i < 2; i++)
    {
        pid_t result;

        do
        {
            result = waitpid(pids[i], &status, 0);
        }
        while (result == -1 && errno == EINTR);

        if (result == -1)
        {
            perror("waitpid failed");
            return;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = get_elapsed_time(start, end);

    printf("Concurrent processes completed.\n");
    printf("Total execution time: %.6f seconds\n", elapsed);
    printf("Parent synchronized using waitpid().\n");

    save_report("parallel sleep 2 and sleep 3",
                elapsed, 0);
}

int main(void)
{
    char input[MAX_INPUT];

    FILE *file = fopen("execution_report.csv", "a");

    if (file != NULL)
    {
        if (ftell(file) == 0)
            fprintf(file, "Command,Time_Seconds,Exit_Status\n");

        fclose(file);
    }

    printf("\n============================================\n");
    printf(" LINUX COMMAND EXECUTION TIME PROFILER\n");
    printf("============================================\n");

    printf("Enter Linux commands or:\n");
    printf("parallel    - Demonstrate concurrency\n");
    printf("signal-demo - Demonstrate signals\n");
    printf("exit        - Close profiler\n");

    while (1)
    {
        char *args[MAX_ARGS];
        int count = 0;

        printf("\nprofiler> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        input[strcspn(input, "\n")] = '\0';

        if (input[0] == '\0')
            continue;

        if (strcmp(input, "exit") == 0)
            break;

        if (strcmp(input, "parallel") == 0)
        {
            parallel_demo();
            continue;
        }

        if (strcmp(input, "signal-demo") == 0)
        {
            signal_demo();
            continue;
        }

        char *token = strtok(input, " \t");

        while (token != NULL && count < MAX_ARGS - 1)
        {
            args[count++] = token;
            token = strtok(NULL, " \t");
        }

        if (token != NULL)
        {
            printf("Too many arguments.\n");
            continue;
        }

        args[count] = NULL;

        if (count > 0)
            execute_command(args);
    }

    printf("\nProfiler closed.\n");
    printf("Reports saved in execution_report.csv\n");

    return 0;
}

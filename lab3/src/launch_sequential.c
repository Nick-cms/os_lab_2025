#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <seed> <array_size>\n", argv[0]);
        printf("Example: %s 42 1000000\n", argv[0]);
        return 1;
    }

    int seed = atoi(argv[1]);
    int array_size = atoi(argv[2]);

    if (seed <= 0 || array_size <= 0) {
        printf("Error: seed and array_size must be positive numbers\n");
        return 1;
    }

    printf("Launching sequential_min_max in separate process...\n");
    printf("Seed: %d, Array size: %d\n", seed, array_size);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork failed");
        return 1;
    }
    else if (pid == 0) {
        // Дочерний процесс - запускаем sequential_min_max
        char seed_str[16], size_str[16];
        sprintf(seed_str, "%d", seed);
        sprintf(size_str, "%d", array_size);
        
        char *args[] = {"./sequential_min_max", seed_str, size_str, NULL};
        execvp(args[0], args);
        
        // Если execvp вернул управление, значит произошла ошибка
        perror("execvp failed");
        exit(1);
    }
    else {
        // Родительский процесс - ждем завершения дочернего
        int status;
        waitpid(pid, &status, 0);
        
        gettimeofday(&end_time, NULL);
        
        double total_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
        total_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        
        if (WIFEXITED(status)) {
            printf("\nChild process exited with status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("\nChild process terminated by signal: %d\n", WTERMSIG(status));
        }
        
        printf("Total execution time (including process creation): %.2fms\n", total_time);
    }
    
    return 0;
}
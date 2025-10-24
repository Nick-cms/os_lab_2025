#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

// Глобальные переменные для обработки сигнала
volatile sig_atomic_t timeout_reached = 0;
pid_t *child_pids = NULL;

// Обработчик сигнала SIGALRM
void handle_alarm(int sig) {
    timeout_reached = 1;
}

int main(int argc, char **argv) {
    int seed = -1;
    int array_size = -1;
    int pnum = -1;
    int timeout = 0; // 0 означает отсутствие таймаута
    bool with_files = false;

    while (true) {
        int current_optind = optind ? optind : 1;

        static struct option options[] = {
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'},
            {"timeout", required_argument, 0, 't'}, // Добавляем опцию timeout
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "ft:", options, &option_index); // Добавляем 't:' для timeout

        if (c == -1) break;

        switch (c) {
            case 0:
                switch (option_index) {
                    case 0:
                        seed = atoi(optarg);
                        if (seed <= 0) {
                            printf("seed must be a positive number\n");
                            return 1;
                        }
                        break;
                    case 1:
                        array_size = atoi(optarg);
                        if (array_size <= 0) {
                            printf("array_size must be a positive number\n");
                            return 1;
                        }
                        break;
                    case 2:
                        pnum = atoi(optarg);
                        if (pnum <= 0) {
                            printf("pnum must be a positive number\n");
                            return 1;
                        }
                        break;
                    case 3:
                        with_files = true;
                        break;
                    default:
                        printf("Index %d is out of options\n", option_index);
                }
                break;
            case 'f':
                with_files = true;
                break;
            case 't': // Обработка таймаута
                timeout = atoi(optarg);
                if (timeout <= 0) {
                    printf("timeout must be a positive number\n");
                    return 1;
                }
                break;
            case '?':
                break;
            default:
                printf("getopt returned character code 0%o?\n", c);
        }
    }

    if (optind < argc) {
        printf("Has at least one no option argument\n");
        return 1;
    }

    if (seed == -1 || array_size == -1 || pnum == -1) {
        printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files] [--timeout \"seconds\"]\n", argv[0]);
        return 1;
    }

    // Устанавливаем обработчик сигнала SIGALRM
    signal(SIGALRM, handle_alarm);

    // Выделяем память для хранения PID дочерних процессов
    child_pids = malloc(pnum * sizeof(pid_t));
    if (child_pids == NULL) {
        perror("malloc");
        return 1;
    }

    int *array = malloc(sizeof(int) * array_size);
    GenerateArray(array, array_size, seed);

    int (*pipe_fds)[2] = NULL;
    if (!with_files) {
        pipe_fds = malloc(pnum * sizeof(int[2]));
        for (int i = 0; i < pnum; i++) {
            if (pipe(pipe_fds[i]) == -1) {
                perror("pipe");
                free(child_pids);
                return 1;
            }
        }
    }

    int active_child_processes = 0;
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    // Запускаем таймер, если задан таймаут
    if (timeout > 0) {
        alarm(timeout);
    }

    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork();
        if (child_pid >= 0) {
            if (child_pid == 0) {
                // Дочерний процесс - сбрасываем обработчик сигнала
                signal(SIGALRM, SIG_DFL);

                if (!with_files) {
                    for (int j = 0; j < pnum; j++) {
                        if (j != i) {
                            close(pipe_fds[j][0]);
                            close(pipe_fds[j][1]);
                        }
                    }
                    close(pipe_fds[i][0]);
                }

                int block_size = array_size / pnum;
                int begin = i * block_size;
                int end = (i == pnum - 1) ? array_size : (i + 1) * block_size;

                struct MinMax local_min_max = GetMinMax(array, begin, end);
                sleep(5);

                if (with_files) {
                    char filename[32];
                    sprintf(filename, "min_max_%d.txt", i);
                    FILE *file = fopen(filename, "w");
                    if (file != NULL) {
                        fprintf(file, "%d %d", local_min_max.min, local_min_max.max);
                        fclose(file);
                    }
                } else {
                    write(pipe_fds[i][1], &local_min_max.min, sizeof(int));
                    write(pipe_fds[i][1], &local_min_max.max, sizeof(int));
                    close(pipe_fds[i][1]);
                }

                free(array);
                exit(0);
            } else {
                // Родительский процесс
                child_pids[active_child_processes] = child_pid;
                active_child_processes++;
                if (!with_files) {
                    close(pipe_fds[i][1]);
                }
            }
        } else {
            printf("Fork failed!\n");
            free(child_pids);
            return 1;
        }
    }

    // Ожидаем завершения дочерних процессов с возможностью таймаута
    while (active_child_processes > 0) {
        if (timeout_reached) {
            printf("Timeout reached! Sending SIGKILL to all child processes.\n");
            for (int i = 0; i < active_child_processes; i++) {
                kill(child_pids[i], SIGKILL);
            }
            break;
        }
        
        int status;
        pid_t finished_pid = waitpid(-1, &status, WNOHANG);
        
        if (finished_pid > 0) {
            // Найдем и удалим завершенный процесс из массива
            for (int i = 0; i < active_child_processes; i++) {
                if (child_pids[i] == finished_pid) {
                    // Сдвигаем оставшиеся элементы
                    for (int j = i; j < active_child_processes - 1; j++) {
                        child_pids[j] = child_pids[j + 1];
                    }
                    break;
                }
            }
            active_child_processes--;
        } else if (finished_pid == 0) {
            // Нет завершенных процессов, ждем немного
            usleep(10000); // 10ms
        } else {
            // Ошибка
            perror("waitpid");
            break;
        }
    }

    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    // Собираем результаты только от завершившихся процессов
    for (int i = 0; i < pnum; i++) {
        int min = INT_MAX;
        int max = INT_MIN;
        bool result_available = true;

        // Проверяем, был ли процесс завершен по таймауту
        if (timeout_reached) {
            // Проверяем, существует ли еще процесс
            if (kill(child_pids[i], 0) == -1) {
                // Процесс не существует, пропускаем
                continue;
            }
        }

        if (with_files) {
            char filename[32];
            sprintf(filename, "min_max_%d.txt", i);
            FILE *file = fopen(filename, "r");
            if (file != NULL) {
                fscanf(file, "%d %d", &min, &max);
                fclose(file);
                remove(filename);
            } else {
                result_available = false;
            }
        } else {
            // Для pipe попробуем прочитать, но если процесс был убит, это может не сработать
            ssize_t bytes_read_min = read(pipe_fds[i][0], &min, sizeof(int));
            ssize_t bytes_read_max = read(pipe_fds[i][0], &max, sizeof(int));
            if (bytes_read_min != sizeof(int) || bytes_read_max != sizeof(int)) {
                result_available = false;
            }
            close(pipe_fds[i][0]);
        }

        if (result_available) {
            if (min < min_max.min) min_max.min = min;
            if (max > min_max.max) min_max.max = max;
        }
    }

    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);

    double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
    elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    if (!with_files) {
        free(pipe_fds);
    }
    free(array);
    free(child_pids);

    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
    printf("Elapsed time: %fms\n", elapsed_time);
    
    if (timeout_reached) {
        printf("Warning: Some processes were terminated due to timeout.\n");
    }
    
    fflush(NULL);
    return 0;
}
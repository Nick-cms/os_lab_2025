#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
    int seed = -1;
    int array_size = -1;
    int pnum = -1;
    bool with_files = false;

    while (true) {
        int current_optind = optind ? optind : 1;

        static struct option options[] = {
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "f", options, &option_index);

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
        printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files]\n",
               argv[0]);
        return 1;
    }

    int *array = malloc(sizeof(int) * array_size);
    GenerateArray(array, array_size, seed);

    // Подготовка структур для pipe (если используется)
    int (*pipe_fds)[2] = NULL;
    if (!with_files) {
        pipe_fds = malloc(pnum * sizeof(int[2]));
        for (int i = 0; i < pnum; i++) {
            if (pipe(pipe_fds[i]) == -1) {
                perror("pipe");
                return 1;
            }
        }
    }

    int active_child_processes = 0;
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    // Создание дочерних процессов
    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork();
        if (child_pid >= 0) {
            if (child_pid == 0) {
                // Дочерний процесс
                if (!with_files) {
                    // Закрываем ненужные дескрипторы pipe
                    for (int j = 0; j < pnum; j++) {
                        if (j != i) {
                            close(pipe_fds[j][0]);
                            close(pipe_fds[j][1]);
                        }
                    }
                    close(pipe_fds[i][0]); // Закрываем читающий конец
                }

                // Вычисляем границы части массива для этого процесса
                int block_size = array_size / pnum;
                int begin = i * block_size;
                int end = (i == pnum - 1) ? array_size : (i + 1) * block_size;

                // Ищем min и max в своей части массива
                struct MinMax local_min_max = GetMinMax(array, begin, end);

                if (with_files) {
                    // Используем файлы для передачи результатов
                    char filename[32];
                    sprintf(filename, "min_max_%d.txt", i);
                    FILE *file = fopen(filename, "w");
                    if (file != NULL) {
                        fprintf(file, "%d %d", local_min_max.min, local_min_max.max);
                        fclose(file);
                    }
                } else {
                    // Используем pipe для передачи результатов
                    write(pipe_fds[i][1], &local_min_max.min, sizeof(int));
                    write(pipe_fds[i][1], &local_min_max.max, sizeof(int));
                    close(pipe_fds[i][1]);
                }

                free(array);
                exit(0);
            } else {
                // Родительский процесс
                active_child_processes++;
                if (!with_files) {
                    close(pipe_fds[i][1]); // Закрываем записывающий конец в родителе
                }
            }
        } else {
            printf("Fork failed!\n");
            return 1;
        }
    }

    // Ожидание завершения всех дочерних процессов
    while (active_child_processes > 0) {
        wait(NULL);
        active_child_processes--;
    }

    // Сбор и объединение результатов
    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    for (int i = 0; i < pnum; i++) {
        int min = INT_MAX;
        int max = INT_MIN;

        if (with_files) {
            // Чтение из файлов
            char filename[32];
            sprintf(filename, "min_max_%d.txt", i);
            FILE *file = fopen(filename, "r");
            if (file != NULL) {
                fscanf(file, "%d %d", &min, &max);
                fclose(file);
                remove(filename); // Удаляем временный файл
            }
        } else {
            // Чтение из pipe
            read(pipe_fds[i][0], &min, sizeof(int));
            read(pipe_fds[i][0], &max, sizeof(int));
            close(pipe_fds[i][0]);
        }

        if (min < min_max.min) min_max.min = min;
        if (max > min_max.max) min_max.max = max;
    }

    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);

    double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
    elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    if (!with_files) {
        free(pipe_fds);
    }
    free(array);

    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
    printf("Elapsed time: %fms\n", elapsed_time);
    fflush(NULL);
    return 0;
}
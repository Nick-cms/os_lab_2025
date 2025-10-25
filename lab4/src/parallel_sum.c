#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "utils.h"

void GenerateArray(int *array, unsigned int array_size, unsigned int seed);

struct SumArgs {
  int *array;
  int begin;
  int end;
};

int Sum(const struct SumArgs *args) {
  int sum = 0;
  for (int i = args->begin; i < args->end; i++) {
    sum += args->array[i];
  }
  return sum;
}

void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  return (void *)(size_t)Sum(sum_args);
}

void ParseArguments(int argc, char **argv, uint32_t *threads_num, uint32_t *array_size, uint32_t *seed) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--threads_num") == 0 && i + 1 < argc) {
      *threads_num = atoi(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "--array_size") == 0 && i + 1 < argc) {
      *array_size = atoi(argv[i + 1]);
      i++;
    } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      *seed = atoi(argv[i + 1]);
      i++;
    }
  }
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;
  
  // Парсинг аргументов командной строки
  ParseArguments(argc, argv, &threads_num, &array_size, &seed);
  
  // Проверка корректности аргументов
  if (threads_num == 0 || array_size == 0) {
    printf("Usage: %s --threads_num <num> --array_size <size> --seed <seed>\n", argv[0]);
    return 1;
  }
  
  printf("Threads: %u, Array Size: %u, Seed: %u\n", threads_num, array_size, seed);
  
  // Создание массива потоков
  pthread_t threads[threads_num];
  
  // Генерация массива
  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
    printf("Error: Memory allocation failed!\n");
    return 1;
  }
  
  GenerateArray(array, array_size, seed);
  
  // Вычисление размера блока для каждого потока
  int block_size = array_size / threads_num;
  
  // Создание аргументов для потоков
  struct SumArgs args[threads_num];
  for (uint32_t i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = i * block_size;
    args[i].end = (i == threads_num - 1) ? array_size : (i + 1) * block_size;
  }
  
  // Создание потоков
  for (uint32_t i = 0; i < threads_num; i++) {
    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      free(array);
      return 1;
    }
  }

  // Ожидание завершения потоков и суммирование результатов
  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    int sum = 0;
    pthread_join(threads[i], (void **)&sum);
    total_sum += sum;
  }

  free(array);
  printf("Total Sum: %d\n", total_sum);
  return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int start;
    int end;
    long long mod;
    long long partial_result;
} thread_data_t;

long long global_result = 1;
long long mod_value;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Функция для вычисления частичного произведения
void* compute_partial_factorial(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    data->partial_result = 1;
    
    printf("Thread computing from %d to %d\n", data->start, data->end);
    
    for (int i = data->start; i <= data->end; i++) {
        data->partial_result = (data->partial_result * i) % data->mod;
    }
    
    // Защищаем доступ к глобальной переменной мьютексом
    pthread_mutex_lock(&mutex);
    global_result = (global_result * data->partial_result) % data->mod;
    printf("Thread partial result: %lld\n", data->partial_result);
    pthread_mutex_unlock(&mutex);
    
    return NULL;
}

// Функция для разбора аргументов командной строки
void parse_arguments(int argc, char* argv[], int* k, int* pnum, long long* mod) {
    // Значения по умолчанию
    *k = 10;
    *pnum = 4;
    *mod = 1000000007;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            *k = atoi(argv[i + 1]);
            i++;
        } else if (strncmp(argv[i], "--pnum=", 7) == 0) {
            *pnum = atoi(argv[i] + 7);
        } else if (strncmp(argv[i], "--mod=", 6) == 0) {
            *mod = atoll(argv[i] + 6);
        }
    }
}

int main(int argc, char* argv[]) {
    int k, pnum;
    long long mod;
    
    // Парсим аргументы командной строки
    parse_arguments(argc, argv, &k, &pnum, &mod);
    mod_value = mod;
    
    printf("Computing %d! mod %lld using %d threads\n", k, mod, pnum);
    
    if (k <= 0 || pnum <= 0 || mod <= 0) {
        fprintf(stderr, "Error: All parameters must be positive\n");
        return 1;
    }
    
    if (k == 0 || k == 1) {
        printf("Result: 1\n");
        return 0;
    }
    
    // Создаем потоки
    pthread_t threads[pnum];
    thread_data_t thread_data[pnum];
    
    // Распределяем работу между потоками
    int numbers_per_thread = k / pnum;
    int remainder = k % pnum;
    int current_start = 1;
    
    for (int i = 0; i < pnum; i++) {
        thread_data[i].start = current_start;
        thread_data[i].end = current_start + numbers_per_thread - 1;
        
        // Распределяем остаток
        if (remainder > 0) {
            thread_data[i].end++;
            remainder--;
        }
        
        // Убеждаемся, что не вышли за границы
        if (thread_data[i].end > k) {
            thread_data[i].end = k;
        }
        
        thread_data[i].mod = mod;
        current_start = thread_data[i].end + 1;
        
        // Создаем поток
        if (pthread_create(&threads[i], NULL, compute_partial_factorial, &thread_data[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    
    // Ждем завершения всех потоков
    for (int i = 0; i < pnum; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            return 1;
        }
    }
    
    // Уничтожаем мьютекс
    pthread_mutex_destroy(&mutex);
    
    printf("Final result: %d! mod %lld = %lld\n", k, mod, global_result);
    
    // Проверка: последовательное вычисление для верификации
    long long sequential_result = 1;
    for (int i = 1; i <= k; i++) {
        sequential_result = (sequential_result * i) % mod;
    }
    printf("Sequential result for verification: %lld\n", sequential_result);
    
    if (sequential_result == global_result) {
        printf("Results match!\n");
    } else {
        printf("✗ Results don't match! Parallel: %lld, Sequential: %lld\n", 
               global_result, sequential_result);
    }
    
    return 0;
}
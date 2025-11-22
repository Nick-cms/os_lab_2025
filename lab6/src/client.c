#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/time.h>

#include "common.h"

struct ThreadData {
    struct Server server;
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
    uint64_t result;
    int thread_id;
    pthread_t thread;
    bool completed;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

// Глобальные переменные для синхронизации
struct ThreadMonitor {
    struct ThreadData *threads;
    int total_threads;
    int completed_threads;
    pthread_mutex_t mutex;
    pthread_cond_t all_done;
};

struct ThreadMonitor monitor;

void* ServerThread(void* arg) {
    struct ThreadData* data = (struct ThreadData*)arg;
    
    printf("Thread %d started: connecting to %s:%d for range %lu-%lu\n", 
           data->thread_id, data->server.ip, data->server.port, 
           data->begin, data->end);
    
    struct hostent *hostname = gethostbyname(data->server.ip);
    if (hostname == NULL) {
        fprintf(stderr, "Thread %d: gethostbyname failed with %s\n", 
                data->thread_id, data->server.ip);
        data->result = 0;
        goto thread_complete;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(data->server.port);
    server_addr.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

    int sck = socket(AF_INET, SOCK_STREAM, 0);
    if (sck < 0) {
        fprintf(stderr, "Thread %d: Socket creation failed!\n", data->thread_id);
        data->result = 0;
        goto thread_complete;
    }

    // Устанавливаем таймауты для неблокирующей работы
    struct timeval timeout;
    timeout.tv_sec = 10;  // 10 секунд таймаут
    timeout.tv_usec = 0;
    setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sck, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    printf("Thread %d: Connecting to %s:%d...\n", 
           data->thread_id, data->server.ip, data->server.port);
    
    if (connect(sck, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Thread %d: Connection to %s:%d failed\n", 
                data->thread_id, data->server.ip, data->server.port);
        close(sck);
        data->result = 0;
        goto thread_complete;
    }

    printf("Thread %d: Connected successfully, sending task...\n", data->thread_id);

    char task[sizeof(uint64_t) * 3];
    memcpy(task, &data->begin, sizeof(uint64_t));
    memcpy(task + sizeof(uint64_t), &data->end, sizeof(uint64_t));
    memcpy(task + 2 * sizeof(uint64_t), &data->mod, sizeof(uint64_t));

    if (send(sck, task, sizeof(task), 0) < 0) {
        fprintf(stderr, "Thread %d: Send failed to %s:%d\n", 
                data->thread_id, data->server.ip, data->server.port);
        close(sck);
        data->result = 0;
        goto thread_complete;
    }

    printf("Thread %d: Task sent, waiting for response...\n", data->thread_id);

    char response[sizeof(uint64_t)];
    if (recv(sck, response, sizeof(response), 0) < 0) {
        fprintf(stderr, "Thread %d: Receive failed from %s:%d\n", 
                data->thread_id, data->server.ip, data->server.port);
        close(sck);
        data->result = 0;
        goto thread_complete;
    }

    memcpy(&data->result, response, sizeof(uint64_t));
    printf("Thread %d: Got result from %s:%d: %lu (range %lu-%lu)\n", 
           data->thread_id, data->server.ip, data->server.port, 
           data->result, data->begin, data->end);

    close(sck);

thread_complete:
    // Помечаем поток как завершенный и уведомляем монитор
    pthread_mutex_lock(&monitor.mutex);
    data->completed = true;
    monitor.completed_threads++;
    
    printf("Thread %d: COMPLETED (%d/%d threads done)\n", 
           data->thread_id, monitor.completed_threads, monitor.total_threads);
    
    // Если все потоки завершены, сигнализируем основному потоку
    if (monitor.completed_threads == monitor.total_threads) {
        pthread_cond_broadcast(&monitor.all_done);
    }
    pthread_mutex_unlock(&monitor.mutex);
    
    return NULL;
}

// Функция для проверки статуса потоков без блокировки
void check_threads_progress() {
    pthread_mutex_lock(&monitor.mutex);
    int completed = monitor.completed_threads;
    int total = monitor.total_threads;
    pthread_mutex_unlock(&monitor.mutex);
    
    printf("Progress: %d/%d servers completed\n", completed, total);
}

// Функция для ожидания завершения всех потоков с таймаутом
bool wait_for_all_threads(int timeout_seconds) {
    struct timeval now;
    struct timespec timeout;
    
    pthread_mutex_lock(&monitor.mutex);
    
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + timeout_seconds;
    timeout.tv_nsec = now.tv_usec * 1000;
    
    while (monitor.completed_threads < monitor.total_threads) {
        printf("Waiting for %d more servers...\n", 
               monitor.total_threads - monitor.completed_threads);
        
        if (pthread_cond_timedwait(&monitor.all_done, &monitor.mutex, &timeout) != 0) {
            pthread_mutex_unlock(&monitor.mutex);
            printf("Timeout waiting for servers after %d seconds\n", timeout_seconds);
            return false;
        }
    }
    
    pthread_mutex_unlock(&monitor.mutex);
    return true;
}

int main(int argc, char **argv) {
    uint64_t k = 0;
    uint64_t mod = 0;
    bool k_set = false;
    bool mod_set = false;
    char servers_file[255] = {'\0'};

    // Инициализация монитора
    monitor.threads = NULL;
    monitor.total_threads = 0;
    monitor.completed_threads = 0;
    pthread_mutex_init(&monitor.mutex, NULL);
    pthread_cond_init(&monitor.all_done, NULL);

    while (true) {
        static struct option options[] = {
            {"k", required_argument, 0, 0},
            {"mod", required_argument, 0, 0},
            {"servers", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case 0: {
            switch (option_index) {
            case 0:
                if (!ConvertStringToUI64(optarg, &k)) {
                    fprintf(stderr, "Invalid k value: %s\n", optarg);
                    return 1;
                }
                k_set = true;
                break;
            case 1:
                if (!ConvertStringToUI64(optarg, &mod)) {
                    fprintf(stderr, "Invalid mod value: %s\n", optarg);
                    return 1;
                }
                mod_set = true;
                break;
            case 2:
                strncpy(servers_file, optarg, sizeof(servers_file) - 1);
                servers_file[sizeof(servers_file) - 1] = '\0';
                break;
            default:
                printf("Index %d is out of options\n", option_index);
            }
        } break;

        case '?':
            printf("Arguments error\n");
            break;
        default:
            fprintf(stderr, "getopt returned character code 0%o?\n", c);
        }
    }

    if (!k_set || !mod_set || !strlen(servers_file)) {
        fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
        return 1;
    }

    if (k == 0 || mod == 0) {
        fprintf(stderr, "k and mod must be positive values\n");
        return 1;
    }

    FILE* file = fopen(servers_file, "r");
    if (file == NULL) {
        fprintf(stderr, "Cannot open servers file: %s\n", servers_file);
        return 1;
    }

    struct Server* servers = NULL;
    int servers_num = 0;
    char line[255];
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        struct Server server;
        char* colon = strchr(line, ':');
        if (colon == NULL) {
            fprintf(stderr, "Invalid server format: %s (expected ip:port)\n", line);
            continue;
        }
        
        *colon = '\0';
        strncpy(server.ip, line, sizeof(server.ip) - 1);
        server.ip[sizeof(server.ip) - 1] = '\0';
        server.port = atoi(colon + 1);
        
        if (server.port <= 0) {
            fprintf(stderr, "Invalid port in: %s\n", line);
            continue;
        }
        
        servers_num++;
        servers = realloc(servers, sizeof(struct Server) * servers_num);
        servers[servers_num - 1] = server;
    }
    fclose(file);

    if (servers_num == 0) {
        fprintf(stderr, "No valid servers found in file: %s\n", servers_file);
        free(servers);
        return 1;
    }

    printf("Starting PARALLEL computation of %lu! mod %lu using %d servers\n", k, mod, servers_num);

    // Инициализация данных потоков
    struct ThreadData *thread_data = malloc(sizeof(struct ThreadData) * servers_num);
    monitor.threads = thread_data;
    monitor.total_threads = servers_num;

    uint64_t numbers_per_server = k / (uint64_t)servers_num;
    uint64_t remainder = k % (uint64_t)servers_num;
    uint64_t current_start = 1;

    // Инициализация и запуск всех потоков
    for (int i = 0; i < servers_num; i++) {
        thread_data[i].server = servers[i];
        thread_data[i].begin = current_start;
        thread_data[i].end = current_start + numbers_per_server - 1;
        
        if (remainder > 0) {
            thread_data[i].end++;
            remainder--;
        }
        
        thread_data[i].mod = mod;
        thread_data[i].thread_id = i;
        thread_data[i].completed = false;
        thread_data[i].result = 1;
        
        pthread_mutex_init(&thread_data[i].mutex, NULL);
        pthread_cond_init(&thread_data[i].cond, NULL);

        current_start = thread_data[i].end + 1;

        printf("Server %d: %s:%d will compute range %lu-%lu\n", 
               i, servers[i].ip, servers[i].port, 
               thread_data[i].begin, thread_data[i].end);

        // Запускаем все потоки
        if (pthread_create(&thread_data[i].thread, NULL, ServerThread, &thread_data[i]) != 0) {
            fprintf(stderr, "Failed to create thread for server %s:%d\n", 
                    servers[i].ip, servers[i].port);
            thread_data[i].result = 1;
            thread_data[i].completed = true;
            
            pthread_mutex_lock(&monitor.mutex);
            monitor.completed_threads++;
            pthread_mutex_unlock(&monitor.mutex);
        }
    }

    printf("\nAll %d server threads started working\n", servers_num);
    printf("Waiting for completion...\n\n");

    // Ждем завершения ВСЕХ потоков ПАРАЛЛЕЛЬНО с возможностью показа прогресса
    bool all_completed = wait_for_all_threads(30); // 30 секунд таймаут

    if (!all_completed) {
        printf("Some servers didn't respond in time. Using available results.\n");
    }

    // Показываем финальный прогресс
    check_threads_progress();

    // Объединяем результаты от всех серверов
    uint64_t total_result = 1;
    int successful_servers = 0;
    
    for (int i = 0; i < servers_num; i++) {
        if (thread_data[i].completed && thread_data[i].result != 0) {
            total_result = MultModulo(total_result, thread_data[i].result, mod);
            successful_servers++;
            
            // Освобождаем ресурсы мьютексов
            pthread_mutex_destroy(&thread_data[i].mutex);
            pthread_cond_destroy(&thread_data[i].cond);
        }
    }

    printf("\n%d/%d servers completed successfully\n", successful_servers, servers_num);
    printf("Final result: %lu! mod %lu = %lu\n", k, mod, total_result);

    // Проверка: последовательное вычисление для верификации
    uint64_t sequential_result = 1;
    for (uint64_t i = 1; i <= k; i++) {
        sequential_result = MultModulo(sequential_result, i, mod);
    }
    printf("Sequential result for verification: %lu\n", sequential_result);
    
    if (sequential_result == total_result) {
        printf("Results match! Parallel computation successful!\n");
    } else {
        printf("Results don't match! Parallel: %lu, Sequential: %lu\n", 
               total_result, sequential_result);
    }

    // Освобождаем ресурсы
    free(thread_data);
    free(servers);
    pthread_mutex_destroy(&monitor.mutex);
    pthread_cond_destroy(&monitor.all_done);
    
    return 0;
}
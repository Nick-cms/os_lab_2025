#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

int thread1_completed = 0;
int thread2_completed = 0;

void* thread1_function(void* arg) {
    printf("Thread 1: Trying to lock mutex1...\n");
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: Locked mutex1\n");
    
    // Имитация работы
    sleep(1);
    
    printf("Thread 1: Trying to lock mutex2...\n");
    pthread_mutex_lock(&mutex2);  // DEADLOCK
    printf("Thread 1: Locked mutex2\n");
    
    // Критическая секция
    printf("Thread 1: Entering critical section\n");
    sleep(1);
    printf("Thread 1: Exiting critical section\n");
    
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);
    
    thread1_completed = 1;
    printf("Thread 1: Completed successfully\n");
    
    return NULL;
}

void* thread2_function(void* arg) {
    printf("Thread 2: Trying to lock mutex2...\n");
    pthread_mutex_lock(&mutex2);
    printf("Thread 2: Locked mutex2\n");
    
    // Имитация работы
    sleep(1);
    
    printf("Thread 2: Trying to lock mutex1...\n");
    pthread_mutex_lock(&mutex1);  // DEADLOCK здесь!
    printf("Thread 2: Locked mutex1\n");
    
    // Критическая секция
    printf("Thread 2: Entering critical section\n");
    sleep(1);
    printf("Thread 2: Exiting critical section\n");
    
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);
    
    thread2_completed = 1;
    printf("Thread 2: Completed successfully\n");
    
    return NULL;
}

void* monitor_thread(void* arg) {
    int time_elapsed = 0;
    
    while (time_elapsed < 10) {
        sleep(1);
        time_elapsed++;
        printf("Monitor: %d seconds elapsed - ", time_elapsed);
        
        if (!thread1_completed && !thread2_completed) {
            printf("Both threads are stuck in DEADLOCK!\n");
        } else if (!thread1_completed) {
            printf("Thread 1 is stuck, Thread 2 completed\n");
        } else if (!thread2_completed) {
            printf("Thread 2 is stuck, Thread 1 completed\n");
        } else {
            printf("Both threads completed successfully\n");
            break;
        }
    }
    
    return NULL;
}

int main() {
    pthread_t thread1, thread2, monitor;
    
    printf("=== Deadlock Demonstration ===\n");
    printf("This program will demonstrate a classic deadlock scenario.\n");
    printf("Two threads will try to acquire two mutexes in different order.\n\n");
    
    // Создаем монитор-поток для отслеживания состояния
    pthread_create(&monitor, NULL, monitor_thread, NULL);
    
    // Создаем основные потоки
    if (pthread_create(&thread1, NULL, thread1_function, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }
    
    if (pthread_create(&thread2, NULL, thread2_function, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }
    
    // Ждем завершения монитора
    pthread_join(monitor, NULL);
    
    printf("\n=== Final Status ===\n");
    printf("Thread 1 completed: %s\n", thread1_completed ? "YES" : "NO");
    printf("Thread 2 completed: %s\n", thread2_completed ? "YES" : "NO");
    
    if (!thread1_completed || !thread2_completed) {
        printf("\n*** DEADLOCK CONFIRMED ***\n");
        printf("The program detected a deadlock situation.\n");
        printf("At least one thread could not complete its work.\n");
    } else {
        printf("\n*** No deadlock occurred ***\n");
    }
    
    // Очистка ресурсов (хотя потоки могут быть все еще заблокированы)
    printf("Program finished.\n");
    
    return 0;
}
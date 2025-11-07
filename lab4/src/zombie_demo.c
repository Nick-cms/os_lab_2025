#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void demonstrate_zombie() {
    printf("Демонстрация зомби-процессов\n");
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Дочерний процесс
        printf("Дочерний процесс: PID = %d, PPID = %d\n", getpid(), getppid());
        printf("Дочерний процесс завершается, но родитель не вызывает wait()\n");
        exit(0); // Завершаем дочерний процесс
    } else if (pid > 0) {
        // Родительский процесс
        printf("Родительский процесс: PID = %d, создал дочерний с PID = %d\n", getpid(), pid);
        printf("Родительский процесс НЕ вызывает wait() - дочерний станет зомби!\n");
        
        // Даем время увидеть зомби в системе
        printf("Ожидание 20 секунд... Проверьте процессы в другом терминале:\n");
        printf("Команда: 'ps aux | grep %d'\n", pid);
        sleep(20);
        
        // Теперь ждем завершение дочернего процесса
        printf("Теперь родитель вызывает wait()...\n");
        wait(NULL);
        printf("Зомби процесс убран!\n");
    } else {
        perror("fork failed");
        exit(1);
    }
}

void multiple_zombies() {
    printf("\nМножественные зомби-процессы\n");
    
    int num_children = 3;
    pid_t children[3];
    
    for (int i = 0; i < num_children; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // Дочерний процесс
            printf("Дочерний %d: PID = %d завершается\n", i, getpid());
            exit(0);
        } else if (pid > 0) {
            // Родительский процесс
            children[i] = pid;
            printf("Создан дочерний процесс %d с PID = %d\n", i, pid);
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    printf("Создано %d дочерних процессов. Ожидание 20 секунд...\n", num_children);
    printf("Проверьте зомби процессы командой: 'ps aux | grep defunct'\n");
    sleep(20);
    
    // Убираем зомби
    printf("Убираем зомби процессы...\n");
    for (int i = 0; i < num_children; i++) {
        waitpid(children[i], NULL, 0);
        printf("Зомби процесс %d убран\n", children[i]);
    }
}

void prevent_zombie_with_wait() {
    printf("\nПредотвращение зомби с помощью wait()\n");
    
    pid_t pid = fork();
    
    if (pid == 0) {
        printf("Дочерний процесс: PID = %d работает...\n", getpid());
        sleep(2);
        printf("Дочерний процесс завершается\n");
        exit(0);
    } else if (pid > 0) {
        printf("Родительский процесс ожидает завершение дочернего...\n");
        wait(NULL); // Ждем завершение дочернего процесса
        printf("Дочерний процесс корректно завершен, зомби не создан!\n");
    } else {
        perror("fork failed");
        exit(1);
    }
}

int main() {
    int choice;
    
    printf("Демонстрация зомби-процессов\n");
    printf("1. Одиночный зомби процесс\n");
    printf("2. Множественные зомби процессы\n");
    printf("3. Предотвращение зомби с wait()\n");
    printf("Выберите вариант: ");
    
    scanf("%d", &choice);
    
    switch(choice) {
        case 1:
            demonstrate_zombie();
            break;
        case 2:
            multiple_zombies();
            break;
        case 3:
            prevent_zombie_with_wait();
            break;
        default:
            printf("Неверный выбор\n");
            break;
    }
    
    return 0;
}
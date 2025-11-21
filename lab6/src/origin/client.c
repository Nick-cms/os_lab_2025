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

struct Server {
  char ip[255];
  int port;
};

struct ThreadData {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  int thread_id;
};

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (result + a) % mod;
    a = (a * 2) % mod;
    b /= 2;
  }

  return result % mod;
}

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0)
    return false;

  *val = i;
  return true;
}

void* ServerThread(void* arg) {
  struct ThreadData* data = (struct ThreadData*)arg;
  
  struct hostent *hostname = gethostbyname(data->server.ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", data->server.ip);
    data->result = 0;
    return NULL;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(data->server.port);
  server_addr.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    data->result = 0;
    return NULL;
  }

  if (connect(sck, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", data->server.ip, data->server.port);
    close(sck);
    data->result = 0;
    return NULL;
  }

  char task[sizeof(uint64_t) * 3];
  memcpy(task, &data->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &data->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &data->mod, sizeof(uint64_t));

  if (send(sck, task, sizeof(task), 0) < 0) {
    fprintf(stderr, "Send failed to %s:%d\n", data->server.ip, data->server.port);
    close(sck);
    data->result = 0;
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (recv(sck, response, sizeof(response), 0) < 0) {
    fprintf(stderr, "Receive failed from %s:%d\n", data->server.ip, data->server.port);
    close(sck);
    data->result = 0;
    return NULL;
  }

  memcpy(&data->result, response, sizeof(uint64_t));
  printf("Thread %d got result from %s:%d: %lu (range %lu-%lu)\n", 
         data->thread_id, data->server.ip, data->server.port, 
         data->result, data->begin, data->end);

  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = 0;
  uint64_t mod = 0;
  bool k_set = false;
  bool mod_set = false;
  char servers_file[255] = {'\0'}; // 255 - максимальная длина пути в большинстве систем

  while (true) {
    
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

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
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  if (k == 0 || mod == 0) {
    fprintf(stderr, "k and mod must be positive values\n");
    return 1;
  }

  // Чтение серверов из файла
  FILE* file = fopen(servers_file, "r");
  if (file == NULL) {
    fprintf(stderr, "Cannot open servers file: %s\n", servers_file);
    return 1;
  }

  struct Server* servers = NULL;
  int servers_num = 0;
  char line[255];
  
  while (fgets(line, sizeof(line), file)) {
    // Удаляем символ новой строки
    line[strcspn(line, "\n")] = 0;
    
    // Пропускаем пустые строки
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

  printf("Found %d servers, computing %lu! mod %lu\n", servers_num, k, mod);

  // Распределяем работу между серверами
  pthread_t threads[servers_num];
  struct ThreadData thread_data[servers_num];
  
  uint64_t numbers_per_server = k / (uint64_t)servers_num;
  uint64_t remainder = k % (uint64_t)servers_num;
  uint64_t current_start = 1;

  for (int i = 0; i < servers_num; i++) {
    thread_data[i].server = servers[i];
    thread_data[i].begin = current_start;
    thread_data[i].end = current_start + numbers_per_server - 1;
    
    // Распределяем остаток
    if (remainder > 0) {
      thread_data[i].end++;
      remainder--;
    }
    
    thread_data[i].mod = mod;
    thread_data[i].thread_id = i;
    current_start = thread_data[i].end + 1;

    if (pthread_create(&threads[i], NULL, ServerThread, &thread_data[i]) != 0) {
      fprintf(stderr, "Failed to create thread for server %s:%d\n", 
              servers[i].ip, servers[i].port);
      thread_data[i].result = 1; // нейтральный элемент для умножения
    }
  }

  // Ждем завершения всех потоков
  for (int i = 0; i < servers_num; i++) {
    pthread_join(threads[i], NULL);
  }

  // Объединяем результаты от всех серверов
  uint64_t total_result = 1;
  for (int i = 0; i < servers_num; i++) {
    if (thread_data[i].result != 0) { // 0 означает ошибку
      total_result = MultModulo(total_result, thread_data[i].result, mod);
    }
  }

  printf("Final result: %lu! mod %lu = %lu\n", k, mod, total_result);

  // Проверка: последовательное вычисление для верификации
  uint64_t sequential_result = 1;
  for (uint64_t i = 1; i <= k; i++) {
    sequential_result = MultModulo(sequential_result, i, mod);
  }
  printf("Sequential result for verification: %lu\n", sequential_result);
  
  if (sequential_result == total_result) {
    printf("Results match!\n");
  } else {
    printf("Results don't match! Parallel: %lu, Sequential: %lu\n", 
           total_result, sequential_result);
  }

  free(servers);
  return 0;
}
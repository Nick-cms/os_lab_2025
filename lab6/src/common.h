#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

// Структура для передачи диапазона вычислений
struct FactorialArgs {
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
};

// Структура для информации о сервере
struct Server {
    char ip[255];
    int port;
};

// Функция модульного умножения
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

// Функция преобразования строки в uint64_t
bool ConvertStringToUI64(const char *str, uint64_t *val);

// Функция для вычисления факториала в диапазоне
uint64_t Factorial(const struct FactorialArgs *args);

#endif
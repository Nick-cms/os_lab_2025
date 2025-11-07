CC = gcc
TARGETS = parallel_min_max process_memory

all: $(TARGETS)

# Сборка parallel_min_max
parallel_min_max: parallel_min_max.c find_min_max.c utils.c
	$(CC) -o $@ parallel_min_max.c find_min_max.c utils.c

# Сборка process_memory
process_memory: process_memory.c
	$(CC) -o $@ process_memory.c

# Тесты для parallel_min_max
test_parallel: parallel_min_max
	@echo "Тест parallel_min_max без таймаута"
	./parallel_min_max --seed 123 --array_size 1000000 --pnum 4
	@echo ""
	@echo "Тест parallel_min_max с таймаутом"
	./parallel_min_max --seed 123 --array_size 1000000 --pnum 4 --timeout 2
	@echo ""
	@echo "Тест parallel_min_max с файлами и таймаутом"
	./parallel_min_max --seed 123 --array_size 1000000 --pnum 4 --by_files --timeout 3

# Тест для process_memory
test_memory: process_memory
	@echo ""
	@echo "Запуск process_memory"
	./process_memory

# Запуск всех тестов
test: test_parallel test_memory

# Очистка
clean:
	rm -f parallel_min_max process_memory

.PHONY: all clean test test_parallel test_memory
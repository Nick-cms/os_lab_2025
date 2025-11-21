SERVER_COUNT=3
TNUM_COUNT=4

all: server client

server: server.c
	gcc -Wall -Wextra -pthread -g -o server server.c

client: client.c
	gcc -Wall -Wextra -pthread -g -o client client.c

servers.txt:
	@echo "Creating servers.txt with $(SERVER_COUNT) servers..."
	@for i in $$(seq 1 $(SERVER_COUNT)); do \
		port=$$((20001 + $$i - 1)); \
		echo "127.0.0.1:$$port" >> servers.txt; \
	done
	@echo "Servers file created:"
	@cat servers.txt

start-servers: server servers.txt
	@echo "Starting $(SERVER_COUNT) servers..."
	@for i in $$(seq 1 $(SERVER_COUNT)); do \
		port=$$((20001 + $$i - 1)); \
		echo "Starting server on port $$port..."; \
		./server --port $$port --tnum $(TNUM_COUNT) > server_$$port.log 2>&1 & \
		echo $$! > server_$$port.pid; \
	done

stop-servers:
	@echo "Stopping all servers..."
	@for pidfile in server_*.pid; do \
		if [ -f "$$pidfile" ]; then \
			pid=$$(cat $$pidfile); \
			if kill $$pid 2>/dev/null; then \
				echo "Stopped server with PID $$pid"; \
			else \
				echo "Server with PID $$pid already stopped"; \
			fi; \
			rm -f $$pidfile; \
		fi; \
	done
	@rm -f server_*.log
	@echo "All servers stopped"

test-client: client servers.txt
	./client --k 20 --mod 1000000007 --servers servers.txt

test: start-servers
	@sleep 3
	@echo "Running client test..."
	@make test-client || true
	@make stop-servers
	@make clean

status:
	@echo "Running servers:"
	@ps aux | grep "[.]/server" || echo "No servers running"

clean:
	rm -f server client servers.txt server_*.log server_*.pid

.PHONY: all clean start-servers stop-servers test-client test show-logs status
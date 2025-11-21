CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = tcpclient tcpserver udpclient udpserver

TCP_PORT = 10050
TCP_BUFSIZE = 100
UDP_PORT = 20001
UDP_BUFSIZE = 1024
IP = 127.0.0.1

all: $(TARGETS)

tcpclient: tcpclient.c
	$(CC) $(CFLAGS) -o $@ $<

tcpserver: tcpserver.c
	$(CC) $(CFLAGS) -o $@ $<

udpclient: udpclient.c
	$(CC) $(CFLAGS) -o $@ $<

udpserver: udpserver.c
	$(CC) $(CFLAGS) -o $@ $<

run-tcp-server: tcpserver
	./tcpserver $(TCP_PORT) $(TCP_BUFSIZE)

run-tcp-client: tcpclient
	./tcpclient $(IP) $(TCP_PORT) $(TCP_BUFSIZE)

run-udp-server: udpserver
	./udpserver $(UDP_PORT) $(UDP_BUFSIZE)

run-udp-client: udpclient
	./udpclient $(IP) $(UDP_PORT) $(UDP_BUFSIZE)

run-servers: tcpserver udpserver
	@echo "Starting TCP server on port $(TCP_PORT)..."
	@./tcpserver $(TCP_PORT) $(TCP_BUFSIZE) &
	@echo "TCP server PID: $$!"
	@echo "Starting UDP server on port $(UDP_PORT)..."
	@./udpserver $(UDP_PORT) $(UDP_BUFSIZE) &
	@echo "UDP server PID: $$!"
	@echo "Servers started in background."

stop-servers:
	@pkill -f tcpserver || true
	@pkill -f udpserver || true
	@echo "Servers stopped"

test-tcp: tcpserver tcpclient
	@echo "Testing TCP connection..."
	@timeout 5s ./tcpserver $(TCP_PORT) $(TCP_BUFSIZE) & \
	SERVER_PID=$$!; \
	sleep 1; \
	echo "Hello TCP" | ./tcpclient $(IP) $(TCP_PORT) $(TCP_BUFSIZE) || true; \
	sleep 1; \
	kill $$SERVER_PID 2>/dev/null || true

test-udp: udpserver udpclient
	@echo "Testing UDP connection..."
	@timeout 5s ./udpserver $(UDP_PORT) $(UDP_BUFSIZE) & \
	SERVER_PID=$$!; \
	sleep 1; \
	echo "Hello UDP" | ./udpclient $(IP) $(UDP_PORT) $(UDP_BUFSIZE) || true; \
	sleep 1; \
	kill $$SERVER_PID 2>/dev/null || true

clean:
	rm -f $(TARGETS)

.PHONY: all clean run-tcp-server run-tcp-client run-udp-server run-udp-client run-servers stop-servers test-tcp test-udp test-udp-alt test-udp-nc
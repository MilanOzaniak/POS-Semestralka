CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -pthread -O2 -D_POSIX_C_SOURCE=200809L
INCLUDES = -Ishared

SERVER_BIN = server
CLIENT_BIN = client

SERVER_SRC = Server/server.c shared/ipc.c
CLIENT_SRC = Client/client.c shared/ipc.c

SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

all: server client

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_OBJ)

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_OBJ) -lncurses

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all server client clean


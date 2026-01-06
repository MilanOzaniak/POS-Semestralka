CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -pedantic -std=c11 -O2 -pthread -D_POSIX_C_SOURCE=200809L
INCS    := -Ishared -IServer -IClient

SERVER_SRCS := Server/main.c Server/server.c Server/game.c Server/map.c shared/ipc.c
CLIENT_SRCS := Client/main.c Client/client.c Client/draw.c Client/menu.c shared/ipc.c

SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

all: server client

server: server_bin

client: client_bin

server_bin: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(SERVER_OBJS) -o server

client_bin: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -lncurses -o client

%.o: %.c
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) server client

.PHONY: all server client clean


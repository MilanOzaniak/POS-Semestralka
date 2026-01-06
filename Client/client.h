#pragma once
#include <pthread.h>
#include <stdint.h>
#include "../shared/messages.h"

typedef struct {
    int fd;
    uint32_t my_id;
    uint8_t is_host;

    pthread_t recv_th;
    pthread_mutex_t mtx;

    state_t st;
    uint8_t has_state;
    uint8_t disconnected;
} client_t;

int  client_connect(client_t *C, const char *ip, uint16_t port);
void client_close(client_t *C);

void client_send_dir(client_t *C, uint8_t dir);
void client_send_action(client_t *C, uint8_t action);

int  client_start_recv(client_t *C);
void client_get_state(client_t *C, state_t *out, uint8_t *has, uint8_t *disc);

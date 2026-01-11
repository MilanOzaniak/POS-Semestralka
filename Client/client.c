#define _POSIX_C_SOURCE 200809L
#include "client.h"
#include "../shared/ipc.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


// funkcia na zahadzovanie sprav ktore nechceme spracovavať
// lebo dalšie spravy by sa rozbili
static void drain_payload(int fd, uint16_t len) {
    uint8_t buf[512];
    while (len > 0) {
        uint16_t chunk = (len > sizeof(buf)) ? (uint16_t)sizeof(buf) : len;
        if (recv_all(fd, buf, chunk) != 0) break;
        len = (uint16_t)(len - chunk);
    }
}


// prijimanie sprav zo servera
static void* recv_thread_fn(void *arg) {
    client_t *C = (client_t*)arg;

    for (;;) {
        uint16_t type = 0, len = 0;
        // ak nesedi velkost tak zahodime
        if (recv_hdr(C->fd, &type, &len) != 0) break;

        if (type == MSG_STATE) {

            if (len != sizeof(state_t)) { 
              drain_payload(C->fd, len); break; 
            }

            state_t tmp;
            if (recv_all(C->fd, &tmp, sizeof(tmp)) != 0) break;
            
            // synchronizacia
            pthread_mutex_lock(&C->mtx);
            C->st = tmp;
            C->has_state = 1;
            pthread_mutex_unlock(&C->mtx);
        } else {
            drain_payload(C->fd, len);
        }
    }
    
    // ak sme skoncili cyklus tak bol hrac odpojeny alebo nastala chyba
    pthread_mutex_lock(&C->mtx);
    C->disconnected = 1;
    pthread_mutex_unlock(&C->mtx);

    return NULL;
}


int client_connect(client_t *C, const char *ip, uint16_t port) {
    memset(C, 0, sizeof(*C));
    pthread_mutex_init(&C->mtx, NULL);
    
    // vytvorenie TCP socketu
    C->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (C->fd < 0) return -1;

    // nastavenie cielovej adresy
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // prevod IP stringu na binarnu formu
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
      close(C->fd); return -1; 
    }

    // TCP
    if (connect(C->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      close(C->fd); return -1; 
    }
    
    // posleme msg_join ak == 0 tak presla
    if (send_msg(C->fd, MSG_JOIN, NULL, 0) != 0) { 
      close(C->fd); 
      return -1; 
    }

    // cakanie na message MSG_JOIN_OK ostatne zahadzujeme
    for (;;) {
        uint16_t type = 0, len = 0;
        if (recv_hdr(C->fd, &type, &len) != 0) { 
          close(C->fd); return -1; 
        }

        if (type == MSG_JOIN_OK) {
            if (len != sizeof(join_ok_t)) { 
              drain_payload(C->fd, len); close(C->fd); return -1; 
            }

            join_ok_t ok;
            if (recv_all(C->fd, &ok, sizeof(ok)) != 0) { 
              close(C->fd); return -1; 
            }
            C->my_id = ok.player_id;
            C->is_host = ok.is_host;
            break;
        } else {
            drain_payload(C->fd, len);
        }
    }

    return 0;
}

// spustanie prijimacieho vlakna
int client_start_recv(client_t *C) {
    if (pthread_create(&C->recv_th, NULL, recv_thread_fn, C) != 0) return -1;
    return 0;
}

// zatvorenie socketu klienta
void client_close(client_t *C) {
    if (C->fd >= 0) close(C->fd);
    C->fd = -1;
}

// posielanie spravy s direction, action je none lebo nas nezaujima
void client_send_dir(client_t *C, uint8_t dir) {
    input_t in;
    in.dir = dir;
    in.action = ACT_NONE;
    (void)send_msg(C->fd, MSG_INPUT, &in, sizeof(in));
}

// posielanie spravy s akciou (start, koniec), direction nas nezaujima
void client_send_action(client_t *C, uint8_t action) {
    input_t in;
    in.dir = 255;
    in.action = action;
    (void)send_msg(C->fd, MSG_INPUT, &in, sizeof(in));
}

// vracia posledny prijaty state_t 
void client_get_state(client_t *C, state_t *out, uint8_t *has, uint8_t *disc) {
    pthread_mutex_lock(&C->mtx);
    *out = C->st;
    *has = C->has_state;
    *disc = C->disconnected;
    pthread_mutex_unlock(&C->mtx);
}

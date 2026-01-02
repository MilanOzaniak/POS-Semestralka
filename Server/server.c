#include "../shared/messages.h"
#include "../shared/ipc.h"


#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int fd;
    uint32_t id;

    uint8_t active;
    uint8_t alive;
    uint8_t last_dir;

    uint32_t score;

    uint16_t snake_len;
    pos_t snake[MAX_SNAKE];

    pthread_t th;
} player_t;

typedef struct {
    int lfd;

    pthread_mutex_t mtx;

    player_t players[MAX_PLAYERS];
    uint32_t next_id;

    uint8_t lobby; 
    uint8_t start_req;
    uint8_t restart_req;

    uint32_t tick;

    uint8_t fruit_count;
    pos_t fruits[MAX_FRUITS];

    uint8_t shutdown;
} server_t;

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static void sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int listen_on(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    return fd;
}

static int wait_for_join(int cfd) {
    for (;;) {
        uint16_t type=0, len=0;
        if (recv_hdr(cfd, &type, &len) != 0) return -1;
        if (len > 4096) return -1;

        uint8_t payload[4096];
        if (len > 0 && recv_all(cfd, payload, len) != 0) return -1;

        if (type == MSG_JOIN) return 0;
 
    }
}

static int pos_eq(pos_t a, pos_t b) { return a.x == b.x && a.y == b.y; }

static int any_snake_contains(server_t *S, pos_t p) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        if (!pl->active) continue;
        
        if (!pl->alive) continue;
        for (uint16_t k = 0; k < pl->snake_len; k++) {
            if (pos_eq(pl->snake[k], p)) return 1;
        }
    }
    return 0;
}

static void spawn_fruits(server_t *S) {
    S->fruit_count = MAX_FRUITS;
    for (uint8_t i = 0; i < S->fruit_count; i++) {
        for (int tries = 0; tries < 5000; tries++) {
            pos_t p = {(uint8_t)(rand() % WORLD_W), (uint8_t)(rand() % WORLD_H)};
            if (any_snake_contains(S, p)) continue;

            int clash = 0;
            for (uint8_t j = 0; j < i; j++) if (pos_eq(S->fruits[j], p)) clash = 1;
            if (!clash) { S->fruits[i] = p; break; }
        }
    }
}

static void init_round(server_t *S) {


    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        if (!pl->active) continue;

        pl->alive = 1;
        pl->last_dir = DIR_RIGHT;
        pl->snake_len = 4;

        uint8_t base_x = 5, base_y = 5;
        if (i == 1) { base_x = WORLD_W - 6; base_y = 5; }
        if (i == 2) { base_x = 5; base_y = WORLD_H - 6; }
        if (i == 3) { base_x = WORLD_W - 6; base_y = WORLD_H - 6; }

        for (uint16_t k = 0; k < pl->snake_len; k++) {
            pl->snake[k].x = (uint8_t)(base_x - k);
            pl->snake[k].y = base_y;
        }
    }

    spawn_fruits(S);
}

static int cell_occupied_by_any(server_t *S, pos_t p) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        if (!pl->active || !pl->alive) continue;
        for (uint16_t k = 0; k < pl->snake_len; k++) {
            if (pos_eq(pl->snake[k], p)) return 1;
        }
    }
    return 0;
}

static void move_one(server_t *S, player_t *pl) {
    if (!pl->active || !pl->alive) return;

    pos_t head = pl->snake[0];
    pos_t nh = head;

    if (pl->last_dir == DIR_UP)    nh.y = (uint8_t)(nh.y - 1);
    if (pl->last_dir == DIR_DOWN)  nh.y = (uint8_t)(nh.y + 1);
    if (pl->last_dir == DIR_LEFT)  nh.x = (uint8_t)(nh.x - 1);
    if (pl->last_dir == DIR_RIGHT) nh.x = (uint8_t)(nh.x + 1);


    if (nh.x >= WORLD_W || nh.y >= WORLD_H) { pl->alive = 0; return; }

    
    if (cell_occupied_by_any(S, nh)) { pl->alive = 0; return; }


    int ate = -1;
    for (uint8_t i = 0; i < S->fruit_count; i++) {
        if (pos_eq(S->fruits[i], nh)) { ate = i; break; }
    }

    if (ate >= 0 && pl->snake_len < MAX_SNAKE) {
        for (int k = (int)pl->snake_len; k > 0; k--) pl->snake[k] = pl->snake[k-1];
        pl->snake[0] = nh;
        pl->snake_len++;
        pl->score += 10;

        
        for (int tries = 0; tries < 5000; tries++) {
            pos_t p = {(uint8_t)(rand() % WORLD_W), (uint8_t)(rand() % WORLD_H)};
            if (!cell_occupied_by_any(S, p)) { S->fruits[(uint8_t)ate] = p; break; }
        }
    } else {
        for (int k = (int)pl->snake_len - 1; k > 0; k--) pl->snake[k] = pl->snake[k-1];
        pl->snake[0] = nh;
    }
}

static void broadcast_state(server_t *S) {
    state_t st;
    memset(&st, 0, sizeof(st));
    st.tick = S->tick;
    st.w = WORLD_W;
    st.h = WORLD_H;

    st.lobby = S->lobby;

    
    uint8_t pc = 0, ac = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        if (pl->active) pc++;
        if (pl->active && pl->alive) ac++;
    }
    st.player_count = pc;
    st.alive_count = ac;
    st.round_over = (S->lobby == 0 && ac == 0) ? 1 : 0;

    st.fruit_count = S->fruit_count;
    for (uint8_t i = 0; i < st.fruit_count; i++) st.fruits[i] = S->fruits[i];

    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        player_state_t *ps = &st.players[i];

        ps->id = pl->id;
        ps->active = pl->active;
        ps->alive = pl->alive;
        ps->score = pl->score;
        ps->len = pl->snake_len;

        for (uint16_t k = 0; k < pl->snake_len && k < MAX_SNAKE; k++) {
            ps->seg[k] = pl->snake[k];
        }
    }

    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        if (!pl->active) continue;
        if (send_msg(pl->fd, MSG_STATE, &st, sizeof(st)) != 0) {
            close(pl->fd);
            pl->active = 0;
            pl->alive = 0;
        }
    }
}

static void* client_recv_thread(void *arg) {
    
    struct pack { server_t *S; int idx; };
    struct pack *P = (struct pack*)arg;
    server_t *S = P->S;
    int idx = P->idx;
    free(P);

    for (;;) {
        pthread_mutex_lock(&S->mtx);
        player_t *pl = &S->players[idx];
        int fd = pl->fd;
        uint8_t active = pl->active;
        pthread_mutex_unlock(&S->mtx);

        if (!active) break;

        uint16_t type=0, len=0;
        if (recv_hdr(fd, &type, &len) != 0) break;
        if (len > 4096) break;

        uint8_t buf[4096];
        if (len > 0 && recv_all(fd, buf, len) != 0) break;

        if (type == MSG_INPUT && len == sizeof(input_t)) {
            input_t in;
            memcpy(&in, buf, sizeof(in));

            pthread_mutex_lock(&S->mtx);
            if (S->players[idx].active) {
                if (in.dir <= DIR_RIGHT) S->players[idx].last_dir = in.dir;
                if (in.action == ACT_QUIT) {
                    close(S->players[idx].fd);
                    S->players[idx].active = 0;
                    S->players[idx].alive = 0;
                }
            }
            pthread_mutex_unlock(&S->mtx);
        }
    }

    pthread_mutex_lock(&S->mtx);
    if (S->players[idx].active) {
        close(S->players[idx].fd);
        S->players[idx].active = 0;
        S->players[idx].alive = 0;
    }
    pthread_mutex_unlock(&S->mtx);
    return NULL;
}

static void* accept_thread_fn(void *arg) {
    server_t *S = (server_t*)arg;

    while (1) {
        pthread_mutex_lock(&S->mtx);
        uint8_t shut = S->shutdown;
        pthread_mutex_unlock(&S->mtx);
        if (shut) break;

        struct sockaddr_in cli;
        socklen_t clen = sizeof(cli);
        int cfd = accept(S->lfd, (struct sockaddr*)&cli, &clen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            continue;
        }

        if (wait_for_join(cfd) != 0) { close(cfd); continue; }

        pthread_mutex_lock(&S->mtx);

        
        int slot = -1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!S->players[i].active) { slot = i; break; }
        }
        if (slot == -1) {
            pthread_mutex_unlock(&S->mtx);
            close(cfd);
            continue;
        }

        uint32_t pid = S->next_id++;
        S->players[slot].fd = cfd;
        S->players[slot].id = pid;
        S->players[slot].active = 1;
        S->players[slot].alive = 0;     
        S->players[slot].last_dir = DIR_RIGHT;

        join_ok_t ok = {.player_id = pid};
        (void)send_msg(cfd, MSG_JOIN_OK, &ok, sizeof(ok));

       
        struct pack { server_t *S; int idx; };
        struct pack *P = malloc(sizeof(*P));
        if (P) {
            P->S = S; P->idx = slot;
            pthread_create(&S->players[slot].th, NULL, client_recv_thread, P);
        }

        printf("[server] player joined id=%u slot=%d (players max 4)\n", pid, slot);

        pthread_mutex_unlock(&S->mtx);
    }
    return NULL;
}

static void* cmd_thread_fn(void *arg) {
    server_t *S = (server_t*)arg;
    printf("[server] lobby commands: g=start, r=restart (after all dead), q=quit\n");

    while (1) {
        int c = getchar();
        if (c == EOF) continue;

        pthread_mutex_lock(&S->mtx);
        if (c == 'g' || c == 'G') S->start_req = 1;
        if (c == 'r' || c == 'R') S->restart_req = 1;
        if (c == 'q' || c == 'Q') S->shutdown = 1;
        uint8_t shut = S->shutdown;
        pthread_mutex_unlock(&S->mtx);

        if (shut) break;
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    srand((unsigned)time(NULL));

    uint16_t port = (uint16_t)atoi(argv[1]);
    server_t S;
    memset(&S, 0, sizeof(S));
    pthread_mutex_init(&S.mtx, NULL);

    S.next_id = 1;
    S.lobby = 1;
    S.start_req = 0;
    S.restart_req = 0;

    S.lfd = listen_on(port);
    if (S.lfd < 0) return 1;

    printf("[server] listening on %u\n", port);
    printf("[server] waiting for 1-4 players...\n");

    pthread_t acc_th, cmd_th;
    pthread_create(&acc_th, NULL, accept_thread_fn, &S);
    pthread_create(&cmd_th, NULL, cmd_thread_fn, &S);

    const uint32_t TICK_MS = 100;
    uint64_t next_tick = now_ms();

    while (1) {
        pthread_mutex_lock(&S.mtx);
        uint8_t shut = S.shutdown;
        uint8_t lobby = S.lobby;
        uint8_t start_req = S.start_req;
        uint8_t restart_req = S.restart_req;

        
        int active_cnt = 0, alive_cnt = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (S.players[i].active) active_cnt++;
            if (S.players[i].active && S.players[i].alive) alive_cnt++;
        }

        
        if (lobby && start_req && active_cnt >= 1) {
            S.lobby = 0;
            S.start_req = 0;
            init_round(&S);
            printf("[server] GAME START with %d players\n", active_cnt);
        } else if (lobby) {
            
            S.start_req = 0; 
        }

       
        if (!S.lobby && alive_cnt == 0) {
            
            if (restart_req && active_cnt >= 1) {
                S.restart_req = 0;
                init_round(&S);
                printf("[server] ROUND RESTART\n");
            }
        } else {
            
            S.restart_req = 0;
        }

        pthread_mutex_unlock(&S.mtx);
        if (shut) break;

        uint64_t now = now_ms();
        if (now < next_tick) {
            sleep_ms((uint32_t)(next_tick - now));
            continue;
        }
        next_tick += TICK_MS;

        pthread_mutex_lock(&S.mtx);
        S.tick++;

       
        int ac = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (S.players[i].active && S.players[i].alive) ac++;
        }

        if (!S.lobby && ac > 0) {
            
            for (int i = 0; i < MAX_PLAYERS; i++) {
                move_one(&S, &S.players[i]);
            }
        }

        broadcast_state(&S);
        pthread_mutex_unlock(&S.mtx);
    }

    printf("[server] shutting down...\n");
    pthread_mutex_lock(&S.mtx);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (S.players[i].active) close(S.players[i].fd);
        S.players[i].active = 0;
        S.players[i].alive = 0;
    }
    pthread_mutex_unlock(&S.mtx);

    close(S.lfd);

    pthread_join(cmd_th, NULL);
    pthread_join(acc_th, NULL);

    pthread_mutex_destroy(&S.mtx);
    return 0;
}

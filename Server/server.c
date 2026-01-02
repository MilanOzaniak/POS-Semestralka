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
    uint8_t shutdown;
    uint32_t tick;
    uint8_t fruit_count;
    pos_t fruits[MAX_FRUITS];
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
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (listen(fd, 16) < 0) return -1;
    return fd;
}

static int wait_for_join(int cfd) {
    for (;;) {
        uint16_t type, len;
        if (recv_hdr(cfd, &type, &len) != 0) return -1;
        uint8_t buf[4096];
        if (len > 0 && recv_all(cfd, buf, len) != 0) return -1;
        if (type == MSG_JOIN) return 0;
    }
}

static int pos_eq(pos_t a, pos_t b) {
    return a.x == b.x && a.y == b.y;
}

static int is_opposite(uint8_t a, uint8_t b) {
    return (a == DIR_UP    && b == DIR_DOWN) ||
           (a == DIR_DOWN  && b == DIR_UP) ||
           (a == DIR_LEFT  && b == DIR_RIGHT) ||
           (a == DIR_RIGHT && b == DIR_LEFT);
}


static int collides(server_t *S, player_t *me, pos_t nh, int grow) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        player_t *pl = &S->players[i];
        if (!pl->active || !pl->alive) continue;
        for (uint16_t k = 0; k < pl->snake_len; k++) {
            if (pl == me && !grow && k == pl->snake_len - 1) continue;
            if (pos_eq(pl->snake[k], nh)) return 1;
        }
    }
    return 0;
}

static void spawn_fruits(server_t *S) {
    S->fruit_count = MAX_FRUITS;
    for (uint8_t i = 0; i < S->fruit_count; i++) {
        for (;;) {
            pos_t p = {rand() % WORLD_W, rand() % WORLD_H};
            int ok = 1;
            for (int j = 0; j < MAX_PLAYERS; j++) {
                player_t *pl = &S->players[j];
                if (!pl->active || !pl->alive) continue;
                for (uint16_t k = 0; k < pl->snake_len; k++) {
                    if (pos_eq(pl->snake[k], p)) ok = 0;
                }
            }
            if (ok) { S->fruits[i] = p; break; }
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
        uint8_t bx = 5, by = 5;
        if (i == 1) bx = WORLD_W - 6;
        if (i == 2) by = WORLD_H - 6;
        if (i == 3) { bx = WORLD_W - 6; by = WORLD_H - 6; }
        for (int k = 0; k < 4; k++) {
            pl->snake[k].x = bx - k;
            pl->snake[k].y = by;
        }
    }
    spawn_fruits(S);
}

static void move_one(server_t *S, player_t *pl) {
    pos_t nh = pl->snake[0];
    if (pl->last_dir == DIR_UP) nh.y--;
    if (pl->last_dir == DIR_DOWN) nh.y++;
    if (pl->last_dir == DIR_LEFT) nh.x--;
    if (pl->last_dir == DIR_RIGHT) nh.x++;

    if (nh.x >= WORLD_W || nh.y >= WORLD_H) { pl->alive = 0; return; }

    int ate = -1;
    for (uint8_t i = 0; i < S->fruit_count; i++)
        if (pos_eq(S->fruits[i], nh)) ate = i;

    if (collides(S, pl, nh, ate >= 0)) { pl->alive = 0; return; }

    if (ate >= 0 && pl->snake_len < MAX_SNAKE) {
        for (int k = pl->snake_len; k > 0; k--) pl->snake[k] = pl->snake[k-1];
        pl->snake[0] = nh;
        pl->snake_len++;
        pl->score += 10;
        for (;;) {
          pos_t p = { (uint8_t)(rand() % WORLD_W), (uint8_t)(rand() % WORLD_H) };

          if (p.x >= WORLD_W || p.y >= WORLD_H) continue;

          if (!collides(S, pl, p, 1)) {
            int clash = 0;
            for (uint8_t j = 0; j < S->fruit_count; j++) {
              if ((int)j == ate) continue;
              if (pos_eq(S->fruits[j], p)) { clash = 1; break; }
            }
          if (!clash) {
            S->fruits[ate] = p;
            break;
          }
        }
      }    
    } else {
        for (int k = pl->snake_len - 1; k > 0; k--) pl->snake[k] = pl->snake[k-1];
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
      if (S->players[i].active) pc++;
      if (S->players[i].active && S->players[i].alive) ac++;
  }
  st.player_count = pc;
  st.alive_count = ac;
  st.round_over = (!S->lobby && ac == 0) ? 1 : 0;

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
    if (S->players[i].active){
      send_msg(S->players[i].fd, MSG_STATE, &st, sizeof(st));
    }
  }
}

static void* client_thread(void *arg) {
    struct { server_t *S; int i; } *P = arg;
    server_t *S = P->S;
    int i = P->i;
    free(P);

    for (;;) {
        uint16_t type, len;
        if (recv_hdr(S->players[i].fd, &type, &len) != 0) break;

        uint8_t buf[64];
        if (len > sizeof(buf)) break;
        if (len > 0 && recv_all(S->players[i].fd, buf, len) != 0) break;

        if (type == MSG_INPUT) {
            if (len != sizeof(input_t)) continue;

            input_t in;
            memcpy(&in, buf, sizeof(in));

            pthread_mutex_lock(&S->mtx);

            if (S->players[i].active) {
                if (in.dir <= DIR_RIGHT) {
                  uint8_t cur = S->players[i].last_dir;

                  if (S->players[i].snake_len <= 1 || !is_opposite(cur, in.dir)) {
                  S->players[i].last_dir = in.dir;
                  }
                }
                if (in.action == ACT_START)   S->start_req = 1;
                if (in.action == ACT_RESTART) S->restart_req = 1;

                if (in.action == ACT_QUIT) {
                    pthread_mutex_unlock(&S->mtx);
                    break;
                }
            }

            pthread_mutex_unlock(&S->mtx);
        }  
    }

    pthread_mutex_lock(&S->mtx);

    if (S->players[i].active) {
        close(S->players[i].fd);
        S->players[i].active = 0;
        S->players[i].alive = 0;
    }
    pthread_mutex_unlock(&S->mtx);

    return NULL;
}

static void* accept_thread(void *arg) {
    server_t *S = arg;
    while (!S->shutdown) {
        int cfd = accept(S->lfd, NULL, NULL);
        if (cfd < 0) continue;
        if (wait_for_join(cfd) != 0) { close(cfd); continue; }

        pthread_mutex_lock(&S->mtx);
        int slot = -1;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (!S->players[i].active) { slot = i; break; }
        if (slot >= 0) {
            S->players[slot].fd = cfd;
            S->players[slot].id = S->next_id++;
            S->players[slot].active = 1;
            S->players[slot].alive = 0;
            join_ok_t ok = { S->players[slot].id };
            send_msg(cfd, MSG_JOIN_OK, &ok, sizeof(ok));
            struct { server_t *S; int i; } *P = malloc(sizeof(*P));
            P->S = S; P->i = slot;
            pthread_create(&S->players[slot].th, NULL, client_thread, P);
            pthread_detach(S->players[slot].th);
        } else close(cfd);
        pthread_mutex_unlock(&S->mtx);
    }
    return NULL;
}

static void* cmd_thread(void *arg) {
    server_t *S = arg;
    for (;;) {
        int c = getchar();
        if (c == 'g') S->start_req = 1;
        if (c == 'r') S->restart_req = 1;
        if (c == 'q') { S->shutdown = 1; close(S->lfd); break; }
    }
    return NULL;
}

int main(int argc, char **argv) {

    (void)argc;
    server_t S;
    memset(&S, 0, sizeof(S));
    pthread_mutex_init(&S.mtx, NULL);
    srand(time(NULL));

    S.lfd = listen_on(atoi(argv[1]));
    S.lobby = 1;

    pthread_t acc, cmd;
    pthread_create(&acc, NULL, accept_thread, &S);
    pthread_create(&cmd, NULL, cmd_thread, &S);

    uint64_t next = now_ms();
    while (!S.shutdown) {
        if (now_ms() >= next) {
            next += 100;
            S.tick++;
            int alive = 0, active_cnt = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
              if (S.players[i].active) active_cnt++;
              if (S.players[i].active && S.players[i].alive) alive++;
            }

        if (S.lobby && S.start_req && active_cnt > 0) {
          S.lobby = 0;
          S.start_req = 0;
          init_round(&S);
        }

        if (!S.lobby && alive == 0) {
          S.lobby = 1;
          S.start_req = 0;
          S.restart_req = 0;
        }

        if (!S.lobby) {
          for (int i = 0; i < MAX_PLAYERS; i++){
            if (S.players[i].alive) move_one(&S, &S.players[i]);
          }
        }

        broadcast_state(&S);
        } else sleep_ms(5);
    }

    pthread_join(acc, NULL);
    pthread_join(cmd, NULL);
    close(S.lfd);
    return 0;
}

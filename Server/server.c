#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include "../shared/ipc.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// sleep
static void sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

// vrati 1 ak su dva smery opacne
// kvoli ked ide hrac doprava tak nemoze stlacit sipku dolava
static int is_opposite(uint8_t a, uint8_t b) {
    return (a == DIR_UP    && b == DIR_DOWN) ||
           (a == DIR_DOWN  && b == DIR_UP) ||
           (a == DIR_LEFT  && b == DIR_RIGHT) ||
           (a == DIR_RIGHT && b == DIR_LEFT);
}

// vytvori listening socket na danom porte
int server_listen(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int yes = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { 
      close(fd); 
      return -1; 
    }

    if (listen(fd, 16) < 0) { 
      close(fd); 
      return -1; 
    }
    return fd;
}

// ak dostaneme spravu ktora nas nezaujima tak zahadzujeme, aby sa stream nerozhodil
static void drain_payload(int fd, uint16_t len) {
    uint8_t buf[512];
    while (len > 0) {
        uint16_t chunk = (len > sizeof(buf)) ? (uint16_t)sizeof(buf) : len;
        if (recv_all(fd, buf, chunk) != 0) break;
        len = (uint16_t)(len - chunk);
    }
}

// cakame na handshake, po join cakame od klienta MSG_JOIN
static int wait_for_join(int cfd) {
    for (;;) {
        uint16_t type = 0, len = 0;
        if (recv_hdr(cfd, &type, &len) != 0) return -1;

        if (type == MSG_JOIN) {
            if (len > 0) drain_payload(cfd, len);
            return 0;
        }

        if (len > 0) drain_payload(cfd, len);
    }
}

// posle state_t vsetkym hracom 
static void broadcast_state_locked(server_t *S) {
    state_t st;
    game_make_state(&S->G, S->players, &st);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!S->players[i].active) continue;
        if (send_msg(S->players[i].fd, MSG_STATE, &st, (uint16_t)sizeof(st)) != 0) {
            close(S->players[i].fd);
            S->players[i].active = 0;
            S->players[i].alive = 0;
        }
    }
}

// thread pre jedneho klienta
// cita spravy (input, action)
// aktualizuje stav servera
static void* client_thread(void *arg) {
    struct { server_t *S; int i; } *P = (void*)arg;
    server_t *S = P->S;
    int i = P->i;
    free(P);

    for (;;) {
        uint16_t type = 0, len = 0;
        if (recv_hdr(S->players[i].fd, &type, &len) != 0) break;

        if (type == MSG_INPUT) {
            if (len != sizeof(input_t)) { if (len) drain_payload(S->players[i].fd, len); continue; }
            input_t in;
            if (recv_all(S->players[i].fd, &in, (uint16_t)sizeof(in)) != 0) break;

            // kriticka oblast
            pthread_mutex_lock(&S->mtx);

             if (S->players[i].active) {
                // spracovavanie smeru
                if (in.dir <= DIR_RIGHT) {
                    uint8_t cur = S->players[i].last_dir;
                    if (S->players[i].snake_len <= 1 || !is_opposite(cur, in.dir)) {
                        S->players[i].last_dir = in.dir;
                    }
                }
                // spracovavanie actions
                if (in.action == ACT_START) {
                  S->G.start_req = 1;
                }
          
                if (in.action == ACT_RESTART) {
                  S->G.restart_req = 1; 
                }

                if (in.action == ACT_QUIT) {
                    pthread_mutex_unlock(&S->mtx);
                    break;
                }
            }

            pthread_mutex_unlock(&S->mtx);
          // create posiela iba host
        } else if (type == MSG_CREATE) {
            if (len != sizeof(create_game_t)) {
              if (len){
                drain_payload(S->players[i].fd, len); 
                continue; 
              }

              create_game_t cfg;

              if (recv_all(S->players[i].fd, &cfg, (uint16_t)sizeof(cfg)) != 0){
                break;
              } 
              
              // kriticka oblast
              // zmena konfiguracie
              pthread_mutex_lock(&S->mtx);
              if (S->players[i].active && S->players[i].id == S->host_id) {
                  game_apply_config(&S->G, &cfg);
              }
          
              pthread_mutex_unlock(&S->mtx);
            } else {
                if (len){
                  drain_payload(S->players[i].fd, len);
                }
            }
          }


      // kriticka oblast
      // odpojenie klienta
      pthread_mutex_lock(&S->mtx);
      if (S->players[i].active) {
          close(S->players[i].fd); // zavrie fd
          S->players[i].active = 0;
          S->players[i].alive = 0;
          
         // ak bol host vyberieme noveho hosta 
          if (S->host_id == S->players[i].id) {
              S->host_id = 0;
              for (int k = 0; k < MAX_PLAYERS; k++) {
                  if (S->players[k].active) {
                    S->host_id = S->players[k].id;
                    break;
                  }
              }
          }
      }
      pthread_mutex_unlock(&S->mtx);
    }
      return NULL;
  
}

// bezi paralelne so server loopom
// akceptuje novych klientov, caka na MSG_JOIN
static void* accept_thread(void *arg) {
    server_t *S = (server_t*)arg;

    while (1) {
      pthread_mutex_lock(&S->mtx);
      uint8_t shut = S->shutdown;
      int lfd = S->lfd;
      pthread_mutex_unlock(&S->mtx);
      // ak je shut = 1 vypiname
      if (shut) break;
      
      // cakanie na noveho klienta
      int cfd = accept(lfd, NULL, NULL);
      if (cfd < 0) {
          if (errno == EINTR) continue;
          continue;
      }

      if (wait_for_join(cfd) != 0) { 
        close(cfd); 
        continue; 
      }

      pthread_mutex_lock(&S->mtx);

      // pozrie ci je volny slot
      int slot = -1;
      for (int i = 0; i < MAX_PLAYERS; i++) {
          if (!S->players[i].active) { 
            slot = i; 
            break; 
          }
      }
      
      // server je plny
      if (slot < 0) {
          pthread_mutex_unlock(&S->mtx);
          close(cfd);
          continue;
      }
      
      // ak je volne miesto inicializuje hraca
      S->players[slot].fd = cfd;
      S->players[slot].id = S->next_id++;
      S->players[slot].active = 1;
      S->players[slot].alive = 0;
      S->players[slot].last_dir = DIR_RIGHT;
      S->players[slot].score = 0;
      S->players[slot].snake_len = 0;
      
      // prvy hrac je host
      uint8_t is_host = 0;
      if (S->host_id == 0) {
          S->host_id = S->players[slot].id;
          is_host = 1;
      }
      
      // odosle join_ok msg
      join_ok_t ok;
      ok.player_id = S->players[slot].id;
      ok.is_host = is_host;
      (void)send_msg(cfd, MSG_JOIN_OK, &ok, (uint16_t)sizeof(ok));


      // spustime thread ktory bude obsluhovať tohto klienta
      struct { server_t *S; int i; } *P = malloc(sizeof(*P));
      if (P) {
          P->S = S;
          P->i = slot;
          if (pthread_create(&S->players[slot].th, NULL, client_thread, P) == 0) {
              pthread_detach(S->players[slot].th);
          } else {
              free(P);
              close(cfd);
              S->players[slot].active = 0;
              S->players[slot].alive = 0;
          }
      }

      pthread_mutex_unlock(&S->mtx);
    }
  
  return NULL;
}

// inicializacia serveru 
// vytvori listening socket
// inicializuje hru
void server_init(server_t *S, uint16_t port, uint8_t w, uint8_t h) {
    (void)w; (void)h;
    memset(S, 0, sizeof(*S));
    pthread_mutex_init(&S->mtx, NULL);

    S->next_id = 1;
    S->lfd = server_listen(port);

    game_init(&S->G);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        S->players[i].active = 0;
        S->players[i].alive = 0;
        S->players[i].fd = -1;
    }
}

// hlavny server loop
// spusti accept thread
// 100 ms jeden tick
void server_run(server_t *S) {
    pthread_t acc;
    pthread_create(&acc, NULL, accept_thread, S);

    uint64_t next_tick = now_ms();

    while (1) {
        //kriticka oblast
        pthread_mutex_lock(&S->mtx);

        uint8_t shut = S->shutdown;

        // spocitanie zivych a aktivnych hracov
        int active_cnt = 0, alive_cnt = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (S->players[i].active) active_cnt++;
            if (S->players[i].active && S->players[i].alive) alive_cnt++;
        }

        // prechod z lobby do game
        if (S->G.lobby && S->G.start_req && active_cnt > 0) {
            S->G.lobby = 0;
            S->G.start_req = 0;
            game_init_round(&S->G, S->players);
        } else {
            S->G.start_req = 0;
        }
        
        // posunieme hru o jeden tick
        uint64_t now = now_ms();
        if (now >= next_tick) {
            next_tick += 100;
            S->G.tick++;

            if (!S->G.lobby) {
                game_move_all(&S->G, S->players);
            }
            // broadcastneme novy stav vsetkym hracom
            broadcast_state_locked(S);
        }
        
        // znovu spocitame kolko je zivych
        alive_cnt = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (S->players[i].active && S->players[i].alive) alive_cnt++;
        }


        // ak su vsetci mrtvy tak prechadzame do lobby
        if (!S->G.lobby && alive_cnt == 0) {
            S->G.lobby = 1;
            S->G.start_req = 0;
            S->G.restart_req = 0;
        }

        pthread_mutex_unlock(&S->mtx);

        if (shut) break;

        sleep_ms(5);
    }

    pthread_join(acc, NULL);
}


#include "../shared/messages.h"
#include "../shared/ipc.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int fd;

    pthread_mutex_t mtx;
    state_t st;
    uint8_t has_state;
    uint8_t disconnected;

    uint32_t my_id;
} client_t;

static void sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static int connect_to(const char *ip, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int do_join(int fd, uint32_t *out_id) {
    if (send_msg(fd, MSG_JOIN, NULL, 0) != 0) return -1;

    uint16_t type=0, len=0;
    if (recv_hdr(fd, &type, &len) != 0) return -1;
    if (type != MSG_JOIN_OK || len != sizeof(join_ok_t)) return -1;

    join_ok_t ok;
    if (recv_all(fd, &ok, sizeof(ok)) != 0) return -1;
    *out_id = ok.player_id;
    return 0;
}

static void* recv_thread_fn(void *arg) {
    client_t *C = (client_t*)arg;

    for (;;) {
        uint16_t type=0, len=0;
        if (recv_hdr(C->fd, &type, &len) != 0) break;

        if (type == MSG_STATE) {
            if (len != sizeof(state_t)) break;
            state_t tmp;
            if (recv_all(C->fd, &tmp, sizeof(tmp)) != 0) break;

            pthread_mutex_lock(&C->mtx);
            C->st = tmp;
            C->has_state = 1;
            pthread_mutex_unlock(&C->mtx);
        } else {
            uint8_t buf[4096];
            while (len > 0) {
                uint16_t chunk = (len > sizeof(buf)) ? sizeof(buf) : len;
                if (recv_all(C->fd, buf, chunk) != 0) { len = 0; break; }
                len -= chunk;
            }
        }
    }

    pthread_mutex_lock(&C->mtx);
    C->disconnected = 1;
    pthread_mutex_unlock(&C->mtx);
    return NULL;
}

static void send_dir(client_t *C, uint8_t dir) {
    input_t in;
    in.dir = dir;
    in.action = ACT_NONE;
    (void)send_msg(C->fd, MSG_INPUT, &in, sizeof(in));
}

static void send_quit(client_t *C) {
    input_t in;
    in.dir = 255;
    in.action = ACT_QUIT;
    (void)send_msg(C->fd, MSG_INPUT, &in, sizeof(in));
}

static char snake_head_char(int idx, int is_me) {
    if (is_me) return '@';
    return (char)('A' + idx); }

static void draw(const state_t *st, uint32_t my_id) {
    erase();

    mvprintw(0, 0, "Players: %u alive: %u  tick:%u  %s%s",
             st->player_count, st->alive_count, st->tick,
             st->lobby ? "LOBBY" : "GAME",
             st->round_over ? " (ROUND OVER)" : "");

    int ox = 1, oy = 2;
    for (int x = 0; x < st->w + 2; x++) {
        mvaddch(oy - 1, ox + x - 1, '#');
        mvaddch(oy + st->h, ox + x - 1, '#');
    }
    for (int y = 0; y < st->h; y++) {
        mvaddch(oy + y, ox - 1, '#');
        mvaddch(oy + y, ox + st->w, '#');
    }

    if (st->lobby) {
        mvprintw(oy + st->h + 1, 0, "Waiting in lobby. On SERVER press 'g' to start");
        mvprintw(oy + st->h + 2, 0, "Controls: arrows/WASD move, q -> quit");
    } else if (st->round_over) {
        mvprintw(oy + st->h + 1, 0, "All snakes dead. Press 'r' to restart round.");
        mvprintw(oy + st->h + 2, 0, "Controls: q -> quit, r -> restart");
    } else {
        mvprintw(oy + st->h + 1, 0, "Controls: arrows/WASD move | q -> quit");
    }

    for (uint8_t i = 0; i < st->fruit_count; i++) {
        mvaddch(oy + st->fruits[i].y, ox + st->fruits[i].x, 'o');
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const player_state_t *p = &st->players[i];
        if (!p->active) continue;

        int is_me = (p->id == my_id);

        mvprintw(1, 0 + i*18, " P%d id=%u %s sc=%u ", i+1, p->id, p->alive ? "ALIVE" : "DEAD ", p->score);

        if (!p->alive) continue;

        for (uint16_t k = 0; k < p->len && k < MAX_SNAKE; k++) {
            char ch = (k == 0) ? snake_head_char(i, is_me) : 'x';
            mvaddch(oy + p->seg[k].y, ox + p->seg[k].x, ch);
        }
    }

    refresh();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    int fd = connect_to(argv[1], (uint16_t)atoi(argv[2]));
    if (fd < 0) {
        fprintf(stderr, "connect failed\n");
        return 1;
    }

    client_t C;
    memset(&C, 0, sizeof(C));
    C.fd = fd;
    pthread_mutex_init(&C.mtx, NULL);

    if (do_join(fd, &C.my_id) != 0) {
        fprintf(stderr, "JOIN failed\n");
        close(fd);
        return 1;
    }

    pthread_t th;
    pthread_create(&th, NULL, recv_thread_fn, &C);

    // ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    int running = 1;
    while (running) {
        pthread_mutex_lock(&C.mtx);
        uint8_t disc = C.disconnected;
        uint8_t has = C.has_state;
        state_t st = C.st;
        pthread_mutex_unlock(&C.mtx);

        if (disc) {
            erase();
            mvprintw(0, 0, "Disconnected from server.");
            refresh();
            sleep_ms(700);
            break;
        }

        int ch = getch();
        if (ch != ERR) {
            switch (ch) {
                case KEY_UP:    send_dir(&C, DIR_UP); break;
                case KEY_DOWN:  send_dir(&C, DIR_DOWN); break;
                case KEY_LEFT:  send_dir(&C, DIR_LEFT); break;
                case KEY_RIGHT: send_dir(&C, DIR_RIGHT); break;
                case 'w': case 'W': send_dir(&C, DIR_UP); break;
                case 's': case 'S': send_dir(&C, DIR_DOWN); break;
                case 'a': case 'A': send_dir(&C, DIR_LEFT); break;
                case 'd': case 'D': send_dir(&C, DIR_RIGHT); break;
                case 'q': case 'Q': send_quit(&C); running = 0; break;
                default: break;
            }
        }

        if (has) draw(&st, C.my_id);
        else {
            erase();
            mvprintw(0, 0, "Waiting for server state...");
            refresh();
        }

        sleep_ms(33);
    }

    endwin();

    pthread_join(th, NULL);
    pthread_mutex_destroy(&C.mtx);
    close(fd);
    return 0;
}

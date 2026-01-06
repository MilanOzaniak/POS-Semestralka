#define _POSIX_C_SOURCE 200809L
#include "client.h"
#include "draw.h"
#include "menu.h"
#include "../shared/ipc.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    client_t C;
    if (client_connect(&C, ip, port) != 0) {
        endwin();
        return 1;
    }

    if (client_start_recv(&C) != 0) {
        endwin();
        return 1;
    }

    uint8_t is_host = C.is_host;

    if (is_host) {
        menu_result_t res;
        if (!menu_run(&res, 1)) {
            client_send_action(&C, ACT_QUIT);
            client_close(&C);
            endwin();
            return 0;
        }
        send_msg(C.fd, MSG_CREATE, &res.cfg, sizeof(res.cfg));
    }

    int running = 1;

    while (running) {
        state_t st;
        uint8_t has = 0, disc = 0;

        client_get_state(&C, &st, &has, &disc);
        if (disc) break;

        int ch = getch();
        if (ch != ERR) {
            switch (ch) {
                case KEY_UP:    client_send_dir(&C, DIR_UP);    break;
                case KEY_DOWN:  client_send_dir(&C, DIR_DOWN);  break;
                case KEY_LEFT:  client_send_dir(&C, DIR_LEFT);  break;
                case KEY_RIGHT: client_send_dir(&C, DIR_RIGHT); break;

                case 'w': case 'W': client_send_dir(&C, DIR_UP);    break;
                case 's': case 'S': client_send_dir(&C, DIR_DOWN);  break;
                case 'a': case 'A': client_send_dir(&C, DIR_LEFT);  break;
                case 'd': case 'D': client_send_dir(&C, DIR_RIGHT); break;

                case 'g': case 'G': client_send_action(&C, ACT_START);   break;
                case 'r': case 'R': client_send_action(&C, ACT_RESTART); break;

                case 'q': case 'Q':
                    client_send_action(&C, ACT_QUIT);
                    running = 0;
                    break;
                default:
                    break;
            }
        }

        if (has) draw(&st, C.my_id);
        sleep_ms(16);
    }

    endwin();
    client_close(&C);
    return 0;
}



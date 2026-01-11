#define _POSIX_C_SOURCE 200809L
#include "client.h"
#include "draw.h"
#include "menu.h"
#include "../shared/ipc.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


// cca 60 fps
static void sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

// spusti server, ked klient bezi ako host
static int start_server(uint16_t port) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%u", port);

        execl("./server", "./server", port_str, NULL);
        _exit(1);
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip|host> <port>\n", argv[0]);
        return 1;
    }

    const char *arg = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    const char *ip = arg;

    // ak je host
    if (strcmp(arg, "host") == 0) {
        if (start_server(port) != 0) {
            fprintf(stderr, "Failed to start server\n");
            return 1;
        }
        ip = "127.0.0.1";
        sleep(1); 
    }
    
    // pripojenie na server
    client_t C;
    if (client_connect(&C, ip, port) != 0) {
        return 1;
    }


    // spustenie recv threadu, prijimanie sprav zo servera
    if (client_start_recv(&C) != 0) {
        client_close(&C);
        return 1;
    }

    if (C.is_host) {
        menu_result_t res;
        // vypnutie 
        if (!menu_run(&res, 1)) {
            client_send_action(&C, ACT_QUIT);
            client_close(&C);
            return 0;
        }
        send_msg(C.fd, MSG_CREATE, &res.cfg, sizeof(res.cfg)); // poslanie serveru spravu s konfiguraciou hry
    }

    initscr();
    cbreak(); // citame hned klavesy, povolenie getch()
    noecho(); // nepiseme stlacene klavesy na obrazovku
    keypad(stdscr, TRUE); // mozeme citat sipky
    nodelay(stdscr, TRUE); // 
    curs_set(0); // vypneme kurzor

    int running = 1;
    
    // hlavny loop clienta
    while (running) {
        state_t st;
        uint8_t has = 0; // ked 1 tak mozeme kreslit
        uint8_t disc = 0; // ked 1 tak sme dissconnected
        
        // vytiahne posledny state
        client_get_state(&C, &st, &has, &disc);
        if (disc) break;
        
        // citanie z klavesnice getch() povolene
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
        
        // ak je platny state tak ho vykreslime
        if (has) draw(&st, C.my_id);
        sleep_ms(16);
    }
    
    // upratenie ncurses a spojenia
    endwin();
    client_close(&C);
    return 0;
}

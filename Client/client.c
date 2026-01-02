#include "../shared/messages.h"
#include "../shared/ipc.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int connect_to(const char *ip, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid IP: %s\n", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

static uint8_t map_key_to_dir(char c) {
    if (c == 'w' || c == 'W') return DIR_UP;
    if (c == 's' || c == 'S') return DIR_DOWN;
    if (c == 'a' || c == 'A') return DIR_LEFT;
    if (c == 'd' || c == 'D') return DIR_RIGHT;
    return 255;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);

    int fd = connect_to(ip, port);
    if (fd < 0) return 1;

    printf("[client] connected to %s:%u\n", ip, port);

    if (send_msg(fd, MSG_JOIN, NULL, 0) != 0) {
        printf("[client] failed to send JOIN\n");
        close(fd);
        return 1;
    }
    printf("[client] sent JOIN\n");

    uint16_t type = 0, len = 0;
    if (recv_hdr(fd, &type, &len) != 0) {
        printf("[client] disconnected before JOIN_OK\n");
        close(fd);
        return 1;
    }

    if (type != MSG_JOIN_OK || len != sizeof(join_ok_t)) {
        printf("[client] unexpected msg type=%u len=%u (expected JOIN_OK)\n", type, len);
        if (len > 0) {
            uint8_t tmp[4096];
            if (len <= sizeof(tmp)) (void)recv_all(fd, tmp, len);
        }
        close(fd);
        return 1;
    }

    join_ok_t ok;
    if (recv_all(fd, &ok, sizeof(ok)) != 0) {
        printf("[client] failed to read JOIN_OK payload\n");
        close(fd);
        return 1;
    }

    printf("[client] got JOIN_OK player_id=%u\n", ok.player_id);
    printf("Ovládanie: w/a/s/d + Enter, q + Enter = koniec\n");

    for (;;) {
        struct pollfd pfds[2];
        pfds[0].fd = fd;
        pfds[0].events = POLLIN;

        pfds[1].fd = STDIN_FILENO;
        pfds[1].events = POLLIN;

        int pr = poll(pfds, 2, -1);
        if (pr < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        if (pfds[0].revents & POLLIN) {
            uint16_t mtype = 0, mlen = 0;
            if (recv_hdr(fd, &mtype, &mlen) != 0) {
                printf("[client] server disconnected\n");
                break;
            }

            if (mlen > 4096) {
                printf("[client] payload too large (%u), closing\n", mlen);
                break;
            }

            uint8_t payload[4096];
            if (mlen > 0) {
                if (recv_all(fd, payload, mlen) != 0) {
                    printf("[client] server disconnected (payload)\n");
                    break;
                }
            }

            if (mtype == MSG_STATE) {
                if (mlen != sizeof(state_t)) {
                    printf("[client] bad STATE len=%u\n", mlen);
                    continue;
                }
                state_t st;
                memcpy(&st, payload, sizeof(st));
                printf("[client] tick=%u\n", st.tick);
            } else if (mtype == MSG_PONG) {
            } else {
                printf("[client] unknown msg type=%u len=%u\n", mtype, mlen);
            }
        }

        if (pfds[1].revents & POLLIN) {
            char line[64];
            if (!fgets(line, sizeof(line), stdin)) {
                printf("[client] stdin closed\n");
                break;
            }

            char c = line[0];
            if (c == 'q' || c == 'Q') {
                printf("[client] quitting\n");
                break;
            }

            uint8_t d = map_key_to_dir(c);
            if (d == 255) {
                continue;
            }

            input_t in;
            in.dir = d;

            if (send_msg(fd, MSG_INPUT, &in, sizeof(in)) != 0) {
                printf("[client] failed to send INPUT\n");
                break;
            }
            printf("[client] sent INPUT (%c)\n", c);
        }
    }

    close(fd);
    return 0;
}


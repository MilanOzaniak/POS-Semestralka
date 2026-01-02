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
#include <time.h>
#include <unistd.h>

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static const char *dir_str(uint8_t d) {
    switch (d) {
        case DIR_UP: return "UP";
        case DIR_DOWN: return "DOWN";
        case DIR_LEFT: return "LEFT";
        case DIR_RIGHT: return "RIGHT";
        default: return "UNKNOWN";
    }
}

static int listen_on(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
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

static int accept_one(int lfd) {
    struct sockaddr_in cli;
    socklen_t cli_len = sizeof(cli);
    int cfd = accept(lfd, (struct sockaddr*)&cli, &cli_len);
    if (cfd < 0) {
        perror("accept");
        return -1;
    }

    char ip[64];
    inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
    printf("[server] client connected from %s:%u\n", ip, ntohs(cli.sin_port));

    return cfd;
}

static int wait_for_join(int cfd, uint32_t *out_player_id) {
    for (;;) {
        uint16_t type = 0, len = 0;
        if (recv_hdr(cfd, &type, &len) != 0) {
            printf("[server] client disconnected (before JOIN)\n");
            return -1;
        }

        if (len > 4096) {
            printf("[server] payload too large (%u), closing\n", len);
            return -1;
        }

        uint8_t payload[4096];
        if (len > 0) {
            if (recv_all(cfd, payload, len) != 0) {
                printf("[server] client disconnected (JOIN payload)\n");
                return -1;
            }
        }

        if (type == MSG_JOIN) {
            printf("[server] got JOIN\n");
            join_ok_t ok;
            ok.player_id = 1; 
            if (send_msg(cfd, MSG_JOIN_OK, &ok, sizeof(ok)) != 0) {
                printf("[server] failed to send JOIN_OK\n");
                return -1;
            }
            printf("[server] sent JOIN_OK id=%u\n", ok.player_id);
            *out_player_id = ok.player_id;
            return 0;
        }

        printf("[server] ignoring msg type=%u before JOIN\n", type);
    }
}

static int handle_one_message(int cfd, uint8_t *last_dir) {
    uint16_t type = 0, len = 0;
    if (recv_hdr(cfd, &type, &len) != 0) {
        return -1; // disconnect
    }

    if (len > 4096) {
        printf("[server] payload too large (%u)\n", len);
        return -1;
    }

    uint8_t payload[4096];
    if (len > 0) {
        if (recv_all(cfd, payload, len) != 0) return -1;
    }

    if (type == MSG_INPUT) {
        if (len != sizeof(input_t)) {
            printf("[server] bad INPUT len=%u\n", len);
            return 0;
        }
        input_t in;
        memcpy(&in, payload, sizeof(in));
        *last_dir = in.dir;
        printf("[server] INPUT dir=%s\n", dir_str(*last_dir));
    } else if (type == MSG_PING) {
        (void)send_msg(cfd, MSG_PONG, NULL, 0);
    } else {
        printf("[server] unknown msg type=%u len=%u\n", type, len);
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    uint16_t port = (uint16_t)atoi(argv[1]);

    int lfd = listen_on(port);
    if (lfd < 0) return 1;

    printf("[server] listening on port %u\n", port);

    int cfd = accept_one(lfd);
    if (cfd < 0) {
        close(lfd);
        return 1;
    }

    uint32_t player_id = 0;
    if (wait_for_join(cfd, &player_id) != 0) {
        close(cfd);
        close(lfd);
        return 1;
    }

    uint32_t tick = 0;
    uint8_t last_dir = DIR_RIGHT;

    const uint64_t TICK_MS = 100;
    uint64_t next_tick = now_ms() + TICK_MS;

    printf("[server] entering game loop (tick=%ums)\n", (unsigned)TICK_MS);

    for (;;) {
        uint64_t now = now_ms();
        int timeout_ms = 0;
        if (next_tick > now) {
            uint64_t diff = next_tick - now;
            timeout_ms = (diff > 1000ULL) ? 1000 : (int)diff;
        } else {
            timeout_ms = 0;
        }

        struct pollfd pfd;
        pfd.fd = cfd;
        pfd.events = POLLIN;

        int pr = poll(&pfd, 1, timeout_ms);
        if (pr < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        if (pr > 0 && (pfd.revents & POLLIN)) {
            if (handle_one_message(cfd, &last_dir) != 0) {
                printf("[server] client disconnected\n");
                break;
            }
        }

        now = now_ms();
        while (now >= next_tick) {
            tick++;


            state_t st;
            st.tick = tick;

            if (send_msg(cfd, MSG_STATE, &st, sizeof(st)) != 0) {
                printf("[server] failed to send STATE\n");
                goto done;
            }


            next_tick += TICK_MS;
            now = now_ms();
        }
    }

done:
    close(cfd);
    close(lfd);
    return 0;
}

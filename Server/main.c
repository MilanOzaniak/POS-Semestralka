#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    uint16_t port = (uint16_t)atoi(argv[1]);

    server_t S;
    server_init(&S, port, WORLD_W, WORLD_H);

    if (S.lfd < 0) {
        fprintf(stderr, "listen failed\n");
        return 1;
    }

    server_run(&S);
    return 0;
}

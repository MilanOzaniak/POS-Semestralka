#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

int send_all(int fd, const void *buf, uint16_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

int recv_all(int fd, void *buf, uint16_t len) {
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= n;
    }
    return 0;
}

int send_hdr(int fd, uint16_t type, uint16_t len) {
    uint16_t hdr[2];
    hdr[0] = htons(type);
    hdr[1] = htons(len);
    return send_all(fd, hdr, sizeof(hdr));
}

int recv_hdr(int fd, uint16_t *type, uint16_t *len) {
    uint16_t hdr[2];
    if (recv_all(fd, hdr, sizeof(hdr)) != 0) return -1;
    *type = ntohs(hdr[0]);
    *len  = ntohs(hdr[1]);
    return 0;
}

int send_msg(int fd, uint16_t type, const void *buf, uint16_t len) {
    if (send_hdr(fd, type, len) != 0) return -1;
    if (len > 0 && send_all(fd, buf, len) != 0) return -1;
    return 0;
}

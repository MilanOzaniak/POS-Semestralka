#include "ipc.h"
#include "messages.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

int send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

int recv_all(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t*)buf;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(fd, p + recvd, len - recvd, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1; 
        recvd += (size_t)n;
    }
    return 0;
}

int send_msg(int fd, uint16_t type, const void *payload, uint16_t len) {
    msg_hdr_t h;
    h.type = type;
    h.len  = len;

    if (send_all(fd, &h, sizeof(h)) != 0) return -1;
    if (len > 0 && payload != NULL) {
        if (send_all(fd, payload, len) != 0) return -1;
    }
    return 0;
}

int recv_hdr(int fd, uint16_t *type, uint16_t *len) {
    msg_hdr_t h;
    if (recv_all(fd, &h, sizeof(h)) != 0) return -1;
    if (type) *type = h.type;
    if (len)  *len  = h.len;
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "ipc.h"
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

// vytvorene s pomocou AI

// poslanie "len" dlzky bajtov
int send_all(int fd, const void *buf, uint16_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            // error, spojenie prerusene
            if (errno == EINTR) continue;
            return -1;
        }
        // posun pointeru o pocet odoslanych bajtov a zniženie zostavajucej dlzkly 
        p += n;
        len -= n;
    }
    return 0;
}


// prijatie "len" dlzky bajtov
int recv_all(int fd, void *buf, uint16_t len) {
    uint8_t *p = buf;
    // n== 0 spojenie zatvorene
    // n < 0 chyba 
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        // posun pointeru a znizenie zostavajucej dlzky
        p += n;
        len -= n;
    }
    return 0;
}

// poslanie hlavicky, pouziva big endian
int send_hdr(int fd, uint16_t type, uint16_t len) {
    uint16_t hdr[2];
    hdr[0] = htons(type);
    hdr[1] = htons(len);
    return send_all(fd, hdr, sizeof(hdr));
}

// precitanie hlavicky 
int recv_hdr(int fd, uint16_t *type, uint16_t *len) {
    uint16_t hdr[2];
    if (recv_all(fd, hdr, sizeof(hdr)) != 0) return -1;
    *type = ntohs(hdr[0]);
    *len  = ntohs(hdr[1]);
    return 0;
}

// pošle celu spravu hlavicku + payload 
int send_msg(int fd, uint16_t type, const void *buf, uint16_t len) {
    if (send_hdr(fd, type, len) != 0) return -1;
    if (len > 0 && send_all(fd, buf, len) != 0) return -1;
    return 0;
}

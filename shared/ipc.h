#pragma once
#include <stdint.h>

int send_all(int fd, const void *buf, uint16_t len);
int recv_all(int fd, void *buf, uint16_t len);

int send_hdr(int fd, uint16_t type, uint16_t len);
int recv_hdr(int fd, uint16_t *type, uint16_t *len);

int send_msg(int fd, uint16_t type, const void *buf, uint16_t len);


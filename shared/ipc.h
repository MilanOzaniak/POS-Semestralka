#pragma once
#include <stddef.h>
#include <stdint.h>

int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);

int send_msg(int fd, uint16_t type, const void *payload, uint16_t len);
int recv_hdr(int fd, uint16_t *type, uint16_t *len);

